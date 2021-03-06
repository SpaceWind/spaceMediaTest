#include <QVector>
#include <QDebug>
#include <QFileInfo>
#include <QTimer>
#include <QSslKey>
#include <QtConcurrent/QtConcurrent>
#include "videodownloader.h"
#include "statisticdatabase.h"
#include "globalstats.h"
#include "platformdefines.h"


#ifdef PLATFORM_DEFINE_ANDROID
#include "JlCompress.h"
#endif

VideoDownloaderWorker::VideoDownloaderWorker(PlayerConfigAPI config, QObject *parent) : QObject(parent)
{
    this->config = config;
    file = 0;
    currentItemIndex = 0;
    connect (&DatabaseInstance,SIGNAL(resourceFound(QList<StatisticDatabase::Resource>)),this,SLOT(getResources(QList<StatisticDatabase::Resource>)));
    connect (&swapper,SIGNAL(done()),this,SLOT(download()));
    restarter = 0;
    manager = new QNetworkAccessManager(this);
    QObject::connect(manager, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)), this, SLOT(onSslError(QNetworkReply*, QList<QSslError>)));
}

VideoDownloaderWorker::~VideoDownloaderWorker()
{
    if (file)
        file->deleteLater();
}

void VideoDownloaderWorker::prepareDownload()
{
    getDatabaseInfo();
}

void VideoDownloaderWorker::checkDownload()
{
    int itemCount = 0;
    itemsToDownload.clear();
    QVector<PlayerConfigAPI::Campaign::Area::Content> allItems = config.items();
    std::sort(allItems.begin(), allItems.end(), [&, this](const PlayerConfigAPI::Campaign::Area::Content &a, const PlayerConfigAPI::Campaign::Area::Content &b)
    {
        if (GlobalStatsInstance.isItemHighPriority(a.content_id) && !GlobalStatsInstance.isItemHighPriority(b.content_id))
            return true;
        if (a.play_type == "free" && b.play_type != "free")
            return true;
        else
            return false;

        if (a.play_type == "only_empty" && b.play_type != "only_empty")
            return true;
        else
            return false;
    });

    foreach (const PlayerConfigAPI::Campaign::Area::Content &item, allItems)
    {
        //no need to download online resource
        if (item.type == "html5_online")
        {
            GlobalStatsInstance.setItemActivated(item.content_id, true);
            continue;
        }

        QString filename = VIDEO_FOLDER + item.content_id + item.file_hash + item.file_extension;
        QString filehash;
        if (!QFile::exists(filename))
        {
            qDebug() << "checkDownload:: file does not exists " << item.content_id << " and need to be downloaded";
            itemsToDownload.append(item);
        }
        else if ((filehash = getCacheFileHash(filename)) != item.file_hash)
        {
            qDebug() << "different hashes " << filehash << " vs " << item.file_hash;
            itemsToDownload.append(item);
        }
        else
            GlobalStatsInstance.setItemActivated(item.content_id, true);
        itemCount++;
    }

    GlobalStatsInstance.setContentPlay(itemCount);
    qDebug() << "FILES NEED TO BE DOWNLOADED: " << itemsToDownload.count();
    emit checkDownloadItemsTodownloadResult(itemsToDownload.count());

    //checking for ready-to-play items
    int readyToPlayCount = 0;
    foreach (const PlayerConfigAPI::Campaign::Area::Content &item, allItems)
        if (GlobalStatsInstance.isItemActivated(item.content_id))
            readyToPlayCount++;
    emit readyToPlayItemsCount(readyToPlayCount);
}

void VideoDownloaderWorker::onSslError(QNetworkReply *reply, QList<QSslError>)
{
    qDebug() << "VDW:SSLERROR!";
    reply->ignoreSslErrors();
}

void VideoDownloaderWorker::onSslError(QList<QSslError> data)
{
    QNetworkReply *r = qobject_cast<QNetworkReply *>(sender());
    qDebug() << "VDW:SSLERROR!";
    r->ignoreSslErrors();
}

void VideoDownloaderWorker::start()
{
    currentItemIndex = 0;
    download();
}

void VideoDownloaderWorker::download()
{

    if (itemsToDownload.count() > currentItemIndex)
    {
        qDebug() << "Downloading " + itemsToDownload[currentItemIndex].name;
        GlobalStatsInstance.setItemActivated(itemsToDownload[currentItemIndex].content_id, false);
        emit totalDownloadProgress(double(currentItemIndex+1)/double(itemsToDownload.count()),itemsToDownload[currentItemIndex].name);

        QString tempFileName = VIDEO_FOLDER + itemsToDownload[currentItemIndex].content_id +
                itemsToDownload[currentItemIndex].file_hash + itemsToDownload[currentItemIndex].file_extension + "_";

        if (QFile::exists(tempFileName))
        {
            qDebug() << "Preprocessing temp file";
            QFileInfo info(tempFileName);
            if (info.size() > itemsToDownload[currentItemIndex].file_size)
            {
                qDebug() << "Temp File is corrupted. Removing and downloading new one";
                QFile::remove(tempFileName);
            }
            if (getCacheFileHash(tempFileName) != itemsToDownload[currentItemIndex].file_hash &&
                info.size() == itemsToDownload[currentItemIndex].file_size)
            {
                qDebug() << "Temp File has normal size but wrong hash. Removing and downloading new one";
                QFile::remove(tempFileName);
            }
        }

        if (QFile::exists(tempFileName))
        {
            qDebug() << "temp file found. Resuming downloading";

            qDebug() << "Server hash " << itemsToDownload[currentItemIndex].file_hash;
            qDebug() << "tempFileHash" << getCacheFileHash(tempFileName);

            if (itemsToDownload[currentItemIndex].file_hash == getCacheFileHash(tempFileName))
            {
                PlayerConfigAPI::Campaign::Area::Content currentItem = itemsToDownload[currentItemIndex];
                QString currentItemId = currentItem.content_id;
                emit fileDownloaded(currentItemIndex);
                currentItemIndex++;
                if (currentItem.type == "html5_zip")
                {
                    PlatformSpecificService.extractFile(currentItemId + currentItem.file_hash + currentItem.file_extension, currentItemId);
                }
                else
                    swapper.add(VIDEO_FOLDER + currentItemId + currentItem.file_hash + currentItem.file_extension,
                                VIDEO_FOLDER + currentItemId + currentItem.file_hash + currentItem.file_extension + "_");
                swapper.start();
                QTimer::singleShot(5000, [currentItemId, currentItem]() {
                    qDebug() << "Item is ready " << currentItem.name;
                    GlobalStatsInstance.setItemActivated(currentItemId, true);
                });
                return;
            }

            QFileInfo info(tempFileName);
            file = new QFile(tempFileName);
            file->open(QFile::Append);
            if (info.size() == itemsToDownload[currentItemIndex].file_size)
            {

                qDebug()<<"Seems like file is already downloaded. Registering in database.";
                qDebug()<<"C="<<itemsToDownload.count() << " I=" << currentItemIndex;
                PlayerConfigAPI::Campaign::Area::Content currentItem = itemsToDownload[currentItemIndex];
                QString currentItemId = currentItem.content_id;
                emit fileDownloaded(currentItemIndex);
                currentItemIndex++;
                file->flush();
                file->close();
                reply = 0;
                delete file;
                file = 0;
                if (currentItem.type == "html5_zip")
                {
                    PlatformSpecificService.extractFile(currentItemId + currentItem.file_hash + currentItem.file_extension, currentItemId);
                }
                else
                    swapper.add(VIDEO_FOLDER + currentItemId + currentItem.file_hash + currentItem.file_extension,
                                VIDEO_FOLDER + currentItemId + currentItem.file_hash + currentItem.file_extension + "_");
                swapper.start();
                QTimer::singleShot(5000, [currentItemId, currentItem]() {
                    qDebug() << "Item is ready " << currentItem.name;
                    GlobalStatsInstance.setItemActivated(currentItemId, true);
                });
                return;
            }
            QNetworkRequest request(QUrl(itemsToDownload[currentItemIndex].file_url));
            QByteArray rangeHeaderValue = "bytes=" + QByteArray::number(info.size()) + "-";
            request.setRawHeader("Range",rangeHeaderValue);
            reply = manager->get(request);
            QObject::connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(onSslError(QList<QSslError>)));
        }
        else
        {
            file = new QFile(tempFileName);
            file->open(QFile::WriteOnly);

            QNetworkRequest request(QUrl(itemsToDownload[currentItemIndex].file_url));

            reply = manager->get(request);
            QObject::connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(onSslError(QList<QSslError>)));
        }

        connect(reply, SIGNAL(finished()), this, SLOT(httpFinished()));
        connect(reply, SIGNAL(readyRead()), this, SLOT(httpReadyRead()));
        connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(updateDataReadProgress(qint64,qint64)));
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(connectError(QNetworkReply::NetworkError)));
        GlobalStatsInstance.registryDownload();

        DatabaseInstance.registryResource(itemsToDownload[currentItemIndex].content_id,
                                          itemsToDownload[currentItemIndex].name,
                                          QDateTime::currentDateTimeUtc(),
                                          0);
    }
    else
    {
        qDebug() << "downloading completed";
        emit done(itemsToDownload.count());
    }
}

void VideoDownloaderWorker::connectError(QNetworkReply::NetworkError err)
{
    qDebug() << "Error! Connection lost!" << err;
  //  currentItemIndex--;
 //   if (restarter)
 //       delete restarter;
 //   reply->disconnect();
 //   restarter = new QTimer(this);
 //   restarter->start(10000);
 //   connect(restarter,SIGNAL(timeout()),this,SLOT(runDonwload()));
 //   file->flush();
 //   file->close();

 //   reply->deleteLater();
 //   file->deleteLater();
}

void VideoDownloaderWorker::runDonwload()
{
    qDebug() << "VDW: run Download";
    checkDownload();
    start();
}

void VideoDownloaderWorker::runDownloadNew()
{
    qDebug() << "VDW: run Donwload new";
    checkDownload();
    start();
}

void VideoDownloaderWorker::writeToFileJob(QFile *f, QNetworkReply *r)
{
    f->write(r->readAll());
    f->flush();
}

QString VideoDownloaderWorker::updateHash(QString fileName)
{
    QFile f(fileName);
    if (f.open(QFile::ReadOnly))
    {
        QCryptographicHash hash(QCryptographicHash::Md5);
        if (hash.addData(&f))
        {
            QString hashHex = QString(hash.result().toHex()).toLower();
            HashMeasure hashMeasure;
            hashMeasure.date = QDateTime::currentDateTime();
            hashMeasure.hash = hashHex;
            hashCache[fileName] = hashMeasure;
            f.close();
            return hashHex;
        }
    }
    return "";
}

QString VideoDownloaderWorker::getFileHash(QString fileName)
{
    QFile f(fileName);
    if (f.open(QFile::ReadOnly)) {
        QCryptographicHash hash(QCryptographicHash::Md5);
        if (hash.addData(&f)) {
            f.close();
            return QString(hash.result().toHex()).toLower();
        }
    }
    return "";
}

QString VideoDownloaderWorker::getCacheFileHash(QString fileName)
{
    QFileInfo fileInfo(fileName);

    if (hashCache.contains(fileName)){
        qDebug() << "md5 of " + fileName + " found in cache";
        if (fileInfo.lastModified() > hashCache[fileName].date)
        {
            qDebug() << "but file was updated: calculating hash";
            return updateHash(fileName);
        }
        else
            return hashCache[fileName].hash;
    }
    else
    {
        qDebug() << "filehash  of " + fileName + " was not found in cache - calculating!";
        return updateHash(fileName);
    }
}

void VideoDownloaderWorker::updateConfig(PlayerConfigAPI config)
{
    this->config = config;
    currentItemIndex = 0;
    if (file)
    {
        delete file;
        file = 0;
    }
}

void VideoDownloaderWorker::getDatabaseInfo()
{
    DatabaseInstance.getResources();
}

void VideoDownloaderWorker::httpFinished()
{
    if (reply->error())
    {
        qDebug() << "VDW::httpFinished -> Error No internet connection";
        reply->disconnect();
        QTimer::singleShot(10000,this,SLOT(download()));
        if (manager)
        {
            manager->disconnect();
            manager->deleteLater();
        }
        manager = new QNetworkAccessManager(this);
        return;
    }

    qDebug()<<"File downloading Finished. Registering in database.";
    qDebug()<<"C="<<itemsToDownload.count() << " I=" << currentItemIndex;
    PlayerConfigAPI::Campaign::Area::Content currentItem = itemsToDownload[currentItemIndex];
    QString currentItemId = currentItem.content_id;
    emit fileDownloaded(currentItemIndex);
    currentItemIndex++;
    file->flush();
    file->close();

    delete reply;
    reply = 0;
    delete file;
    file = 0;
    if (currentItem.type == "html5_zip")
    {
        PlatformSpecificService.extractFile(currentItemId + currentItem.file_hash + currentItem.file_extension, currentItemId);
    }
    else
        swapper.add(VIDEO_FOLDER + currentItemId + currentItem.file_hash + currentItem.file_extension,
                    VIDEO_FOLDER + currentItemId + currentItem.file_hash + currentItem.file_extension + "_");
    swapper.start();
    QTimer::singleShot(5000, [currentItemId, currentItem]() {
        qDebug() << "Item is ready " << currentItem.name;
        GlobalStatsInstance.setItemActivated(currentItemId, true);
    });
}

void VideoDownloaderWorker::httpReadyRead()
{
    static int v = 0;
    v++;
    if (restarter)
        restarter->stop();
    if (file)
    {
      //  QtConcurrent::run(writeToFileJob, file, reply);
        file->write(reply->readAll());
        file->flush();
        if (v % 10 == 0)
            qDebug() << QDateTime::currentDateTime().time().toString("HH:mm:ss ") << "updating file status: "
                     << itemsToDownload[currentItemIndex].name << " [ " << file->size() << " ] bytes";
    }
}

void VideoDownloaderWorker::updateDataReadProgress(qint64 bytesRead, qint64 totalBytes)
{
    emit downloadProgress(double(bytesRead)/double(totalBytes));

    double fileProgress = 1.0 / double(itemsToDownload.count());
    double totalProgress = double(currentItemIndex) * fileProgress;

    emit downloadProgressSingle(totalProgress + double(bytesRead)/double(totalBytes)*fileProgress, itemsToDownload[currentItemIndex].name);
}

void VideoDownloaderWorker::getResources(QList<StatisticDatabase::Resource> resources)
{
    this->resources = resources;
    checkDownload();
}


FileSwapper::FileSwapper()
{
    connect(&swapTimer,SIGNAL(timeout()),this, SLOT(runSwapCycle()));
}

void FileSwapper::add(QString mainFile, QString tempFile)
{
    SwapDefines defines;
    defines.mainFile = mainFile;
    defines.tempFile = tempFile;
    needToSwap.append(defines);
}

void FileSwapper::start()
{
    swapTimer.start(1000);
}

void FileSwapper::stop()
{
    swapTimer.stop();
}

void FileSwapper::runSwapCycle()
{
    QList<SwapDefines> rest;
    foreach (const SwapDefines &defs, needToSwap)
    {
        QFileInfo info(defs.mainFile);

        if (GlobalStatsInstance.getCurrentItem() != info.baseName())
        {
            QFile::remove(defs.mainFile);
            QFile::rename(defs.tempFile,defs.mainFile);
            qDebug() << "Swapped " << defs.mainFile << " with " << defs.tempFile;
        }
        else
            rest.append(defs);
    }
    needToSwap = rest;
    if (needToSwap.isEmpty())
    {
        swapTimer.stop();
        emit done();
    }
}

VideoDownloader::VideoDownloader(PlayerConfigAPI config, QObject *parent) : QThread(parent)
{
    qDebug() << "VIDEODOWNLOADER INIT";
    worker = new VideoDownloaderWorker(config,this);
    updateWorker = new UpdateDownloaderWorker(this);
    connect(worker,SIGNAL(done(int)),this, SIGNAL(done(int)));
    connect(worker,SIGNAL(downloadProgressSingle(double,QString)), this, SIGNAL(downloadProgressSingle(double,QString)));
    connect(worker, SIGNAL(checkDownloadItemsTodownloadResult(int)),this,SIGNAL(donwloadConfigResult(int)));
    connect(worker, SIGNAL(fileDownloaded(int)),this, SIGNAL(fileDownloaded(int)));
    connect(worker, SIGNAL(readyToPlayItemsCount(int)), this, SIGNAL(readyToPlayItemsCount(int)));
    connect(this, SIGNAL(runDownloadSignal()),worker,SLOT(runDonwload()));
    connect(this, SIGNAL(runDownloadSignalNew()),worker,SLOT(runDownloadNew()));
    //
    connect(updateWorker,SIGNAL(ready(QString)),this, SIGNAL(updateReady(QString)));
    connect(this,SIGNAL(startTask(QString,QString,QString)),updateWorker, SLOT(setTask(QString,QString,QString)));
}

VideoDownloader::~VideoDownloader()
{

}

void VideoDownloader::runDownload()
{
    qDebug() << "VIDEO DOWNLOADER::runDownload";
    emit runDownloadSignal();
}

void VideoDownloader::runDownloadNew()
{
    qDebug() << "VIDEO DOWNLOADER::runDownloadNew";
    emit runDownloadSignalNew();
}

void VideoDownloader::startUpdateTask(QString url, QString hash, QString filename)
{
    qDebug() << "VIDEO DOWNLOADER::startUpdateTask";
    emit startTask(url, hash, filename);
}

void VideoDownloader::run()
{

}

UpdateDownloaderWorker::UpdateDownloaderWorker(QObject *parent) : QObject(parent)
{
    manager = new QNetworkAccessManager(this);
    reply = 0;
    file = 0;
}

UpdateDownloaderWorker::~UpdateDownloaderWorker()
{
    if (file)
        file->deleteLater();
}

void UpdateDownloaderWorker::setTask(QString url, QString hash, QString filename)
{
    qDebug() << "UpdateDownloaderWorker::setTask " + url + " / " + hash + " / " + filename;
    this->url = url;
    this->hash = hash;
    this->fileName = filename;

    if (QFile::exists(fileName))
    {
        qDebug() << "UpdateDownloaderWorker::setTask temp file found! Trying to resume";
        QFileInfo info(filename);
        file = new QFile(filename);
        file->open(QFile::Append);
        QNetworkRequest request(url);
        QByteArray rangeHeaderValue = "bytes=" + QByteArray::number(info.size()) + "-";
        request.setRawHeader("Range", rangeHeaderValue);
        reply = manager->get(request);
    }
    else
    {
        file = new QFile(filename);
        file->open(QFile::WriteOnly);
        QNetworkRequest request(url);
        reply = manager->get(request);
    }
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(httpFinished()));
    QObject::connect(reply, SIGNAL(readyRead()), this, SLOT(httpReadyRead()));
}

void UpdateDownloaderWorker::httpReadyRead()
{
    if (file)
    {
        file->write(reply->readAll());
        file->flush();
    }
}

void UpdateDownloaderWorker::httpFinished()
{
    if (reply->error())
    {
        qDebug() << "UpdateDownloaderWorker::httpFinished -> Error No internet connection/error!";
        reply->disconnect();

        if (manager)
        {
            manager->disconnect();
            manager->deleteLater();
        }
        manager = new QNetworkAccessManager(this);
        return;
    }
    file->flush();
    file->close();

    delete reply;
    reply = 0;
    delete file;
    file = 0;
    emit ready(fileName);
}
