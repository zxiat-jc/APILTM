#pragma once

#include "ui_APILTM.h"
#include <QtWidgets/QWidget>

class APILTM : public QWidget {
    Q_OBJECT

public:
    APILTM(QWidget* parent = nullptr);
    ~APILTM();
    void init();

private:
    Ui::APILTMClass ui;
    QString selectedText = "RRR 1.5in"; //
    QString sigleMeasureType = "点坐标测量";
    QString dynamicsMeasureType = "稳定点模式";
    constexpr static const char* API = "API";
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
    /**
     * @brief 测站名初始化
     */
    void initGetStations();

public:
    /**
     * @brief 点坐标测量
     */
    void coordinatePointMeasure();
    /**
     * @brief 定向点测量
     */
    void orientationPiontMeasure();
};
