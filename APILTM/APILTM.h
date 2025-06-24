#pragma once

#include "ui_APILTM.h"
#include <QtWidgets/QWidget>

#include <cmath>
#include <tuple>

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
    QString selectedText = "RRR 1.5in"; // 把球类型
    QString sigleMeasureType = "点坐标测量"; // 测量类型
    QString dynamicsMeasureType = "稳定点模式"; // 动态测量方式
    constexpr static const char* API = "API";
    constexpr static const char* IP = "192.168.31.237"; // 测量仪器类型
    QString _instrumentType;
    QString pointName; // 点名称

public slots:
    /**
     * @brief 连接并初始化跟踪
     *
     */
    void TrackconnectAndStart();
    /**
     * @brief 刷新
     */
    void TrackRefresh();

    /**
     * @brief 单点测量
     */
    void TrackSignalMeasure();
    /**
     * @brief 动态测量
     */
    void TrackDynamicsMeasure();
    /**
     * @brief 停止测量
     */
    void TrackStop();
    /**
     * @brief 退出
     */
    void TrackExit();
    /**
     * @brief 回鸟巢
     */
    void TrackBackBirdNest();
    /**
     * @brief 选择类型
     */
    void onSelectInstrumentType(int index);
    /**
     * @brief 根据工件名称更新坐标系下拉框
     * @param workpiece
     */
    void updateCoordinateSystems(const QString& workpiece);

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
     * @brief 坐标转化
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标
     * @return hvd（高度、垂直度、偏移量）坐标
     */
    std::tuple<double, double, double> xyzToHvd(double x, double y, double z);
};
