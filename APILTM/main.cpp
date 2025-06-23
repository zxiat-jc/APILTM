#include "APILTM.h"

#include <QFile>
#include <QProcess>
#include <QThread>
#include <QTimer>
#include <QtWidgets/QApplication>

#include "QPluginManager.h"
#include "TrackerInterface.h"
// 初始化
void initLock(QApplication& a)
{
    QFile file(QCoreApplication::applicationDirPath() + "/.run.lock");
    if (!file.exists()) {
        file.open(QIODevice::WriteOnly);
        file.close();
        // 等待文件创建完成
        QThread::msleep(100);
    }
    QObject::connect(&a, &QApplication::aboutToQuit, [&]() {
        qDebug() << "程序退出";
        QFile file(QCoreApplication::applicationDirPath() + "/.run.lock");
        if (file.exists()) {
            file.remove();
        }
    });
}
void initAdo(QApplication& a)
{
    QProcess* process = new QProcess(QCoreApplication::instance());
    // 获取当前程序路径
    QString path = QCoreApplication::applicationDirPath() + "/Ado.exe";
    // 头尾添加双引号,避免路径中有空格
    path = "\"" + path + "\"";
    // 进程终止消息
    QObject::connect(process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), [=](int exitCode, QProcess::ExitStatus exitStatus) {
        qDebug() << "Ado.exe进程终止";
        // 1s后重启
        QTimer::singleShot(1000, [=]() {
            process->start(path);
        });
    });
    process->start(path);
}

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);

    QPluginManager::Instance().findLoadPlugins(QCoreApplication::applicationDirPath() + "/plugins");
    qInfo() << "加载插件:" << QPluginManager::Instance().pluginNames();
    QString error;
    QPluginManager::Instance().initializes(a.arguments(), error);
    QPluginManager::Instance().extensionsInitialized();
    QPluginManager::Instance().delayedInitialize();
    void initLock(QApplication & a);
    void initAdo(QApplication & a);
    APILTM w;
    w.show();
    return a.exec();
}
