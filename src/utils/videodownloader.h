#ifndef VIDEODOWNLOADER_H
#define VIDEODOWNLOADER_H

#include <QObject>
#include <QWidget>
#include <QProgressBar>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QLabel>
#include "videoserviceresult.h"

class VideoDownloader : public QObject
{
    Q_OBJECT
public:
    explicit VideoDownloader(PlayerConfig config, QObject *parent = 0);
    ~VideoDownloader();
    void checkDownload();
    void start();

    static QString getFileHash(QString fileName);
    void updateConfig(PlayerConfig config);
    int itemsToDownloadCount(){return itemsToDownload.count();}

signals:
    void done();
    void downloadProgress(double p);
    void totalDownloadProgress(double p, QString name);
    void downloadProgressSingle(double p, QString name);
public slots:
    void httpFinished();
    void httpReadyRead();
    void updateDataReadProgress(qint64 bytesRead, qint64 totalBytes);

private:
    void download();

    QNetworkAccessManager manager;
    QNetworkReply * reply;
    PlayerConfig config;
    QFile * file;
    QVector<PlayerConfig::Area::Playlist::Item> itemsToDownload;
    int currentItemIndex;
};

#endif // VIDEODOWNLOADER_H
