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
    bool isDynamicMeasuring; // 动态测量状态标志
    QTextStream dataStream; // 文本流用于写入文件
    QString dyPointName = "d0"; // 动态测量点名称
    QString desktopPath; // 添加桌面路径变量
    QString selectedText = "RRR 1.5in"; // 把球类型
    QString sigleMeasureType = "点坐标测量"; // 测量类型
    QString dynamicsMeasureType = "稳定点模式"; // 动态测量方式
    QString _instrumentType;
    constexpr static const char* API = "API";

    QStringList dynamicDataList; // 用于存储动态测量数据
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
     * @brief 选择类型
     */
    void onSelectInstrumentType(int index);
    /**
     * @brief 根据工件名称更新坐标系下拉框
     * @param workpiece
     */
    void updateCoordinateSystems(const QString& workpiece);

    /**
     * @brief 动态测量数据处理
     */
    void handleDynamicData(const QString& ip, const QString& name, const QString& type, TrackerFilter::TrackerPoint point);

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
    void processCoordinateMeasurement(const QSharedPointer<TrackerPoint>& data);

    void processOrientationMeasurement(const QSharedPointer<TrackerPoint>& data);
};
