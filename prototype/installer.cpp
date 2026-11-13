#include "installer.h"

#include <QDir>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QElapsedTimer>
#include <QtEndian>
#include <archive.h>
#include <archive_entry.h>
#include <memory>
#include <linux/fs.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {
constexpr qint64 GiB = 1024LL * 1024 * 1024;

QString installationPath(int product)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + "/antigravity-manager/" + (product == 1 ? "antigravity-ide" : "antigravity");
}

QString launcherPath(int product)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + "/applications/io.github.antigravity-manager."
        + QFileInfo(installationPath(product)).fileName() + ".desktop";
}

bool exchangeDirectories(const QString &first, const QString &second)
{
    return syscall(SYS_renameat2, AT_FDCWD, QFile::encodeName(first).constData(),
                   AT_FDCWD, QFile::encodeName(second).constData(), RENAME_EXCHANGE) == 0;
}

bool isRunning(const QString &installation)
{
    const auto processes = QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &process : processes) {
        bool numeric = false;
        process.toUInt(&numeric);
        if (numeric && QFileInfo("/proc/" + process + "/exe").symLinkTarget()
                           .startsWith(installation + "/"))
            return true;
    }
    return false;
}

QString switchVersions(const QString &active, const QString &candidate)
{
    QElapsedTimer timer;
    timer.start();
    while (isRunning(active)) {
        if (timer.elapsed() >= 5 * 60 * 1000)
            return "The product is still running. Close it and retry.";
        QThread::msleep(400);
    }
    return exchangeDirectories(active, candidate) ? QString{} :
        QString("Could not switch versions. The active installation was preserved.");
}

bool extract(const QString &file, const QString &destination, const QString &root)
{
    std::unique_ptr<archive, decltype(&archive_read_free)> reader(archive_read_new(), archive_read_free);
    archive_read_support_filter_gzip(reader.get());
    archive_read_support_format_tar(reader.get());
    if (archive_read_open_filename(reader.get(), QFile::encodeName(file).constData(), 65536) != ARCHIVE_OK)
        return false;

    qint64 expanded = 0;
    int count = 0;
    archive_entry *entry = nullptr;
    int result;
    while ((result = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
        const char *raw = archive_entry_pathname(entry);
        if (!raw || ++count > 100000 || archive_filter_code(reader.get(), 0) != ARCHIVE_FILTER_GZIP)
            return false;
        QString path = QString::fromUtf8(raw);
        if (path.endsWith(QLatin1Char(47)))
            path.chop(1);
        const auto segments = path.split(QLatin1Char(47));
        if (segments.first() != root || segments.contains("..") || segments.contains(".") ||
            segments.contains(QString()) || archive_entry_symlink(entry) || archive_entry_hardlink(entry))
            return false;
        const QString target = destination + "/" + path;
        if (archive_entry_filetype(entry) == AE_IFDIR) {
            if (!QDir().mkpath(target))
                return false;
            continue;
        }
        const qint64 size = archive_entry_size(entry);
        if (archive_entry_filetype(entry) != AE_IFREG || size < 0 || size > 4 * GiB - expanded ||
            !QDir().mkpath(QFileInfo(target).absolutePath()))
            return false;
        QFile output(target);
        if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly))
            return false;
        char buffer[65536];
        qint64 written = 0;
        qint64 bytes;
        while ((bytes = archive_read_data(reader.get(), buffer, sizeof(buffer))) > 0) {
            if (bytes > size - written || output.write(buffer, bytes) != bytes)
                return false;
            written += bytes;
        }
        if (bytes < 0 || written != size || !output.flush())
            return false;
        expanded += written;
        auto permissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
        if (archive_entry_perm(entry) & 0111)
            permissions |= QFileDevice::ExeOwner;
        if (!output.setPermissions(permissions))
            return false;
    }
    return result == ARCHIVE_EOF && count > 0 && archive_read_close(reader.get()) == ARCHIVE_OK;
}

QByteArray appIcon(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const auto header = file.read(16);
    if (header.size() != 16 || qFromLittleEndian<quint32>(header.constData()) != 4)
        return {};
    const qint64 block = qFromLittleEndian<quint32>(header.constData() + 4);
    const qint64 length = qFromLittleEndian<quint32>(header.constData() + 12);
    if (block < 8 || block > 16 * 1024 * 1024 || length > block - 8)
        return {};
    const auto icon = QJsonDocument::fromJson(file.read(length)).object().value("files").toObject()
                         .value("icon.png").toObject();
    bool validOffset = false;
    const qint64 offset = icon.value("offset").toString().toLongLong(&validOffset);
    const int size = icon.value("size").toInt();
    const qint64 dataStart = 8 + block;
    if (!validOffset || offset < 0 || size < 1 || size > 1024 * 1024 ||
        offset > file.size() - dataStart - size || !file.seek(dataStart + offset))
        return {};
    return file.read(size);
}

QString desktopValue(QString value)
{
    return value.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t");
}

QString desktopExec(QString path)
{
    path.replace("\\", "\\\\").replace("\"", "\\\"").replace("$", "\\$").replace("`", "\\`").replace("%", "%%");
    return desktopValue("\"" + path + "\"");
}
}

bool hasInstallation(int product)
{
    const auto path = installationPath(product);
    return QFileInfo(path).isDir() && !QFileInfo(path).isSymLink() &&
        QFileInfo(path + "/" + QFileInfo(path).fileName()).isExecutable();
}

QString managedInstallationFolder(int product)
{
    return hasInstallation(product) ? installationPath(product) : QString{};
}

QString launchInstallation(int product)
{
    const auto active = installationPath(product);
    QLockFile lock(active + ".lock");
    if (!lock.tryLock())
        return "Another installation operation is running.";
    if (!hasInstallation(product) ||
        !QProcess::startDetached(active + "/" + QFileInfo(active).fileName(), {}, active))
        return "Could not start the product.";
    return {};
}

bool hasPreviousInstallation(int product)
{
    const auto path = installationPath(product);
    const QFileInfo previous(path + ".previous");
    return previous.isDir() && !previous.isSymLink() &&
        QFileInfo(previous.filePath() + "/" + QFileInfo(path).fileName()).isExecutable();
}

QString installedRelease(int product)
{
    QFile marker(installationPath(product) + "/.manager-version");
    return marker.open(QIODevice::ReadOnly) ? QString::fromUtf8(marker.read(128)) : QString{};
}

bool hasRtl(int product)
{
    return QFileInfo(installationPath(product) + "/.manager-rtl").isFile();
}

bool wantsRtl(int product)
{
    return hasRtl(product) ||
        QFileInfo(installationPath(product) + "/.manager-rtl-requested").isFile();
}

bool supportsRtl(int product)
{
    const bool ide = product == 1;
    QFile entry(installationPath(product) + (ide ? "/resources/app/out/main.js" : "/resources/app.asar"));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const QByteArray supported = ide
        ? "9c4202bf3deada931660486fb591faa4665ceb7cbcb9a4a6074c24eb446c6be8"
        : "94452cf0ce71bb4c21cfbdd51b31c0c6404e7aeb99539bdecce8954175b9dbaf";
    return entry.open(QIODevice::ReadOnly) && hash.addData(&entry) && hash.result().toHex() == supported;
}

QString setInstallationRtl(int product, bool enable)
{
    const auto active = installationPath(product);
    QLockFile lock(active + ".lock");
    if (!lock.tryLock())
        return "Another installation operation is running.";
    if (!hasInstallation(product) || hasRtl(product) == enable)
        return "RTL state did not change.";
    if (isRunning(active))
        return "Close the product before changing RTL.";

    const bool ide = product == 1;
    if (enable && !supportsRtl(product))
        return "RTL is not verified for this installation.";
    const QString miseData = qEnvironmentVariable("MISE_DATA_DIR",
        qEnvironmentVariable("XDG_DATA_HOME", QDir::homePath() + "/.local/share") + "/mise");
    QString node;
    for (const auto &candidate : {QStandardPaths::findExecutable("node"), miseData + "/shims/node"}) {
        if (!QFileInfo(candidate).isExecutable())
            continue;
        QProcess runtime;
        runtime.start(candidate, {"-e", "const [major,minor]=process.versions.node.split('.').map(Number); process.exit(major>22 || major===22 && minor>=12 ? 0 : 1)"});
        if (runtime.waitForFinished(5000) && runtime.exitStatus() == QProcess::NormalExit && runtime.exitCode() == 0) {
            node = candidate;
            break;
        }
    }
    if (node.isEmpty())
        return "RTL requires Node.js 22.12 or newer on PATH.";
    const auto script = QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath("../share/antigravity-release-check/rtl/managed.mjs");
    QTemporaryDir staging(QFileInfo(active).absolutePath() + "/.rtl-XXXXXX");
    if (!staging.isValid())
        return "Could not prepare RTL staging.";
    const auto candidate = staging.filePath("candidate");
    QProcess copy;
    copy.start("/usr/bin/cp", {"-a", "--reflink=auto", "--", active, candidate});
    if (!copy.waitForFinished(180000) || copy.exitStatus() != QProcess::NormalExit || copy.exitCode() != 0)
        return "Could not stage the installation for RTL.";
    QProcess patch;
    patch.start(node, {script, ide ? "ide" : "app", enable ? "apply" : "restore", candidate});
    if (!patch.waitForFinished(180000) || patch.exitStatus() != QProcess::NormalExit || patch.exitCode() != 0)
        return "RTL operation failed. The active installation was preserved.";
    QFile marker(candidate + "/.manager-rtl");
    if (enable ? !marker.open(QIODevice::WriteOnly | QIODevice::NewOnly) : !marker.remove())
        return "Could not record RTL state.";
    marker.close();
    QFile requested(candidate + "/.manager-rtl-requested");
    if (requested.exists() && !requested.remove())
        return "Could not record RTL state.";
    return switchVersions(active, candidate);
}

QString rollbackInstallation(int product)
{
    const auto active = installationPath(product);
    QLockFile lock(active + ".lock");
    if (!lock.tryLock())
        return "Another installation operation is running.";
    if (!hasInstallation(product) || !hasPreviousInstallation(product))
        return "No previous version is available.";
    return switchVersions(active, active + ".previous");
}

QString removeInstallation(int product)
{
    const auto active = installationPath(product);
    const auto previous = active + ".previous";
    const auto launcher = launcherPath(product);
    QLockFile lock(active + ".lock");
    if (!lock.tryLock())
        return "Another installation operation is running.";
    if (!hasInstallation(product))
        return "No managed installation is available to remove.";
    if (isRunning(active) || isRunning(previous))
        return "Close the product before removing it.";
    if (QFileInfo(previous).isSymLink() ||
        (QFileInfo::exists(previous) && !hasPreviousInstallation(product)))
        return "The previous version directory is invalid.";

    QTemporaryDir trash(QFileInfo(active).absolutePath() + "/.remove-XXXXXX");
    if (!trash.isValid())
        return "Could not prepare the removal directory.";
    const auto stagedActive = trash.filePath("active");
    const auto stagedPrevious = trash.filePath("previous");
    if (!QDir().rename(active, stagedActive))
        return "Could not move the installation.";

    const bool hadPrevious = QFileInfo::exists(previous);
    const bool movedPrevious = hadPrevious && QDir().rename(previous, stagedPrevious);
    const QFileInfo launcherInfo(launcher);
    const bool launcherPresent = launcherInfo.exists() || launcherInfo.isSymLink();
    if ((hadPrevious && !movedPrevious) || (launcherPresent && !QFile::remove(launcher))) {
        const bool restoredActive = QDir().rename(stagedActive, active);
        const bool restoredPrevious = !movedPrevious || QDir().rename(stagedPrevious, previous);
        if (!restoredActive || !restoredPrevious) {
            trash.setAutoRemove(false);
            return "Removal failed. Recovery files: " + trash.path();
        }
        return "Removal failed. The installation was restored.";
    }
    if (!trash.remove())
        return "The launcher was removed, but some files remain in " + trash.path();
    return {};
}

QString installArchive(int product, const QString &archivePath, bool update, const QString &version)
{
    const bool ide = product == 1;
    const QString root = ide ? "Antigravity IDE" : "Antigravity-x64";
    const QString binary = ide ? "antigravity-ide" : "antigravity";
    const QString name = ide ? "Antigravity IDE" : "Antigravity";
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString managed = data + "/antigravity-manager";
    const QString target = installationPath(product);
    const QString launcher = launcherPath(product);

    if (!QDir().mkpath(managed) || !QDir().mkpath(QFileInfo(launcher).absolutePath()))
        return "Could not create the installation directories.";
    QLockFile lock(managed + "/" + binary + ".lock");
    if (!lock.tryLock())
        return "Another installation is running.";
    if (update && (!hasInstallation(product) || !QFileInfo(launcher).isFile()))
        return "No managed installation is available to update.";
    if (!update && (QFileInfo::exists(target) || QFileInfo(target).isSymLink() ||
        QFileInfo::exists(launcher) || QFileInfo(launcher).isSymLink()))
        return "A managed installation or launcher already exists.";
    QFile installedVersion(target + "/.manager-version");
    if (update && !version.isEmpty() && installedVersion.open(QIODevice::ReadOnly) &&
        installedVersion.read(128) == version.toUtf8())
        return "This version is already installed.";
    if (QStorageInfo(managed).bytesAvailable() < 1024 * 1024 * 1024)
        return "At least 1 GiB of free disk space is required.";

    QTemporaryDir staging(managed + "/.install-XXXXXX");
    if (!staging.isValid() || !extract(archivePath, staging.path(), root))
        return "The archive could not be safely extracted.";
    const QString source = staging.filePath(root);
    if (!QFileInfo(source + "/" + binary).isExecutable())
        return "The expected executable is missing.";

    QImage icon;
    if (ide)
        icon.load(source + "/resources/app/resources/linux/code.png", "PNG");
    else
        icon.loadFromData(appIcon(source + "/resources/app.asar"), "PNG");
    if (icon.isNull() || !icon.save(source + "/manager-icon.png", "PNG"))
        return "The product icon could not be prepared.";

    if (!version.isEmpty()) {
        QFile marker(source + "/.manager-version");
        const auto bytes = version.toUtf8();
        if (!marker.open(QIODevice::WriteOnly | QIODevice::NewOnly) ||
            marker.write(bytes) != bytes.size() || !marker.flush())
            return "Could not record the installed version.";
    }
    if (update) {
        if (wantsRtl(product)) {
            QFile requested(source + "/.manager-rtl-requested");
            if (!requested.open(QIODevice::WriteOnly | QIODevice::NewOnly))
                return "Could not record RTL state.";
        }
        const auto previous = target + ".previous";
        if (QFileInfo(previous).isSymLink() ||
            (QFileInfo::exists(previous) && !hasPreviousInstallation(product)))
            return "The previous version directory is invalid.";
        // Exchange the active directory with the staged candidate in one operation.
        const auto error = switchVersions(target, source);
        if (!error.isEmpty())
            return error;
        if (QFileInfo::exists(previous)) {
            if (!exchangeDirectories(source, previous)) {
                staging.setAutoRemove(false);
                return "Updated, but the previous version could not be registered. Recovery files: " + staging.path();
            }
        } else if (!QDir().rename(source, previous)) {
            staging.setAutoRemove(false);
            return "Updated, but the previous version could not be registered. Recovery files: " + staging.path();
        }
        return {};
    }

    const QString entry = "[Desktop Entry]\nType=Application\nName=" + name +
        "\nExec=" + desktopExec(target + "/" + binary) + (ide ? "" : " %u") +
        "\nIcon=" + desktopValue(target + "/manager-icon.png") +
        "\nTerminal=false\nCategories=Development;\nKeywords=Antigravity;\n" +
        (ide ? "" : "MimeType=x-scheme-handler/antigravity;\n");
    if (!QDir().rename(source, target))
        return "The installation could not be activated.";
    QSaveFile desktop(launcher);
    const auto bytes = entry.toUtf8();
    if (!desktop.open(QIODevice::WriteOnly) || desktop.write(bytes) != bytes.size() || !desktop.commit()) {
        if (!QDir(target).removeRecursively())
            return "Launcher registration failed and the installation could not be removed.";
        return "The menu launcher could not be registered.";
    }
    return {};
}
