#include "APILTM.h"

#include "DbServe.h"
#include "LoadingDialog.h"
#include "QPluginManager.h"
#include "Toast.h"
#include "TrackerInterface.h"

#include "CheckBoxDelegate.h"
#include "QButtonDelegate.h"
#include "QtUtils.h"
#include <QButtonGroup>
#include <QStandardItemModel>
#include <QTimer>
APILTM::APILTM(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    init();
    initGetStations();
    ui.groupBox_6->hide();
    this->setAttribute(Qt::WA_DeleteOnClose);

    {
        connect(ui.startapi, &QPushButton::clicked, this, &APILTM::TrackconnectAndStart);
        connect(ui.refresh, &QPushButton::clicked, this, &APILTM::TrackRefresh);
        connect(ui.signalmeasure, &QPushButton::clicked, this, &APILTM::TrackSignalMeasure);
        connect(ui.dynamicsmeasure, &QPushButton::clicked, this, &APILTM::TrackDynamicsMeasure);
        connect(ui.stop, &QPushButton::clicked, this, &APILTM::TrackStop);
        connect(ui.exit, &QPushButton::clicked, this, &APILTM::TrackExit);
        connect(ui.backbird, &QPushButton::clicked, this, &APILTM::TrackBackBirdNest);
        QObject::connect(ui.instrumentType, &QComboBox::currentIndexChanged, this, &APILTM::onSelectInstrumentType);
        ui.signalmeasure->setEnabled(false);
        ui.backbird->setEnabled(false);
        ui.dynamicsmeasure->setEnabled(false);
        ui.stop->setEnabled(false);
        ui.lineEdit_4->setEnabled(false);
        ui.lineEdit_5->setEnabled(false);
    }

    {
        // 初始化按钮组
        QButtonGroup* radioGroup = new QButtonGroup(this);
        radioGroup->addButton(ui.radioButton, 1); // 设置 ID 为 1
        radioGroup->addButton(ui.radioButton_2, 2);
        radioGroup->addButton(ui.radioButton_3, 3);
        radioGroup->addButton(ui.radioButton_4, 4);

        // 默认选中第一个
        ui.radioButton->setChecked(true);
        connect(radioGroup, &QButtonGroup::buttonClicked, this,
            [this](QAbstractButton* button) {
                if (button == ui.radioButton) {
                    selectedText = "RRR 1.5in";
                } else if (button == ui.radioButton_2) {
                    selectedText = "RRR 0.5in";
                } else if (button == ui.radioButton_3) {
                    selectedText = "RRR 7/9in";
                } else if (button == ui.radioButton_4) {
                    selectedText = "Auto";
                }
                TRACKER_INTERFACE->remove(API);
                TRACKER_INTERFACE->add("0.0.0.0", API, _instrumentType, selectedText);
            });

        TRACKER_INTERFACE->add("0.0.0.0", API, _instrumentType, selectedText);
    }
    // 测量模式选择QRadioButton
    {
        QButtonGroup* measueType = new QButtonGroup(this);
        measueType->addButton(ui.coordinatePoint, 1); // 设置 ID 为 1
        measueType->addButton(ui.orientationPiont, 2);
        // 默认选中第一个
        ui.coordinatePoint->setChecked(true);
        connect(measueType, &QButtonGroup::buttonClicked, this,
            [this](QAbstractButton* button) {
                if (button == ui.coordinatePoint) {
                    sigleMeasureType = "点坐标测量";
                } else if (button == ui.orientationPiont) {
                    sigleMeasureType = "定向点测量";
                }
            });
    }
    // 动态测量选择QRadioButton
    {
        QButtonGroup* dynamicsType = new QButtonGroup(this);
        dynamicsType->addButton(ui.timeInterval, 1); // 设置 ID 为 1
        dynamicsType->addButton(ui.distanceInterval, 2);
        dynamicsType->addButton(ui.stabilityPoint, 3);
        // 默认选中第三个
        ui.stabilityPoint->setChecked(true);
        ui.distance_mm->setEnabled(false);
        ui.time_ms->setEnabled(false);
        connect(dynamicsType, &QButtonGroup::buttonClicked, this,
            [this](QAbstractButton* button) {
                if (button == ui.timeInterval) {
                    dynamicsMeasureType = "时间间隔模式";
                    ui.stabilitydis->setEnabled(false);
                    ui.stabilitytime->setEnabled(false);
                    ui.time_ms->setEnabled(true);
                    ui.distance_mm->setEnabled(false);
                } else if (button == ui.distanceInterval) {
                    dynamicsMeasureType = "距离间隔模式";
                    ui.stabilitydis->setEnabled(false);
                    ui.stabilitytime->setEnabled(false);
                    ui.time_ms->setEnabled(false);
                    ui.distance_mm->setEnabled(true);
                } else if (button == ui.stabilityPoint) {
                    dynamicsMeasureType = "稳定点模式";
                    ui.stabilitydis->setEnabled(true);
                    ui.stabilitytime->setEnabled(true);
                    ui.time_ms->setEnabled(false);
                    ui.distance_mm->setEnabled(false);
                }
            });
    }
}

APILTM::~APILTM()
{
    if (TRACKER_INTERFACE && TRACKER_INTERFACE->contains(API)) {
        TRACKER_INTERFACE->disconnect(API);
    }
}

void APILTM::init()
{
    auto workpieces = MW::GetWorkpieces();
    if (workpieces.has_value()) {
        auto&& arr = workpieces.value();
        for (const auto& item : arr) {
            QJsonObject obj = item.toObject();
            QString name = obj["工件名"].toString();
            ui.workpieceName->addItem(name);
        }
    } else {
        // TOAST_TIP("获取工件失败");
    }
    // 如果有工件，获取第一个工件的坐标系
    if (ui.workpieceName->count() > 0) {
        QString firstWorkpiece = ui.workpieceName->itemText(0);
        auto coordinateSystems = MW::GetWorkpiecesAxis(firstWorkpiece);

        if (coordinateSystems.has_value()) {
            ui.coordinateSystem->clear();
            for (const auto& system : coordinateSystems.value()) {
                QJsonObject obj = system.toObject();
                QString sysName = obj["name"].toString();
                ui.coordinateSystem->addItem(sysName);
            }
        } else {
            // TOAST_TIP("获取坐标系失败");
        }
    }
    // 连接信号槽：当工件选择变化时更新坐标系
    connect(ui.workpieceName, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) {
            if (index >= 0) {
                QString workpiece = ui.workpieceName->itemText(index);
                updateCoordinateSystems(workpiece);
            }
        });
    // 连接信号槽：仪器类型选择变化时重新连接
    connect(ui.instrumentType, &QComboBox::currentTextChanged, this,
        [this](const QString& text) {
            qDebug() << "Selected instrument type:" << text;
            _instrumentType = text;
            TRACKER_INTERFACE->remove(API);
            TRACKER_INTERFACE->add("0.0.0.0", API, _instrumentType, selectedText);
        });
    connect(ui.stations, &QComboBox::currentTextChanged, this,
        [this](const QString& stations) {

        });
}

void APILTM::TrackconnectAndStart()
{

    if (ui.instrumentType->currentIndex() < 0) {
        TOAST_TIP("请选择仪器类型");
        return;
    }

    LoadingDialog::ShowLoading(tr("正在初始化..."), false, [&]() {
        if (TRACKER_INTERFACE->connect(API)) {
            if (TRACKER_INTERFACE->init(API)) {
                TOAST_TIP("初始化成功");
                QTimer::singleShot(1000, this, [this]() {
                    ui.signalmeasure->setEnabled(true);
                    ui.backbird->setEnabled(true);
                    ui.dynamicsmeasure->setEnabled(true);
                    ui.stop->setEnabled(true);
                });
            } else {
                TOAST_TIP("初始化失败");
            }
        } else {
            TOAST_TIP("连接失败");
        }
    });
}

void APILTM::TrackRefresh()
{
    if (TRACKER_INTERFACE->contains(API)) {
        TRACKER_INTERFACE->remove(API);
        TRACKER_INTERFACE->add("0.0.0.0", API, _instrumentType, selectedText);
    }
}

void APILTM::TrackSignalMeasure()
{
    if (ui.piontname->text().isEmpty()) {
        TOAST_TIP("请先输入点名");
        return;
    }
    Measure();
}

void APILTM::TrackDynamicsMeasure()
{
    // 初始化按钮组
    if (dynamicsMeasureType == "时间间隔模式") {
        TRACKER_INTERFACE->setProfileTime(ui.stations->currentText(), ui.stabilitytime->text().toDouble());
    } else if (dynamicsMeasureType == "距离间隔模式") {
        TRACKER_INTERFACE->setProfileDistance(ui.stations->currentText(), ui.stabilitydis->text().toDouble() * 1000);
    } else if (dynamicsMeasureType == "稳定点模式") {
        TRACKER_INTERFACE->setProfileTime(ui.stations->currentText(), ui.stabilitytime->text().toDouble() * 1000);
        TRACKER_INTERFACE->setProfileDistance(ui.stations->currentText(), ui.stabilitydis->text().toDouble() * 1000);
    }
    auto&& opt = TRACKER_INTERFACE->measure(API, false);
    if (opt.has_value()) {
        auto&& data = opt.value();
        auto&& [s, h, v, d, rx, ry, rz, p, px, py, pz, t, hum, press, time] = *data;
        ui.X->setText(QString::number(h, 'f', 4));
        ui.Y->setText(QString::number(v, 'f', 4));
        ui.Z->setText(QString::number(d, 'f', 4));
        ui.RMSX->setText(QString::number(px, 'f', 4));
        ui.RMSY->setText(QString::number(py, 'f', 4));
        ui.RMSZ->setText(QString::number(pz, 'f', 4));
        ui.RMS->setText(QString::number(p, 'f', 4));
        ui.hum->setText(QString::number(hum));
        ui.press->setText(QString::number(press));
        QString pointname = Utils::SuffixAddOne(ui.piontname->text());
        ui.piontname->setText(pointname);
        QDateTime instrumentTime = QDateTime::fromString(time, "yyyy-MM-dd hh:mm:ss");
        QPair<QString, QString> dateTime = {
            instrumentTime.toString("yyyy-MM-dd"),
            instrumentTime.toString("hh:mm:ss")
        };

        if (ui.savaDyPoint->isChecked()) {
            // 如果checkbox选中，则保存点坐标
            if (sigleMeasureType == "点坐标测量") {
                // 存入坐标系
                bool success = MW::InsertWorkpiecePoint(ui.workpieceName->currentText(), ui.piontname->text(), QString::number(h), QString::number(v), QString::number(d), QString::number(px), QString::number(py), QString::number(pz), QString::number(p), dateTime);
                if (success) {
                    TOAST_TIP("点坐标测量成功");
                } else {
                    TOAST_TIP("保存失败");
                }
            } else if (sigleMeasureType == "定向点测量") {
                // 存入观测值
                bool success = MW::InsertObservation(ui.workpieceName->currentText(), ui.piontname->text(), ui.stations->currentText(), h, v, d, dateTime, hum, press);
                if (success) {
                    TOAST_TIP("定向点保存成功");
                } else {
                    TOAST_TIP("定向点保存失败");
                }
            }
        }
    }
}

void APILTM::TrackStop()
{
    TRACKER_INTERFACE->stop();
    TOAST_TIP("停止测量成功");
    ui.signalmeasure->setEnabled(true);
    ui.backbird->setEnabled(true);
    ui.dynamicsmeasure->setEnabled(true);
    ui.stop->setEnabled(false);
}

void APILTM::TrackExit()
{
    TRACKER_INTERFACE->disconnect(API);
    // 关闭当前页面
    this->close();
}

void APILTM::TrackBackBirdNest()
{
    if (TRACKER_INTERFACE->birdNest(API)) {
        TOAST_TIP("回鸟巢成功");
    } else {
        TOAST_TIP("回鸟巢失败");
    }
}

void APILTM::onSelectInstrumentType(int index)
{
    if (index < 0 || index >= ui.instrumentType->count()) {
        return; // 无效索引
    }
    QString selectedType = ui.instrumentType->itemText(index);
    qDebug() << "Selected type:" << selectedType;
}

void APILTM::Measure()
{
    LoadingDialog::ShowLoading(tr("正在测量..."), false, [this]() {
        // 是否反面测量false
        auto&& opt = TRACKER_INTERFACE->measure(API, false);
        if (opt.has_value()) {
            auto&& data = opt.value();
            auto&& [s, h, v, d, rx, ry, rz, p, px, py, pz, t, hum, press, time] = *data;
            // UI更新通过主线程执行
            QMetaObject::invokeMethod(this, [&]() {
                ui.X->setText(QString::number(h, 'f', 4));
                ui.Y->setText(QString::number(v, 'f', 4));
                ui.Z->setText(QString::number(d, 'f', 4));
                ui.RMSX->setText(QString::number(px, 'f', 4));
                ui.RMSY->setText(QString::number(py, 'f', 4));
                ui.RMSZ->setText(QString::number(pz, 'f', 4));
                ui.RMS->setText(QString::number(p, 'f', 4));
                ui.hum->setText(QString::number(hum));
                ui.press->setText(QString::number(press));
                QString pointname = Utils::SuffixAddOne(ui.piontname->text());
                ui.piontname->setText(pointname);
            });

            QDateTime instrumentTime = QDateTime::fromString(time, "yyyy-MM-dd hh:mm:ss.zzz");
            if (!instrumentTime.isValid()){
                instrumentTime = QDateTime::currentDateTime();
            }
            QPair<QString, QString> dateTime = {
                instrumentTime.toString("yyyy-MM-dd"),
                instrumentTime.toString("hh:mm:ss")
            };
            // 如果checkbox选中，则保存点坐标
            if (sigleMeasureType == "点坐标测量") {
                // 存入坐标系
                bool success = MW::InsertWorkpiecePoint(ui.workpieceName->currentText(), ui.piontname->text(), QString::number(h), QString::number(v), QString::number(d), QString::number(px), QString::number(py), QString::number(pz), QString::number(p), dateTime);
                if (success) {
                    TOAST_TIP("点坐标测量成功");
                } else {
                    TOAST_TIP("保存失败");
                }
            } else if (sigleMeasureType == "定向点测量") {
                // 存入观测值
                bool success = MW::InsertObservation(ui.workpieceName->currentText(), ui.piontname->text(), ui.stations->currentText(), h, v, d, dateTime, hum, press);
                if (success) {
                    TOAST_TIP("定向点保存成功");
                } else {
                    TOAST_TIP("定向点保存失败");
                }
            }
        } else {
            TOAST_TIP("测量失败");
        }
    });
}

void APILTM::updateCoordinateSystems(const QString& workpiece)
{
    if (workpiece.isEmpty()) {
        ui.coordinateSystem->clear();
        return;
    }

    auto coordinateSystems = MW::GetWorkpiecesAxis(workpiece);
    ui.coordinateSystem->clear();

    if (coordinateSystems.has_value()) {
        for (const auto& system : coordinateSystems.value()) {
            QJsonObject obj = system.toObject();
            qDebug() << obj;
            QString sysName = obj["name"].toString();
            if (!sysName.isEmpty()) {
                ui.coordinateSystem->addItem(sysName);
            }
        }
    } else {
        TOAST_TIP("获取坐标系失败");
    }
}

void APILTM::initGetStations()
{
    MW::AddTracker("3", "0,0,0,0", "AT960");
    auto stations = MW::GetStations();
    qDebug() << stations;
    if (stations.has_value()) {
        for (const auto& system : stations.value()) {
            QJsonObject obj = system.toObject();
            qDebug() << obj;
            QString st = obj["测站"].toString();
            if (!st.isEmpty()) {
                ui.stations->addItem(st);
            }
        }
    } else {
        // TOAST_TIP("获取坐标系失败");
    }
}
