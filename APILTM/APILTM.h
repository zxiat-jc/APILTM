#pragma once

#include "ui_APILTM.h"
#include <QtWidgets/QWidget>

#include <cmath>
#include <optional>
#include <tuple>

#include <Eigen/Dense>
#include <QElapsedTimer>
#include <QFile>
#include <QMutex>
#include <QQueue>
#include <QSharedPointer>
#include <QTextStream>

#include "TrackerFilter.h"
#include "TrackerPoint.h"
// 定义常量（如果尚未定义）
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class APILTM : public QWidget {
    Q_OBJECT

public:
    APILTM(QWidget* parent = nullptr);
    ~APILTM();
    void init();

private:
    Ui::APILTMClass ui;
    QString pointName; // 点名称
    QFile dataFile; // 数据文件对象
    std::atomic_bool isDynamicMeasuring; // 动态测量状态标志
    QTextStream dataStream; // 文本流用于写入文件
    QString dyPointName; // 动态测量点名称
    QString desktopPath; // 添加桌面路径变量
    QString sigleMeasureType = "点坐标测量"; // 测量类型
    QString dynamicsMeasureType = "稳定点模式"; // 动态测量方式

    constexpr static const char* API = "API";
    QStringList dynamicDataList; // 用于存储动态测量数据
    QTimer* uiUpdateTimer; // UI更新定时器
    QMutex dataMutex; // 数据互斥锁
    TrackerFilter::TrackerPoint latestPoint; // 存储最新测量点
    bool hasNewData = false; // 是否有新数据标志
    Eigen::Vector3d latestTransformedPoint; // 存储最新转换后的坐标
public slots:
    /**
     * @brief 连接并初始化跟踪
     *
     */
    void trackconnectAndStart();
    /**
     * @brief 刷新
     */
    void trackRefresh();

    /**
     * @brief 单点测量
     */
    void trackSignalMeasure();

    /**
     * @brief 动态测量
     */
    void trackDynamicsMeasure();

    /**
     * @brief 停止测量
     */
    void trackStop();

    /**
     * @brief 退出
     */
    void trackExit();

    /**
     * @brief 回鸟巢
     */
    void trackBackBirdNest();

    /**
     * @brief 根据工件名称更新坐标系下拉框
     * @param workpiece
     */
    void updateCoordinateSystems(const QString& workpiece);

    /**
     * @brief 动态测量数据处理
     */
    void handleDynamicData(const QString& ip, const QString& name, const QString& type, TrackerFilter::TrackerPoint point);
    /**
     * @brief 更新动态UI
     */
    void updateUI();

public:
    /**
     * @brief 点坐标测量
     */
    void coordinatePointMeasure();
    /**
     * @brief 定向点测量
     */
    void orientationPiontMeasure();
    /**
     * @brief 列表改变
     */
    void listChange();
    /**
     * @brief 坐标系转化
     * @param name
     * @param point
     * @param route
     * @return
     */
    std::optional<std::pair<Eigen::Vector3d, Eigen::Vector3d>> coordinateSystemTransform(QString name, Eigen::Vector3d point);

private:
    /**
     * @brief 点坐标测量处理函数
     * @param data
     */
    void processCoordinateMeasurement(const QSharedPointer<TrackerPoint>& data);
    /**
     * @brief 定向点测量处理函数
     * @param data
     */
    void processOrientationMeasurement(const QSharedPointer<TrackerPoint>& data);
};
