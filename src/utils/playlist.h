#ifndef RANDOMPLAYLIST_H
#define RANDOMPLAYLIST_H

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QWidget>
#include <QVector>
#include <QStringList>
#include "videoserviceresult.h"

#define MAGIC_PLAYLIST_VALUE 4.


//this file is for virtual playlist implementations
//main difference in these playlist is in implementation of next() method
//also updatePlaylist can be vary as we need to prepare lists in different manner
class AbstractPlaylist : public QObject
{
    Q_OBJECT
public:
    explicit AbstractPlaylist(QObject * parent);
    virtual ~AbstractPlaylist(){;}
    virtual void updatePlaylist(const PlayerConfigAPI::Campaign::Area &playlist)=0;
    virtual QString getType()=0;
    virtual PlayerConfigAPI::Campaign::Area::Content findItemById(QString iid)=0;
    QStringList forceItems;
public slots:
    virtual QString next()=0;
    virtual bool haveNext()=0;
    virtual QString getStoredItem()=0;
protected:
    PlayerConfigAPI::Campaign::Area playlist;
};

class SuperPlaylist : public AbstractPlaylist
{
    Q_OBJECT
public:
    explicit SuperPlaylist(QObject * parent);
    virtual ~SuperPlaylist(){;}
    virtual void updatePlaylist(const PlayerConfigAPI::Campaign::Area &playlist);
    virtual QString next();

    virtual bool haveNext();
    virtual PlayerConfigAPI::Campaign::Area::Content findItemById(QString id);
    virtual QString getType() {return "random";}
    virtual QString getStoredItem() {return storedNextItem;}
    void resetCurrentItemIndex();

protected:
    void splitItems();
    void shuffle(bool fixedFloating = true, bool floatingNone = true);
    QString nextFreeItem();


    int allLength;
    double magic;
    QDateTime minPlayTime;
    QString currentCampaignId;
    int currentItemIndex;
    QString storedNextItem;

    QList<PlayerConfigAPI::Campaign::Area::Content> normalFloatingItems;
    QList<PlayerConfigAPI::Campaign::Area::Content> floatingFreeItems;
    QHash<QString,PlayerConfigAPI::Campaign::Area::Content> items;
    QHash<QString,QDateTime> lastTimeShowed;
    QString lastPlayed; int lastFreeFloatingItemPlayedIndex;
};

#endif // RANDOMPLAYLIST_H
