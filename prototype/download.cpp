#include "download.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
constexpr qint64 maxBytes = 1024LL * 1024 * 1024;

QString cachePath(const QUrl &url)
{
    const auto key = QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Sha256).toHex();
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/downloads/" + key + ".part";
}

bool strongETag(const QByteArray &tag)
{
    static const QRegularExpression format(R"(^"[^"\x00-\x20\x7f]{1,250}"$)");
    return format.match(QString::fromLatin1(tag)).hasMatch();
}
}

Download::Download(const QUrl &url) : url_(url), file_(cachePath(url)), lock_(path() + ".lock")
{
}

bool Download::open()
{
    if (!QDir().mkpath(QFileInfo(path()).absolutePath()) || !lock_.tryLock() ||
        !file_.open(QIODevice::ReadWrite))
        return false;
    QFile metadata(path() + ".etag");
    if (metadata.open(QIODevice::ReadOnly))
        validator_ = metadata.read(256);
    offset_ = file_.size();
    if (!strongETag(validator_) || offset_ >= maxBytes) {
        offset_ = 0;
        return file_.resize(0);
    }
    return file_.seek(offset_);
}

QNetworkRequest Download::request() const
{
    QNetworkRequest request(url_);
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept-Encoding", "identity");
    if (offset_ > 0) {
        request.setRawHeader("Range", "bytes=" + QByteArray::number(offset_) + "-");
        request.setRawHeader("If-Range", validator_);
    }
    return request;
}

bool Download::acceptHeaders(QNetworkReply &reply)
{
    const int status = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto tag = reply.rawHeader("ETag");
    const auto encoding = reply.rawHeader("Content-Encoding");
    if (!encoding.isEmpty() && encoding != "identity")
        return false;
    if (status == 206 && offset_ > 0 && tag == validator_) {
        static const QRegularExpression range(R"(^bytes (\d+)-(\d+)/(\d+)$)");
        const auto match = range.match(QString::fromLatin1(reply.rawHeader("Content-Range")));
        bool valid = false;
        expectedSize_ = match.captured(3).toLongLong(&valid);
        return valid && expectedSize_ <= maxBytes && expectedSize_ > offset_ &&
            match.captured(1).toLongLong() == offset_ && match.captured(2).toLongLong() == expectedSize_ - 1;
    }
    if (status != 200)
        return false;
    const QVariant length = reply.header(QNetworkRequest::ContentLengthHeader);
    expectedSize_ = length.isValid() ? length.toLongLong() : -1;
    if (expectedSize_ > maxBytes || !file_.resize(0) || !file_.seek(0))
        return false;
    const auto metadataPath = path() + ".etag";
    QFile::remove(metadataPath);
    if (strongETag(tag)) {
        QSaveFile metadata(metadataPath);
        if (!metadata.open(QIODevice::WriteOnly) || metadata.write(tag) != tag.size() || !metadata.commit())
            return false;
    }
    return true;
}

bool Download::write(QNetworkReply &reply)
{
    if (!accepted_)
        accepted_ = acceptHeaders(reply);
    if (!accepted_)
        return false;
    const auto chunk = reply.readAll();
    return chunk.size() <= maxBytes - file_.pos() && file_.write(chunk) == chunk.size();
}

bool Download::complete(QNetworkReply &reply)
{
    const bool ok = reply.error() == QNetworkReply::NoError && write(reply) &&
        (expectedSize_ < 0 || file_.pos() == expectedSize_) && file_.flush();
    if (!ok && reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 416)
        discard();
    file_.close();
    return ok;
}

void Download::discard()
{
    file_.remove();
    QFile::remove(path() + ".etag");
}
