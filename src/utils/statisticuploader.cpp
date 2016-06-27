#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QFile>
#include <QNetworkRequest>
#include <QUrl>
#include <QNetworkReply>
#include <QProcess>
#include <QTimer>
#include <QUrlQuery>

#include "statisticuploader.h"
#include "globalstats.h"
#include "globalconfig.h"
#include "singleton.h"
#include "sslencoder.h"

StatisticUploader::StatisticUploader(VideoService *videoService, QObject *parent) : QObject(parent)
{
    qDebug() << "Statistic Uploader Initialization";
    this->videoService = videoService;

    connect(&DatabaseInstance,SIGNAL(eventsFound(QList<StatisticDatabase::PlayEvent>)),this,SLOT(eventsReady(QList<StatisticDatabase::PlayEvent>)));
    connect(&DatabaseInstance,SIGNAL(playsFound(QList<StatisticDatabase::Play>)),this,SLOT(playsReady(QList<StatisticDatabase::Play>)));
    connect(&DatabaseInstance, SIGNAL(systemInfoFound(QList<Platform::SystemInfo>)),this,SLOT(systemInfoReady(QList<Platform::SystemInfo>)));
    connect(videoService,SIGNAL(sendStatisticResult(NonQueryResult)),this,SLOT(systemInfoUploadResult(NonQueryResult)));
    connect(videoService,SIGNAL(sendStatisticPlaysResult(NonQueryResult)),this,SLOT(playsUploadResult(NonQueryResult)));
    connect(videoService,SIGNAL(sendStatisticEventsResult(NonQueryResult)),this,SLOT(eventsUploadResult(NonQueryResult)));
}

bool StatisticUploader::start()
{
    qDebug() << "Uploader:start";
    DatabaseInstance.findEventsToSend();
    return false;
}

void StatisticUploader::playsReady(QList<StatisticDatabase::Play> plays)
{
    if (plays.count() == 0)
        return;
    QJsonArray result;
    foreach (const StatisticDatabase::Play &play, plays)
        result.append(play.serialize());
    QJsonDocument doc(result);
    QString strToSend = doc.toJson();
    videoService->sendPlays(strToSend);
}
void StatisticUploader::systemInfoReady(QList<Platform::SystemInfo> data)
{
    if (data.count() == 0)
    {
        return;
    }
    QJsonArray result;
    foreach (const Platform::SystemInfo &info, data)
        result.append(info.serialize());
    QJsonDocument doc(result);
    QString strToSend = doc.toJson();

    videoService->sendStatistic(strToSend);
}

void StatisticUploader::eventsReady(QList<StatisticDatabase::PlayEvent> events)
{
    if (events.count() == 0)
    {
        return;
    }
    QJsonArray result;
    foreach (const StatisticDatabase::PlayEvent &event, events)
        result.append(event.serialize());
    QJsonDocument doc(result);
    QString strToSend = doc.toJson();

    //for debugging
    PlatformSpecificService.writeToFile(strToSend.toLocal8Bit(), VIDEO_FOLDER + "stats.txt");

    //send via videoService
    videoService->sendEvents(strToSend);
}

void StatisticUploader::systemInfoUploadResult(NonQueryResult result)
{
    if (result.status == "success")
    {
        qDebug() << "SystemInfoUploadResult::Success";
        DatabaseInstance.systemInfoUploaded();
    }
    else
    {
        qDebug() << "SystemInfoUploadResult::FAIL" << result.source;
    }
}

void StatisticUploader::playsUploadResult(NonQueryResult result)
{
    if (result.status == "success")
    {
        qDebug() << "PlaysUploadResult::Success";
        DatabaseInstance.playsUploaded();
    }
    else
    {
        qDebug() << "PlaysUploadResult::FAIL" << result.source;
    }
}

void StatisticUploader::eventsUploadResult(NonQueryResult result)
{
    if (result.status == "success")
    {
        qDebug() << "EventsUploadResult::Success";
        DatabaseInstance.eventsUploaded();
    }
    else
    {
        qDebug() << "EventsUploadResult::FAIL" << result.source;
    }
}
