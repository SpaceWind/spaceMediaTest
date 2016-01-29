#include <QStringList>
#include <QProcess>
#include <QDebug>
#include "cpustat.h"

CPUStat::CPUStat(QObject *parent) : QObject(parent)
{
    thread = new CPUStatThread(parent);
    connect(thread,SIGNAL(infoReady(CPUStatWorker::DeviceInfo)),this,SIGNAL(infoReady(CPUStatWorker::DeviceInfo)));
    thread->start();
}

void CPUStat::getInfo()
{
    thread->loadInfo();
}


CPUStatWorker::CPUStatWorker(QObject *parent) : QObject(parent)
{

}

void CPUStatWorker::getInfo()
{
    process = new QProcess();
    connect(process,SIGNAL(readyReadStandardOutput()),this,SLOT(readyRead()));
    process->start("bash data/cpu_usage.sh");
}

void CPUStatWorker::readyRead()
{
    result.append(process->readAllStandardOutput());

    QString s(result);
    if (s.indexOf("END") > 0)
    {
        QStringList items = QString(result).split(" ");
        DeviceInfo stat;
        stat.cpu = items[0].toInt();
        stat.memory = items[1].toInt();
        emit InfoReady(stat);
        process->deleteLater();
        result.clear();
    }
}

CPUStatThread::CPUStatThread(QObject *parent) : QThread(parent)
{
    worker = new CPUStatWorker(parent);
}

void CPUStatThread::loadInfo()
{
    worker->getInfo();
}

void CPUStatThread::run()
{
    qDebug() << "Initialization CPU Stat Thread";
    connect(worker,SIGNAL(InfoReady(CPUStatWorker::DeviceInfo)),this,SLOT(getInfo(CPUStatWorker::DeviceInfo)));
    exec();
}

void CPUStatThread::getInfo(CPUStatWorker::DeviceInfo info)
{
    emit infoReady(info);
}