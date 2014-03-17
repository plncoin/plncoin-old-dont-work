#ifndef UPDATE_H
#define UPDATE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTime>
#include <QFile>
#include <QString>
#include <QUrl>

const static QString PLNCOIN_UPDATE_URL = "http://raf.hajs.pl/update.txt";
const static QString PLNCOIN_LATEST_URL = "http://raf.hajs.pl/plncoin-latest.exe";
const static int PLNCOIN_VERSION = 1;

class UpdateManager : public QObject
{
    Q_OBJECT

public:

    UpdateManager(QObject *parent);
    virtual ~UpdateManager();

    void checkUpdate();
    bool isLatest() const;
    void downloadAndSave(const QString& inFilename);

signals:
    void latest(bool);
    void downloaded();

private slots:
    void update();
    void startDownload();

    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished();
    void downloadReadyRead();

private:
    QString saveFileName(const QUrl &url);

    QNetworkAccessManager manager;
    QNetworkReply *updateReply, *currentDownload;
    QFile output;
    QTime downloadTime;
    QString mFile;

    int mLatest;
};

#endif // UPDATE_H

