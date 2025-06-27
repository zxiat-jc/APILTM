#include "APILTM.h"

#include "DbServe.h"
#include "LoadingDialog.h"
#include "QPluginManager.h"
#include "Toast.h"
#include "TrackerFilter.h"
#include "TrackerInterface.h"
#include "TrackerPoint.h"

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
    init(); // 初始化
    listChange(); // 列表改变
    // 添加图片到指定位置
    QPixmap red(":/res/p1.png");
    // 设置图片大小
    red = red.scaled(54, 41);
    ui.lab_tupian->setPixmap(red);
    // 设置stations可编辑
    ui.stations->setEditable(true);

    auto hideAndRetain = [](QWidget* w) {
        w->hide();
        QSizePolicy sp = w->sizePolicy();
        sp.setRetainSizeWhenHidden(true);
        w->setSizePolicy(sp);
    };

    hideAndRetain(ui.stabilityPoint);
    hideAndRetain(ui.stabilitydis);
    hideAndRetain(ui.stabilitytime);
    hideAndRetain(ui.label_15);
    hideAndRetain(ui.label_14);
    // 获取桌面路径
    desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    {
        connect(ui.startapi, &QPushButton::clicked, this, &APILTM::trackconnectAndStart);
        connect(ui.refresh, &QPushButton::clicked, this, &APILTM::trackRefresh);
        connect(ui.signalmeasure, &QPushButton::clicked, this, &APILTM::trackSignalMeasure);
        connect(ui.dynamicsmeasure, &QPushButton::clicked, this, &APILTM::trackDynamicsMeasure);
        connect(ui.stop, &QPushButton::clicked, this, &APILTM::trackStop);
        connect(ui.exit, &QPushButton::clicked, this, &APILTM::trackExit);
        connect(ui.backbird, &QPushButton::clicked, this, &APILTM::trackBackBirdNest);
        QObject::connect(ui.instrumentType, &QComboBox::currentIndexChanged, this, &APILTM::onSelectInstrumentType);
        ui.signalmeasure->setEnabled(false);
        ui.backbird->setEnabled(false);
        ui.dynamicsmeasure->setEnabled(false);
        ui.stop->setEnabled(false);
        ui.lineEdit_4->setEnabled(false);
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
            });
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
                qDebug() << "当前测量类型：" << sigleMeasureType;
            });
    }
    // 动态测量选择QRadioButton
    {
        QButtonGroup* dynamicsType = new QButtonGroup(this);
        dynamicsType->addButton(ui.timeInterval, 1); // 设置 ID 为 1
        dynamicsType->addButton(ui.distanceInterval, 2);

        // 默认选中第1个
        ui.distance_mm->setEnabled(false);
        ui.time_ms->setEnabled(true);
        connect(dynamicsType, &QButtonGroup::buttonClicked, this,
            [this](QAbstractButton* button) {
                if (button == ui.timeInterval) {
                    dynamicsMeasureType = "时间间隔模式";
                    ui.time_ms->setEnabled(true);
                    ui.distance_mm->setEnabled(false);
                } else if (button == ui.distanceInterval) {
                    dynamicsMeasureType = "距离间隔模式";
                    ui.time_ms->setEnabled(false);
                    ui.distance_mm->setEnabled(true);
                }
            });
    }

    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        auto st = TRACKER_INTERFACE->status(API);

        if (st == TrackerEnum::MeasurmentStatus::ReadyToMeasure) {
            // 测量准备就绪
            QPixmap pix(":/res/p4.png");
            pix = pix.scaled(54, 41);
            ui.lab_tupian->setPixmap(pix);
            ui.signalmeasure->setEnabled(true);
            ui.backbird->setEnabled(true);
            ui.dynamicsmeasure->setEnabled(true);
            ui.stop->setEnabled(true);
        } else if (st == TrackerEnum::MeasurmentStatus::MeasurementInProgress) {
            // 测量进行中
            QPixmap pix(":/res/p2.png");
            pix = pix.scaled(54, 41);
            ui.lab_tupian->setPixmap(pix);
            ui.signalmeasure->setEnabled(false);
            ui.backbird->setEnabled(false);
            ui.dynamicsmeasure->setEnabled(false);
            ui.stop->setEnabled(true);
        } else {
            // 测量未就绪或无效状态
            QPixmap pix(":/res/p1.png");
            pix = pix.scaled(54, 41);
            ui.lab_tupian->setPixmap(pix);
            ui.signalmeasure->setEnabled(false);
            ui.backbird->setEnabled(false);
            ui.dynamicsmeasure->setEnabled(false);
            ui.stop->setEnabled(false);
        }
    });
    timer->start(1000); // 每500ms检查一次

    connect(TF, &TrackerFilter::arrived, this, &APILTM::handleDynamicData);
}

APILTM::~APILTM()
{
    if (TRACKER_INTERFACE && TRACKER_INTERFACE->contains(API)) {
        TRACKER_INTERFACE->disconnect(API);
        TRACKER_INTERFACE->remove(API);
    }
    trackStop();
}

void APILTM::init()
{
    // 初始化获取跟踪仪列表
    auto instrumentTypes = TRACKER_INTERFACE->supportType();
    qDebug() << instrumentTypes;
    if (instrumentTypes.isEmpty()) {
        TOAST_TIP("没有可用的跟踪仪器类型");
    } else {
        for (const auto& tracker : instrumentTypes) {
            ui.instrumentType->addItem(tracker);
            _instrumentType = ui.instrumentType->currentText();
        }
    }
    // 初始化测站
    auto stations = MW::GetStations();
    qDebug() << stations;
    if (stations.has_value()) {
        for (const auto& system : stations.value()) {
            QJsonObject obj = system.toObject();
            qDebug() << obj;
            QString st = obj["name"].toString();
            if (!st.isEmpty()) {
                ui.stations->addItem(st);
            }
        }
    } else {
        TOAST_TIP("获取测站失败");
    }
    // 初始化仪器类型下拉框
    auto workpieces = MW::GetWorkpieces();
    if (workpieces.has_value()) {
        auto&& arr = workpieces.value();
        for (const auto& item : arr) {
            QJsonObject obj = item.toObject();
            QString name = obj["工件名"].toString();
            ui.workpieceName->addItem(name);
        }
    } else {
        TOAST_TIP("获取工件失败");
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
            TOAST_TIP("获取坐标系失败");
        }
    }
}

void APILTM::listChange()
{
    // 连接信号槽：当工件选择变化时更新坐标系
    connect(ui.workpieceName, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) {
            if (index >= 0) {
                QString workpiece = ui.workpieceName->itemText(index);
                updateCoordinateSystems(workpiece);
            }
        });
    // 连接信号槽：仪器类型选择变化
    connect(ui.instrumentType, &QComboBox::currentTextChanged, this,
        [this](const QString& text) {
            qDebug() << "Selected instrument type:" << text;
            _instrumentType = text;
            if (TRACKER_INTERFACE && TRACKER_INTERFACE->contains(API)) {
                TRACKER_INTERFACE->disconnect(API);
                TRACKER_INTERFACE->remove(API);
            }
        });
    // 当测站输入框内容变化时，更新测站
    connect(ui.stations, &QComboBox::editTextChanged, this, [this](const QString& text) {
        if (!text.isEmpty()) {
            // 检查测站是否已存在
            if (!MW::GetStations().has_value() || !MW::GetStations().value().contains(text)) {
                // 如果不存在，则添加新的测站
                if (ui.lineIP->text().isEmpty()) {
                    TOAST_TIP("请输入仪器IP地址");
                    return;
                }
                bool su = MW::AddTracker(text, ui.lineIP->text(), _instrumentType);
                TOAST_TIP(su ? "测站添加成功" : "测站添加失败");
            }
        }
    });
}

void APILTM::trackconnectAndStart()
{

    if (ui.lineIP->text().isEmpty() && _instrumentType != "API") {
        TOAST_TIP("请输入仪器IP地址");
        return;
    }
    if (_instrumentType == "API") {
        if (ui.lineIP->text().isEmpty()) {
            TRACKER_INTERFACE->add("0.0.0.0", API, _instrumentType, selectedText);
            TRACKER_INTERFACE->connect(API);
            return;
        }
        auto apiIPAdess = TRACKER_INTERFACE->ip(API);
        if (apiIPAdess.has_value()) {
            ui.lineIP->setText(apiIPAdess.value());
        } else {
            TOAST_TIP("获取API IP地址失败");
            return;
        }
    }
    if (ui.instrumentType->currentIndex() < 0) {
        TOAST_TIP("请选择仪器类型");
        return;
    }
    QString IPs = ui.lineIP->text();
    qDebug() << "Connecting to tracker at IP:" << IPs;
    LoadingDialog::ShowLoading(tr("正在初始化..."), false, [&]() {
        TRACKER_INTERFACE->add(ui.lineIP->text(), API, _instrumentType, selectedText);
        if (TRACKER_INTERFACE->connect(API)) {
            if (TRACKER_INTERFACE->init(API)) {
                TOAST_TIP("初始化成功");
                QPixmap pix(":/res/p4.png");
                pix = pix.scaled(54, 41);
                ui.lab_tupian->setPixmap(pix);
                QTimer::singleShot(1000, this, [this]() {
                    ui.signalmeasure->setEnabled(true);
                    ui.backbird->setEnabled(true);
                    ui.dynamicsmeasure->setEnabled(true);
                    ui.stop->setEnabled(true);
                });
            } else {
                TOAST_TIP("初始化失败");
                QPixmap pix(":/res/p1.png");
                pix = pix.scaled(54, 41);
                ui.lab_tupian->setPixmap(pix);
            }
        } else {
            TOAST_TIP("连接失败");
        }
    });
}

void APILTM::trackRefresh()
{
    bool ad;
    bool re = TRACKER_INTERFACE->remove(API);
    if (_instrumentType == "API") {
        ad = TRACKER_INTERFACE->add(ui.lineIP->text(), API, _instrumentType, selectedText);
    } else {
        ad = TRACKER_INTERFACE->add(ui.lineIP->text(), API, _instrumentType, selectedText);
    }
    bool co = TRACKER_INTERFACE->connect(API);
    if (re && ad && co) {
        TOAST_TIP("刷新成功");
        QPixmap pix(":/res/p4.png");
        pix = pix.scaled(54, 41);
        ui.lab_tupian->setPixmap(pix);
    } else {
        TOAST_TIP("刷新失败");
        QPixmap pix(":/res/p1.png");
        pix = pix.scaled(54, 41);
        ui.lab_tupian->setPixmap(pix);
    }
}

void APILTM::trackSignalMeasure()
{
    if (ui.workpieceName->currentIndex() < 0) {
        TOAST_TIP("工件名称为空");
        return;
    }

    if (ui.piontname->text().isEmpty()) {
        TOAST_TIP("请先输入点名");
        return;
    }
    if (sigleMeasureType == "点坐标测量") {
        coordinatePointMeasure();
    } else if (sigleMeasureType == "定向点测量") {
        orientationPiontMeasure();
    }
}

void APILTM::trackDynamicsMeasure()
{
    // 初始化按钮组
    if (dynamicsMeasureType == "时间间隔模式") {
        TRACKER_INTERFACE->setProfileTime(API, ui.time_ms->text().toDouble());
        TRACKER_INTERFACE->setProfile(API, CONTINUOUS_TIME);
    } else if (dynamicsMeasureType == "距离间隔模式") {
        TRACKER_INTERFACE->setProfileDistance(API, ui.distance_mm->text().toDouble() * 1000);
        TRACKER_INTERFACE->setProfile(API, CONTINUOUS_DISTANCE);
    }

    // 更新测量状态
    isDynamicMeasuring = true;

    // 启动测量
    bool su = TRACKER_INTERFACE->startMeasure(API);
    if (su) {
        TOAST_TIP("动态测量开始成功");
    } else {
        TOAST_TIP("动态测量开始失败");
        dataFile.close();
        isDynamicMeasuring = false;
    }
}

void APILTM::trackStop()
{

    TRACKER_INTERFACE->stop();
    // 关闭文件
    if (dataFile.isOpen()) {
        QString filePath = dataFile.fileName();
        dataFile.close();
        TOAST_TIP("动态数据已保存至桌面");
    }
    isDynamicMeasuring = false;
    TOAST_TIP("停止测量成功");
    ui.signalmeasure->setEnabled(true);
    ui.backbird->setEnabled(true);
    ui.dynamicsmeasure->setEnabled(true);
}

void APILTM::trackExit()
{
    TRACKER_INTERFACE->disconnect(API);
    // 关闭当前页面
    this->close();
}

void APILTM::trackBackBirdNest()
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
    _instrumentType = ui.instrumentType->itemText(index);
    qDebug() << "Selected instrument type:" << _instrumentType;
}

void APILTM::coordinatePointMeasure()
{
    LoadingDialog::ShowLoading(tr("正在测量..."), false, [this]() {
        // 是否反面测量false
        auto&& opt = TRACKER_INTERFACE->measure(API, false);
        if (opt.has_value()) {
            auto&& data = opt.value();
            auto&& [s, h, v, d, rx, ry, rz, p, px, py, pz, t, hum, press, time] = *data;
            qDebug() << "单点测量数据hvd：" << h << v << d;
            auto [X, Y, Z] = GetLkXYZR(h, v, d);
            Eigen::Vector3d point(X, Y, Z);
            Eigen::Vector3d route(rx, ry, rz);
            // 显示当前坐标系点
            Eigen::Vector3d rs = coordinateSystemTransform(ui.coordinateSystem->currentText(), point, route);
            // qDebug() << "测量点坐标：" << rs.x() << rs.y() << rs.z();
            // UI更新通过主线程执行
            QMetaObject::invokeMethod(this, [&]() {
                ui.X->setText(F3(rs.x()));
                ui.Y->setText(F3(rs.y()));
                ui.Z->setText(F3(rs.z()));
                ui.RMSX->setText(F3(px));
                ui.RMSY->setText(F3(py));
                ui.RMSZ->setText(F3(pz));
                ui.RMS->setText(F3(p));
                ui.hum->setText(QString::number(hum));
                ui.press->setText(QString::number(press));
                pointName = Utils::SuffixAddOne(ui.piontname->text());
                ui.piontname->setText(pointName);
            });

            QDateTime instrumentTime = QDateTime::fromString(time, "yyyy-MM-dd hh:mm:ss.zzz");
            if (!instrumentTime.isValid()) {
                instrumentTime = QDateTime::currentDateTime();
            }
            QPair<QString, QString> dateTime = {
                instrumentTime.toString("yyyy-MM-dd"),
                instrumentTime.toString("hh:mm:ss")
            };
            // 保存原点坐标系
            QString pointName = ui.stations->currentText();
            // qDebug() << "测站名称：" << pointName;
            auto gt = MW::GetStnAxis(ui.stations->currentText());
            if (!gt.has_value()) {
                TOAST_TIP("获取测站坐标系失败");
                return;
            }
            auto&& axis = gt.value();
            Eigen::Vector3d origin(axis.getX(), axis.getY(), axis.getZ());
            Eigen::Vector3d result_0 = Utils::Geometry::Fun::Rotate::RightTransform(point, route, origin);

            // 如果checkbox选中，则保存点坐标
            bool success = MW::InsertWorkpiecePoint(ui.workpieceName->currentText(), ui.piontname->text(), QString::number(result_0.x()), QString::number(result_0.y()), QString::number(result_0.z()), QString::number(px), QString::number(py), QString::number(pz), QString::number(p), dateTime);
            if (success) {
                TOAST_TIP("点坐标测量成功");
            } else {
                TOAST_TIP("保存失败");
            }
        } else {
            TOAST_TIP("测量失败");
        }
    });
}

void APILTM::orientationPiontMeasure()
{
    LoadingDialog::ShowLoading(tr("正在测量..."), false, [this]() {
        // 是否反面测量false
        auto&& opt = TRACKER_INTERFACE->measure(API, false);
        if (opt.has_value()) {
            auto&& data = opt.value();
            auto&& [s, h, v, d, rx, ry, rz, p, px, py, pz, t, hum, press, time] = *data;
            qDebug() << "定向点测量数据hvd：" << h << v << d;
            // UI更新通过主线程执行
            QMetaObject::invokeMethod(this, [&]() {
                ui.hum->setText(QString::number(hum));
                ui.press->setText(QString::number(press));
                pointName = Utils::SuffixAddOne(ui.piontname->text());
                ui.piontname->setText(pointName);
            });

            QDateTime instrumentTime = QDateTime::fromString(time, "yyyy-MM-dd hh:mm:ss.zzz");
            if (!instrumentTime.isValid()) {
                instrumentTime = QDateTime::currentDateTime();
            }
            QPair<QString, QString> dateTime = {
                instrumentTime.toString("yyyy-MM-dd"),
                instrumentTime.toString("hh:mm:ss")
            };
            // 弧度转角度显示
            double h_rad = RAD2DEG(h);
            double v_rad = RAD2DEG(v);
            double distance = d; // 斜距
            QMetaObject::invokeMethod(this, [&]() {
                ui.hz_value->setText(F3(h_rad));
                ui.v_value->setText(F3(v_rad));
                ui.dis_value->setText(F3(distance));
            });
            // 存入观测值（弧度）
            bool success = MW::InsertObservation(ui.workpieceName->currentText(), ui.piontname->text(), ui.stations->currentText(), h, v, d, dateTime, hum, press);
            if (success) {
                TOAST_TIP("定向点保存成功");
            } else {
                TOAST_TIP("定向点保存失败");
            }
        } else {
            TOAST_TIP("测量失败");
        }
    });
}

Eigen::Vector3d APILTM ::coordinateSystemTransform(QString name, Eigen::Vector3d point, Eigen::Vector3d route)
{

    QString pointName = ui.stations->currentText();
    qDebug() << "测站名称：" << pointName;
    auto&& gt = MW::GetStnAxis(ui.stations->currentText());
    if (!gt.has_value()) {
        TOAST_TIP("获取测站坐标系失败");
        return Eigen::Vector3d::Zero();
    }
    auto&& axis = gt.value();

    Eigen::Vector3d origin(axis.getX(), axis.getY(), axis.getZ());

    Eigen::Vector3d re = Utils::Geometry::Fun::Rotate::RightTransform(point, route, origin);
    // 转换的子坐标系
    auto&& ga = MW::GetAxis(ui.workpieceName->currentText(), name);
    if (!ga.has_value()) {
        TOAST_TIP("获取坐标系失败");
        return Eigen::Vector3d(
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::quiet_NaN());
    }
    auto&& axis2 = ga.value();
    Eigen::Vector3d origin2(axis2.getX(), axis2.getY(), axis2.getZ());
    Eigen::Vector3d route2(axis2.getRx(), axis2.getRy(), axis2.getRz());
    Eigen::Vector3d result = Utils::Geometry::Fun::Rotate::RightReTransform(re, route2, origin2);
    return result;
}

void APILTM::handleDynamicData(const QString& ip, const QString& name, const QString& type, TrackerFilter::TrackerPoint point)
{
    if (!isDynamicMeasuring)
        return;

    // 创建文件
    if (ui.savaDyPoint->isChecked() && !dataFile.isOpen()) {
        QString fileName = QString("%1_%2_dynamic.txt")
                               .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"))
                               .arg(reinterpret_cast<quintptr>(this));
        QString fullPath = QDir(desktopPath).filePath(fileName);

        dataFile.setFileName(fullPath);
        if (!dataFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            TOAST_TIP("无法创建数据文件: " + fullPath);
            return;
        }

        dataStream.setDevice(&dataFile);
        // 写入表头
        if (sigleMeasureType == "点坐标测量") {
            dataStream << "\t\t\t\t\t\t动态测量数据记录\n";
            dataStream << "点号\tX(mm)\tY(mm)\tZ(mm)\t时间\n";
        } else if (sigleMeasureType == "定向点测量") {
            dataStream << "\t\t\t\t\t\t动态测量数据记录\n";
            dataStream << "点号\t水平角(°)\t垂直角(°)\t斜距(m)\t时间\n";
        }
        dataStream << "----------------------------------------\n";
        dataStream.flush();
    }
    // 解析数据点
    auto&& [s, h, v, d, rx, ry, rz, p, px, py, pz, t, hum, press, time] = point;
    qDebug() << "动态测量数据hvd：" << h << v << d;
    dyPointName = Utils::SuffixAddOne(dyPointName);
    // 处理时间戳
    QDateTime timestamp;
    if (!time.isEmpty()) {
        timestamp = QDateTime::fromString(time, "yyyy-MM-dd hh:mm:ss.zzz");
    }
    if (!timestamp.isValid()) {
        timestamp = QDateTime::currentDateTime();
    }

    if (sigleMeasureType == "点坐标测量") {
        auto [X, Y, Z] = GetLkXYZR(h, v, d);
        Eigen::Vector3d point(X, Y, Z);
        Eigen::Vector3d route(rx, ry, rz);
        Eigen::Vector3d rs = coordinateSystemTransform(ui.coordinateSystem->currentText(), point, route);
        if (std::isnan(rs.x()) || std::isnan(rs.y()) || std::isnan(rs.z())) {
            TOAST_TIP("坐标系转换失败");
            return;
        }
        QMetaObject::invokeMethod(this, [&]() {
            ui.X->setText(F3(rs.x()));
            ui.Y->setText(F3(rs.y()));
            ui.Z->setText(F3(rs.z()));
            ui.RMSX->setText(F3(px));
            ui.RMSY->setText(F3(py));
            ui.RMSZ->setText(F3(pz));
            ui.RMS->setText(F3(p));
            ui.hum->setText(QString::number(hum));
            ui.press->setText(QString::number(press));
        });
        if (ui.savaDyPoint->isChecked()) {
            // 写入数据到TXT文件 - 使用制表符分隔
            dataStream << dyPointName << "\t"
                       << QString::number(rs.x(), 'f', 3) << "\t"
                       << QString::number(rs.y(), 'f', 3) << "\t"
                       << QString::number(rs.z(), 'f', 3) << "\t"
                       << timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz") << "\n";
            dataStream.flush();
        }
    } else if (sigleMeasureType == "定向点测量") {
        double h_rad = RAD2DEG(h);
        double v_rad = RAD2DEG(v);
        double distance = d; // 斜距
        QMetaObject::invokeMethod(this, [&]() {
            ui.hz_value->setText(F3(h_rad));
            ui.v_value->setText(F3(v_rad));
            ui.dis_value->setText(F3(distance));
        });
        if (ui.savaDyPoint->isChecked()) {
            // 写入数据到TXT文件 - 使用制表符分隔
            dataStream << dyPointName << "\t"
                       << QString::number(h_rad, 'f', 3) << "\t"
                       << QString::number(v_rad, 'f', 3) << "\t"
                       << QString::number(distance, 'f', 3) << "\t"
                       << timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz") << "\n";

            // 刷新缓冲区确保数据写入
            dataStream.flush();
        }
    }
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

std::tuple<double, double, double> APILTM::xyzToHvd(double x, double y, double z)
{
    // 处理零坐标情况
    if (x == 0.0 && y == 0.0 && z == 0.0) {
        return { 0.0, 0.0, 0.0 };
    }

    double hd = std::sqrt(x * x + y * y);
    double v = 0.0;

    // 处理垂直角计算中的除零问题
    if (hd == 0 && z == 0) {
        v = 0.0;
    } else {
        v = std::atan2(hd, z); // 使用atan2避免除零错误
    }

    double h = 0.0;

    // 计算水平角
    if (x == 0.0) {
        if (y >= 0) {
            h = 3 * M_PI / 2;
        } else {
            h = M_PI / 2;
        }
    } else {
        if (x > 0) {
            if (y > 0) {
                h = 2 * M_PI - std::atan(y / x);
            } else {
                h = std::atan(-y / x);
            }
        } else {
            if (y > 0) {
                h = M_PI + std::atan(-y / x);
            } else {
                h = M_PI - std::atan(y / x);
            }
        }
    }

    // 将水平角调整到[-π, π]范围内
    if (h > M_PI) {
        h -= 2 * M_PI;
    } else if (h < -M_PI) {
        h += 2 * M_PI;
    }

    double d = std::sqrt(x * x + y * y + z * z);
    return { h, v, d };
}