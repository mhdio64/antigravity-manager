#include "installer.h"
#include "download.h"

#include <QCoreApplication>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QIcon>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QRegularExpression>
#include <QSettings>
#include <QLocale>
#include <QSaveFile>
#include <QUrl>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QVersionNumber>
#include <QTextStream>
#include <QtConcurrentRun>
#include <memory>

struct Product {
    QString name;
    QString feed;
    QString archive;
};

static const Product products[] = {
    {"Antigravity App",
     "https://antigravity-hub-auto-updater-974169037036.us-central1.run.app/releases",
     "https://storage.googleapis.com/antigravity-public/antigravity-hub/%1-%2/linux-x64/Antigravity.tar.gz"},
    {"Antigravity IDE",
     "https://antigravity-ide-auto-updater-974169037036.us-central1.run.app/releases",
     "https://edgedl.me.gvt1.com/edgedl/release2/j0qc3/antigravity/stable/%1-%2/linux-x64/Antigravity%20IDE.tar.gz"},
};

class Releases : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList results READ results NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(int progress READ progress NOTIFY changed)
    Q_PROPERTY(bool installing READ installing NOTIFY changed)
    Q_PROPERTY(QString installation READ installation NOTIFY changed)
    Q_PROPERTY(bool persian READ persian WRITE setPersian NOTIFY changed)
    Q_PROPERTY(QUrl managerUpdate READ managerUpdate NOTIFY changed)

public:
    QUrl managerUpdate() const { return managerUpdate_; }
    Q_INVOKABLE void openManagerUpdate() const
    {
        if (!managerUpdate_.isEmpty())
            QDesktopServices::openUrl(managerUpdate_);
    }

    Q_INVOKABLE void checkManagerUpdate()
    {
        QNetworkRequest request{QUrl("https://api.github.com/repos/mhdio64/antigravity-manager/releases/latest")};
        request.setTransferTimeout(10000);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        auto *reply = network_.get(request);
        connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
            if (received > 1024 * 1024)
                reply->abort();
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError)
                return;
            const auto release = QJsonDocument::fromJson(reply->readAll()).object();
            const auto tag = release.value("tag_name").toString();
            static const QRegularExpression tagPattern(R"(\Av[0-9]{1,6}\.[0-9]{1,6}\.[0-9]{1,6}\z)");
            if (!tagPattern.match(tag).hasMatch() || release.value("prerelease").toBool() ||
                QVersionNumber::fromString(tag.mid(1)) <= QVersionNumber::fromString(APP_VERSION))
                return;
            const QString variant = QString::fromLatin1(PACKAGE_VARIANT);
            const auto assetName = "antigravity-release-check_" + tag.mid(1) +
                (variant.isEmpty() ? QString{} : "_" + variant) + "_amd64.deb";
            const QUrl expected("https://github.com/mhdio64/antigravity-manager/releases/download/" + tag + "/" + assetName);
            for (const auto &asset : release.value("assets").toArray()) {
                const auto details = asset.toObject();
                if (details.value("state").toString() == "uploaded" &&
                    QUrl(details.value("browser_download_url").toString()) == expected) {
                    managerUpdate_ = expected;
                    emit changed();
                    break;
                }
            }
        });
    }

    Q_INVOKABLE QString externalInstall(int index) const
    {
        if (installed(index))
            return {};
        const QStringList candidates = index == 0
            ? QStringList{"/opt/Antigravity/antigravity", "/opt/antigravity/antigravity",
                          QDir::homePath() + "/.local/share/antigravity/antigravity",
                          QDir::homePath() + "/Downloads/Antigravity-x64/antigravity"}
            : QStringList{"/usr/share/antigravity-ide/antigravity-ide", "/opt/antigravity-ide/antigravity-ide",
                          QDir::homePath() + "/Downloads/Antigravity IDE/antigravity-ide"};
        for (const auto &path : candidates) {
            if (QFileInfo(path).isFile() && QFileInfo(path).isExecutable())
                return path;
        }
        return {};
    }
    bool persian() const { return persian_; }
    void setPersian(bool value)
    {
        if (value == persian_)
            return;
        persian_ = value;
        QSettings().setValue("interface/persian", value);
        installation_.clear();
        check();
    }

    QStringList results() const { return results_; }
    bool busy() const { return remaining_ > 0 || installing_; }
    bool failed() const { return failed_; }
    int progress() const { return progress_; }
    bool installing() const { return installing_; }
    QString installation() const { return installation_; }
    Q_INVOKABLE bool installed(int index) const { return hasInstallation(index); }
    Q_INVOKABLE QString installFolder(int index) const { return managedInstallationFolder(index); }
    Q_INVOKABLE void openInstallFolder(int index)
    {
        const auto folder = managedInstallationFolder(index);
        if (!folder.isEmpty() && !QDesktopServices::openUrl(QUrl::fromLocalFile(folder)))
            finishInstall(index, "Could not open the installation folder.");
    }
    Q_INVOKABLE void launch(int index)
    {
        if (busy() || index < 0 || index > 1)
            return;
        const auto error = launchInstallation(index);
        if (!error.isEmpty())
            finishInstall(index, error);
    }
    Q_INVOKABLE bool canRollback(int index) const { return hasPreviousInstallation(index); }
    Q_INVOKABLE QString localVersion(int index) const { return installedRelease(index); }
    Q_INVOKABLE bool rtlEnabled(int index) const { return hasRtl(index); }
    Q_INVOKABLE bool rtlPending(int index) const { return wantsRtl(index) && !hasRtl(index); }
    Q_INVOKABLE bool rtlVersionKnown(int index) const
    {
        const auto version = installedRelease(index);
        return version.isEmpty() || version == (index == 0 ? "2.31.2" : "2.7.0");
    }
    Q_INVOKABLE void toggleRtl(int index)
    {
        if (busy() || index < 0 || index > 1 || !installed(index))
            return;
        const bool enable = !hasRtl(index);
        installing_ = true;
        progress_ = -1;
        installation_ = localized("Preparing RTL…");
        runInstallation(index, enable ? "RTL enabled." : "RTL removed.", [index, enable] {
            return setInstallationRtl(index, enable);
        });
        emit changed();
    }
    Q_INVOKABLE void remove(int index)
    {
        if (busy() || index < 0 || index > 1 || !installed(index))
            return;
        installing_ = true;
        progress_ = -1;
        installation_ = products[index].name + ": " + localized("Removing…");
        runInstallation(index, "Removed. Your settings and projects were kept.", [index] {
            return removeInstallation(index);
        });
        emit changed();
    }
    Q_INVOKABLE bool canUpdate(int index) const
    {
        return available(index) && versions_[index] != installedRelease(index);
    }

    Q_INVOKABLE void rollback(int index)
    {
        if (busy() || index < 0 || index > 1 || !canRollback(index))
            return;
        installing_ = true;
        progress_ = -1;
        installation_ = products[index].name + ": " + localized("Restoring previous version. Close the product if it is open.");
        runInstallation(index, "Previous version restored.", [index] {
            return rollbackInstallation(index);
        });
        emit changed();
    }

    Q_INVOKABLE bool available(int index) const
    {
        return index >= 0 && index < 2 && !archives_[index].isEmpty();
    }

    Q_INVOKABLE void install(int index)
    {
        if (busy() || !available(index))
            return;
        const bool update = installed(index);
        if (update && !canUpdate(index))
            return;

        auto file = std::make_shared<Download>(QUrl(archives_[index]));
        if (!file->open()) {
            finishInstall(index, "Could not create a temporary download file.");
            return;
        }

        installing_ = true;
        progress_ = -1;
        installation_ = products[index].name + ": " + localized("Downloading…");
        auto *reply = network_.get(file->request());
        connect(reply, &QNetworkReply::readyRead, this, [reply, file] {
            if (!file->write(*reply))
                reply->abort();
        });
        connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
            progress_ = total > 0 ? int(received * 100 / total) : -1;
            emit changed();
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply, file, index, update] {
            const bool downloaded = file->complete(*reply);
            reply->deleteLater();
            if (!downloaded) {
                finishInstall(index, "Download failed or there was insufficient temporary storage.");
                return;
            }
            progress_ = -1;
            installation_ = products[index].name + ": " + localized(update
                ? "Preparing update. Close the product if it is open."
                : "Verifying and installing…");
            const QString version = versions_[index];
            runInstallation(index, update ? "Updated." : "Installed. Open it from your application menu.",
                            [file, index, update, version] {
                const auto result = installArchive(index, file->path(), update, version);
                file->discard();
                return result;
            });
            emit changed();
        });
        emit changed();
    }

    Q_INVOKABLE bool exportReport(const QUrl &destination)
    {
        if (!destination.isLocalFile())
            return false;
        QSaveFile file(destination.toLocalFile());
        const auto data = report_.join("\n").toUtf8();
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
    }

    Q_INVOKABLE void check()
    {
        if (busy())
            return;

        remaining_ = 2;
        failed_ = false;
        for (int index = 0; index < 2; ++index) {
            const auto &product = products[index];
            archives_[index].clear();
            results_[index] = product.name + ": " + localized("Checking…");
            QNetworkRequest request{QUrl(product.feed)};
            request.setTransferTimeout(15000);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::ManualRedirectPolicy);
            auto *reply = network_.get(request);
            connect(reply, &QNetworkReply::downloadProgress, reply,
                    [reply](qint64 received, qint64 total) {
                if (received > 1024 * 1024 || total > 1024 * 1024)
                    reply->abort();
            });
            connect(reply, &QNetworkReply::finished, this, [this, reply, index] {
                finish(reply, index);
            });
        }
        emit changed();
    }

signals:
    void changed();
    void installFinished(bool success);

private:
    QString localized(const QString &message) const;
    void log(int index, const QString &event)
    {
        report_.append(QDateTime::currentDateTimeUtc().toString(Qt::ISODate) +
                       " " + products[index].name + " " + event);
        if (report_.size() > 100)
            report_.removeFirst();
    }
    template<typename Task>
    void runInstallation(int index, const QString &success, Task task)
    {
        auto *watcher = new QFutureWatcher<QString>(this);
        connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, index, success] {
            finishInstall(index, watcher->result(), success);
            watcher->deleteLater();
        });
        watcher->setFuture(QtConcurrent::run(task));
    }

    void finishInstall(int index, const QString &error, const QString &success = {})
    {
        installing_ = false;
        installation_ = products[index].name + ": " + localized(error.isEmpty() ? success : error);
        emit changed();
        log(index, error.isEmpty() ? success : error.split(" Recovery files:").first()
            .split("some files remain in ").first());
        emit installFinished(error.isEmpty());
    }

    void finish(QNetworkReply *reply, int index)
    {
        const auto &product = products[index];
        const auto releases = QJsonDocument::fromJson(reply->readAll()).array();
        const auto entry = releases.isEmpty() ? QJsonObject{} : releases.first().toObject();
        const auto version = entry.value("version").toString();
        const auto execution = entry.value("execution_id").toString();
        static const QRegularExpression versionPattern(R"(\A[0-9]{1,6}\.[0-9]{1,6}\.[0-9]{1,6}\z)");
        static const QRegularExpression executionPattern(R"(\A[0-9]{1,24}\z)");

        if (reply->error() == QNetworkReply::NoError &&
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200 &&
            versionPattern.match(version).hasMatch() && executionPattern.match(execution).hasMatch()) {
            archives_[index] = product.archive.arg(version, execution);
            versions_[index] = version;
            log(index, "Release check OK: " + version);
            results_[index] = product.name + " " + version + "\n" + archives_[index];
        } else {
            failed_ = true;
            log(index, "Release check failed");
            results_[index] = product.name + ": " + localized("Release check failed.")
                + "\nhttps://antigravity.google/download";
        }
        reply->deleteLater();
        --remaining_;
        emit changed();
    }

    QNetworkAccessManager network_;
    QStringList results_{QString(), QString()};
    QString archives_[2];
    QString versions_[2];
    QString installation_;
    QUrl managerUpdate_;
    QStringList report_{"Antigravity Manager session report"};
    int remaining_ = 0;
    int progress_ = -1;
    bool installing_ = false;
    bool failed_ = false;
    bool persian_ = QSettings().value("interface/persian",
        QLocale::system().language() == QLocale::Persian).toBool();
};

QString Releases::localized(const QString &message) const
{
    if (!persian_)
        return message;
    static const QHash<QString, QString> translations{
        {"Could not open the installation folder.", "بازکردن پوشهٔ نصب ممکن نبود."},
        {"Could not start the product.", "اجرای برنامه ممکن نبود."},
        {"Preparing RTL…", "در حال آماده‌سازی راست‌چین…"},
        {"RTL enabled.", "راست‌چین فعال شد."},
        {"RTL removed.", "راست‌چین حذف شد."},
        {"RTL state did not change.", "وضعیت راست‌چین تغییر نکرد."},
        {"Close the product before changing RTL.", "پیش از تغییر راست‌چین، برنامه را ببندید."},
        {"RTL is not verified for this installation.", "سازگاری راست‌چین با این نصب تأیید نشده است."},
        {"RTL requires Node.js 22.12 or newer on PATH.", "راست‌چین به Node.js نسخهٔ ۲۲٫۱۲ یا جدیدتر در PATH نیاز دارد."},
        {"Could not prepare RTL staging.", "آماده‌سازی پوشهٔ موقت راست‌چین ممکن نبود."},
        {"Could not stage the installation for RTL.", "آماده‌سازی نسخهٔ موقت نصب برای راست‌چین ممکن نبود."},
        {"RTL operation failed. The active installation was preserved.", "عملیات راست‌چین ناموفق بود. نصب فعال حفظ شد."},
        {"Could not record RTL state.", "ثبت وضعیت راست‌چین ممکن نبود."},
        {"Removing…", "در حال حذف…"},
        {"Removed. Your settings and projects were kept.", "حذف شد. تنظیمات و پروژه‌ها حفظ شدند."},
        {"Restoring previous version. Close the product if it is open.", "در حال بازگردانی نسخهٔ قبلی. اگر برنامه باز است، آن را ببندید."},
        {"Previous version restored.", "نسخهٔ قبلی بازگردانده شد."},
        {"Could not create a temporary download file.", "ساخت فایل موقت دانلود ممکن نبود."},
        {"Downloading…", "در حال دانلود…"},
        {"Download failed or there was insufficient temporary storage.", "دانلود ناموفق بود یا فضای موقت کافی نبود."},
        {"Preparing update. Close the product if it is open.", "در حال آماده‌سازی به‌روزرسانی. اگر برنامه باز است، آن را ببندید."},
        {"Verifying and installing…", "در حال بررسی و نصب…"},
        {"Updated.", "به‌روزرسانی انجام شد."},
        {"Installed. Open it from your application menu.", "نصب شد. برنامه را از منوی سیستم باز کنید."},
        {"Checking…", "در حال بررسی…"},
        {"Release check failed.", "بررسی نسخه ناموفق بود."},
        {"The product is still running. Close it and retry.", "برنامه هنوز باز است. آن را ببندید و دوباره تلاش کنید."},
        {"Could not switch versions. The active installation was preserved.", "جابه‌جایی نسخه‌ها ممکن نبود. نصب فعال حفظ شد."},
        {"Another installation operation is running.", "عملیات دیگری روی نصب در حال اجراست."},
        {"No previous version is available.", "نسخهٔ قبلی موجود نیست."},
        {"No managed installation is available to remove.", "نصب مدیریت‌شده‌ای برای حذف وجود ندارد."},
        {"Close the product before removing it.", "پیش از حذف، برنامه را ببندید."},
        {"The previous version directory is invalid.", "پوشهٔ نسخهٔ قبلی نامعتبر است."},
        {"Could not prepare the removal directory.", "آماده‌سازی پوشهٔ موقت حذف ممکن نبود."},
        {"Could not move the installation.", "جابه‌جایی فایل‌های نصب ممکن نبود."},
        {"Removal failed. Recovery files: ", "حذف ناموفق بود. فایل‌های بازیابی: "},
        {"Removal failed. The installation was restored.", "حذف ناموفق بود. نصب به جای قبلی بازگشت."},
        {"The launcher was removed, but some files remain in ", "ورودی منو حذف شد. تعدادی فایل در این مسیر باقی مانده‌اند: "},
        {"Could not create the installation directories.", "ساخت پوشه‌های نصب ممکن نبود."},
        {"Another installation is running.", "نصب دیگری در حال اجراست."},
        {"No managed installation is available to update.", "نصب مدیریت‌شده‌ای برای به‌روزرسانی وجود ندارد."},
        {"A managed installation or launcher already exists.", "نصب یا ورودی منوی مدیریت‌شده از قبل وجود دارد."},
        {"This version is already installed.", "این نسخه از قبل نصب شده است."},
        {"At least 1 GiB of free disk space is required.", "حداقل یک گیبی‌بایت فضای خالی لازم است."},
        {"The archive could not be safely extracted.", "باز کردن ایمن آرشیو ممکن نبود."},
        {"The expected executable is missing.", "فایل اجرایی مورد انتظار پیدا نشد."},
        {"The product icon could not be prepared.", "آماده‌سازی آیکن برنامه ممکن نبود."},
        {"Could not record the installed version.", "ثبت نسخهٔ نصب‌شده ممکن نبود."},
        {"Updated, but the previous version could not be registered. Recovery files: ", "به‌روزرسانی انجام شد، اما ثبت نسخهٔ قبلی ممکن نبود. فایل‌های بازیابی: "},
        {"The installation could not be activated.", "فعال‌کردن نصب ممکن نبود."},
        {"Launcher registration failed and the installation could not be removed.", "ثبت ورودی منو ناموفق بود و پاک‌کردن فایل‌های نصب ممکن نبود."},
        {"The menu launcher could not be registered.", "ثبت ورودی منو ممکن نبود."},
    };
    const auto exact = translations.constFind(message);
    if (exact != translations.cend())
        return exact.value();
    for (auto entry = translations.cbegin(); entry != translations.cend(); ++entry) {
        if (entry.key().endsWith(QLatin1Char(' ')) && message.startsWith(entry.key()))
            return entry.value() + QChar(0x2066) + message.mid(entry.key().size()) + QChar(0x2069);
    }
    return message;
}

int main(int argc, char **argv)
{
    QCoreApplication::setOrganizationName("Antigravity Manager");
    QCoreApplication::setApplicationName("Antigravity Release Check");
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    if (argc == 3 && QByteArray(argv[1]) == "--install-latest") {
        QCoreApplication app(argc, argv);
        const QString product = app.arguments().at(2);
        if (product != "app" && product != "ide")
            return 2;
        const int index = product == "ide" ? 1 : 0;
        Releases releases;
        QObject::connect(&releases, &Releases::changed, &app, [&] {
            if (releases.busy())
                return;
            QObject::disconnect(&releases, &Releases::changed, &app, nullptr);
            if (releases.canUpdate(index)) {
                releases.install(index);
            } else {
                QTextStream(stderr) << (releases.available(index)
                    ? QString("This version is already installed.") : releases.results().at(index)) << Qt::endl;
                app.exit(1);
            }
        });
        QObject::connect(&releases, &Releases::installFinished, &app, [&](bool success) {
            QTextStream(success ? stdout : stderr) << releases.installation() << Qt::endl;
            app.exit(success ? 0 : 1);
        });
        releases.check();
        return app.exec();
    }
    const QByteArray operation = argc > 1 ? QByteArray(argv[1]) : QByteArray{};
    if ((argc == 4 && (operation == "--install-archive" || operation == "--update-archive")) ||
        (argc == 3 && (operation == "--rollback" || operation == "--remove" || operation == "--launch" ||
                      operation == "--apply-rtl" || operation == "--remove-rtl"))) {
        QCoreApplication app(argc, argv);
        const QString product = app.arguments().at(2);
        if (product != "app" && product != "ide")
            return 2;
        const int index = product == "ide" ? 1 : 0;
        QString error;
        if (operation == "--launch")
            error = launchInstallation(index);
        else if (operation == "--rollback")
            error = rollbackInstallation(index);
        else if (operation == "--remove")
            error = removeInstallation(index);
        else if (operation == "--apply-rtl" || operation == "--remove-rtl")
            error = setInstallationRtl(index, operation == "--apply-rtl");
        else
            error = installArchive(index, app.arguments().at(3), operation == "--update-archive");
        QTextStream(error.isEmpty() ? stdout : stderr)
            << (error.isEmpty() ? "Done." : error) << Qt::endl;
        return error.isEmpty() ? 0 : 1;
    }
    if (argc == 2 && QByteArray(argv[1]) == "--check") {
        QCoreApplication app(argc, argv);
        Releases releases;
        QObject::connect(&releases, &Releases::changed, &app, [&] {
            if (!releases.busy()) {
                QTextStream(stdout) << releases.results().join("\n") << Qt::endl;
                app.exit(releases.failed() ? 1 : 0);
            }
        });
        releases.check();
        return app.exec();
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setDesktopFileName("antigravity-release-check");
    QGuiApplication::setWindowIcon(QIcon(":/icons/antigravity-release-check.png"));
    Releases releases;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("releases", &releases);
    engine.load(QUrl("qrc:/Main.qml"));
    return engine.rootObjects().isEmpty() ? 1 : app.exec();
}

#include "main.moc"
