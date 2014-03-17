#include "update.h"

#include <QTimer>
#include <QFileInfo>

UpdateManager::UpdateManager(QObject *parent): QObject(parent)
{
}

UpdateManager::~UpdateManager()
{
}

void UpdateManager::checkUpdate()
{
    QNetworkRequest request(PLNCOIN_UPDATE_URL);
    updateReply = manager.get(request);

    connect(updateReply, SIGNAL(finished()), this, SLOT(update()));
}

bool UpdateManager::isLatest() const
{
    return mLatest == PLNCOIN_VERSION;
}

void UpdateManager::update()
{
    QString ver(updateReply->readAll().data());
    mLatest = ver.toInt();
    latest(isLatest());
}

void UpdateManager::downloadAndSave(const QString& inFilename)
{
    mFile = inFilename;
    QTimer::singleShot(0, this, SLOT(startDownload()));
}

void UpdateManager::startDownload()
{
    QUrl url = QUrl(PLNCOIN_LATEST_URL);
    QString filename = saveFileName(url);
    if(!mFile.isEmpty()) filename = mFile;

    #ifdef WIN32
    if(!filename.endsWith(".exe")) {
        filename.append(".exe");
    }
    #endif

    output.setFileName(filename);

    if(!output.open(QIODevice::WriteOnly)) {
        fprintf(stderr, "Problem opening save file '%s' for download '%s': %s\n",
                qPrintable(filename), url.toEncoded().constData(),
                qPrintable(output.errorString()));

        return;
    }

    QNetworkRequest request(url);
    currentDownload = manager.get(request);

    connect(currentDownload, SIGNAL(downloadProgress(qint64,qint64)), SLOT(downloadProgress(qint64,qint64)));
    connect(currentDownload, SIGNAL(finished()), SLOT(downloadFinished()));
    connect(currentDownload, SIGNAL(readyRead()), SLOT(downloadReadyRead()));

    // Info o rozpoczęciu pobierania
    printf("Downloading %s...\n", url.toEncoded().constData());
    downloadTime.start();
}

void UpdateManager::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
}

void UpdateManager::downloadFinished()
{
    output.close();
    currentDownload->deleteLater();

    downloaded();
}

void UpdateManager::downloadReadyRead()
{
    output.write(currentDownload->readAll());
}

QString UpdateManager::saveFileName(const QUrl &url)
{
    QString path = url.path();
    QString basename = QFileInfo(path).fileName();

    if(basename.isEmpty()) {
        basename = "plncoin-qt";
    }

    if(QFile::exists(basename))
    {
        int i = 0;
        basename += '.';

        while(QFile::exists(basename + QString::number(i))) {
            ++i;
        }

        basename += QString::number(i);
    }

    return basename;
}
