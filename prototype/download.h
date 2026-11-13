#pragma once

#include <QFile>
#include <QLockFile>
#include <QNetworkReply>
#include <QNetworkRequest>

class Download {
public:
    explicit Download(const QUrl &url);
    bool open();
    QNetworkRequest request() const;
    bool write(QNetworkReply &reply);
    bool complete(QNetworkReply &reply);
    QString path() const { return file_.fileName(); }
    void discard();

private:
    bool acceptHeaders(QNetworkReply &reply);
    QUrl url_;
    QFile file_;
    QLockFile lock_;
    QByteArray validator_;
    qint64 offset_ = 0;
    qint64 expectedSize_ = -1;
    bool accepted_ = false;
};
