#include "APILTM.h"

#include <functional>

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
#include <QMessageBox>
#include <QStandardItemModel>
#include <QTimer>
#include <QtConcurrent>

APILTM::APILTM(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    init(); // 初始化
    listChange(); // 列表改变
    QTimer::singleShot(0, this, [this]() { Utils::Gui::LabelImageMax(ui.picture, ":/res/p1.png"); });

    // 设置stations可编辑
    ui.stations->setEditable(true);
    ui.signalmeasure->setEnabled(false);
    ui.backbird->setEnabled(false);
    ui.dynamicsmeasure->setEnabled(false);
    ui.stop->setEnabled(false);
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

    QObject::connect(TF, &TrackerFilter::statused, this, [this](const QString& ip, const QString& name, const QString& type, TrackerFilter::MeasurmentStatus st) {
        if (st == TrackerFilter::MeasurmentStatus::ReadyToMeasure) {
            // 测量准备就绪
            Utils::Gui::LabelImageMax(ui.picture, ":/res/p4.png");
            ui.signalmeasure->setEnabled(true);
            ui.backbird->setEnabled(true);
            ui.dynamicsmeasure->setEnabled(true);
            ui.stop->setEnabled(true);
        } else if (st == TrackerFilter::MeasurmentStatus::NotReady || st == TrackerFilter::MeasurmentStatus::MeasurementInProgress) {
            // 测量进行中
            Utils::Gui::LabelImageMax(ui.picture, ":/res/p2.png");
            ui.signalmeasure->setEnabled(false);
            ui.backbird->setEnabled(true);
            ui.dynamicsmeasure->setEnabled(false);
            ui.stop->setEnabled(true);
        } else {
            // 测量未就绪或无效状态
            Utils::Gui::LabelImageMax(ui.picture, ":/res/p1.png");
            ui.signalmeasure->setEnabled(false);
            ui.backbird->setEnabled(false);
            ui.dynamicsmeasure->setEnabled(false);
            ui.stop->setEnabled(false);
        }
    });

    connect(TF, &TrackerFilter::arrived, this, &APILTM::handleDynamicData);

    if (ui.instrumentType->currentText() == API) {
        ui.lineIP->setEnabled(false);
    }
}

APILTM::~APILTM()
{
    if (TRACKER_INTERFACE && TRACKER_INTERFACE->contains(API)) {
        TRACKER_INTERFACE->disconnect(API);
        TRACKER_INTERFACE->remove(API);
    }
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
            updateCoordinateSystems(name);
            ui.workpieceName->addItem(name);
        }
    } else {
        TOAST_TIP("获取工件失败");
    }
}

void APILTM::listChange()
{

    // 连接信号槽：仪器类型选择变化
    connect(ui.instrumentType, &QComboBox::currentTextChanged, this,
        [this](const QString& text) {
            qDebug() << "Selected instrument type:" << text;
            _instrumentType = text;
            ui.lineIP->setEnabled(ui.instrumentType->currentText() != API);
            ui.balls->clear();
            LoadingDialog::ShowLoading(tr("正在断开···"), false, []() { TRACKER_INTERFACE->remove(API); });
        });

    QObject::connect(ui.balls, &QComboBox::currentIndexChanged, this, [this](int) {
        auto&& ball = ui.balls->currentText();
        if (!ball.isEmpty() && TRACKER_INTERFACE->status(API) != TrackerEnum::MeasurmentStatus::Invalid) {
            TRACKER_INTERFACE->setBall(API, ball);
        }
    });
}

void APILTM::trackconnectAndStart()
{
    if (ui.instrumentType->currentIndex() < 0) {
        TOAST_TIP("请选择仪器类型");
        return;
    }

    auto&& ip = ui.lineIP->text();
    if (ip.isEmpty() && _instrumentType != API) {
        TOAST_TIP("请输入仪器IP地址");
        return;
    }

    QString station = ui.stations->currentText();
    if (station.isEmpty()) {
        TOAST_TIP("测站为空");
        return;
    }

    // 获取当前所有测站
    QJsonArray stationsArray = MW::GetStations().value();
    bool stationExists = false;

    // 遍历检查测站是否已存在
    for (const QJsonValue& value : stationsArray) {
        QJsonObject obj = value.toObject();
        if (obj["name"].toString() == station) { // 直接比较name字段
            stationExists = true;
            break;
        }
    }

    if (!stationExists) {
        bool su = MW::AddTracker(station, ip, _instrumentType);
        if (su) {
            TOAST_TIP("添加成功");
            ui.workpieceName->addItem(station);
        }
    }
    // 更新测站IP
    if (!MW::EditTracker(station, ip, _instrumentType, "")) {
        TOAST_TIP("更新测站IP失败");
        return;
    }

    qDebug() << "Connecting to tracker at IP:" << ip;
    TRACKER_INTERFACE->add(ip, API, _instrumentType, "");

    std::function<void()> fun = [this, ip]() {
        if (!TRACKER_INTERFACE->connect(API)) {
            TOAST_TIP("连接失败");
            return;
        }

        QMetaObject::invokeMethod(this, [this]() {
            ui.balls->clear();
            auto&& balls = TRACKER_INTERFACE->balls(API);
            // 逆序
            std::reverse(balls.begin(), balls.end());
            ui.balls->addItems(balls);
            TRACKER_INTERFACE->setBall(API, ui.balls->currentText());
            ui.lineIP->setText(TRACKER_INTERFACE->ip(API).value_or(""));
        });
    };

    if (_instrumentType != API) {
        LoadingDialog::ShowLoading(tr("正在初始化···"), false, fun);
    } else {
        fun();
    }
}

void APILTM::trackRefresh()
{
    bool ad;
    bool re = TRACKER_INTERFACE->remove(API);
    if (_instrumentType == API) {
        ad = TRACKER_INTERFACE->add(ui.lineIP->text(), API, _instrumentType, "");
    } else {
        ad = TRACKER_INTERFACE->add(ui.lineIP->text(), API, _instrumentType, "");
    }
    bool co = TRACKER_INTERFACE->connect(API);
    if (re && ad && co) {
        TOAST_TIP("刷新成功");
    } else {
        TOAST_TIP("刷新失败");
    }
}

void APILTM::trackSignalMeasure()
{

    QJsonArray stationsArray = MW::GetStations().value();
    bool stationExists = false;
    // 遍历检查测站是否已存在
    for (const QJsonValue& value : stationsArray) {
        QJsonObject obj = value.toObject();
        if (obj["name"].toString() == ui.stations->currentText()) { // 直接比较name字段
            stationExists = true;
            break;
        }
    }
    if (!stationExists) {
        TOAST_TIP("测站不存在");
        return;
    }
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
    dyPointName = ui.piontname->text();
    QJsonArray stationsArray = MW::GetStations().value();
    bool stationExists = false;
    // 遍历检查测站是否已存在
    for (const QJsonValue& value : stationsArray) {
        QJsonObject obj = value.toObject();
        if (obj["name"].toString() == ui.stations->currentText()) { // 直接比较name字段
            stationExists = true;
            break;
        }
    }
    if (!stationExists) {
        TOAST_TIP("测站不存在");
        return;
    }

    // 初始化UI更新定时器
    uiUpdateTimer = new QTimer(this);
    uiUpdateTimer->setInterval(200);
    connect(uiUpdateTimer, &QTimer::timeout, this, &APILTM::updateUI);
    // 初始化按钮组
    if (dynamicsMeasureType == "时间间隔模式") {
        TRACKER_INTERFACE->setProfileTime(API, ui.time_ms->text().toDouble());
        TRACKER_INTERFACE->setProfile(API, CONTINUOUS_TIME);
    } else if (dynamicsMeasureType == "距离间隔模式") {
        TRACKER_INTERFACE->setProfileDistance(API, ui.distance_mm->text().toDouble() * 1000);
        TRACKER_INTERFACE->setProfile(API, CONTINUOUS_DISTANCE);
    }

    // 清空数据
    {
        QMutexLocker locker(&dataMutex);
        dynamicDataList.clear();
        hasNewData = false;
    }

    // 更新测量状态
    isDynamicMeasuring = true;
    uiUpdateTimer->start();

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

    // 安全停止定时器
    if (uiUpdateTimer && uiUpdateTimer->isActive()) {
        uiUpdateTimer->stop();
        qDebug() << "UI update timer stopped";
    }

    // 判断是否正在动态测量
    LoadingDialog::ShowLoading(tr("正在保存···"), false, [this]() {
        // 添加空指针检查
        if (TRACKER_INTERFACE && TRACKER_INTERFACE->contains(API)) {
            TRACKER_INTERFACE->stop();
        }
        isDynamicMeasuring = false;
        TOAST_TIP("停止测量成功");;

        if (!ui.savaDyPoint->isChecked() || dynamicDataList.isEmpty()) {
            return;
        }
        // 关闭文件

        // 对数据进行排序（按点名）
        std::sort(dynamicDataList.begin(), dynamicDataList.end(),
            [](const QString& a, const QString& b) {
                // 提取点名部分（假设格式为"dX"）
                bool ok;
                int aNum = a.left(a.indexOf('\t')).mid(1).toInt(&ok);
                if (!ok)
                    return false;
                int bNum = b.left(b.indexOf('\t')).mid(1).toInt(&ok);
                if (!ok)
                    return false;
                return aNum < bNum;
            });
        QString fileName = QString("%1_%2_dynamic.txt")
                               .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"))
                               .arg(reinterpret_cast<quintptr>(this));
        QString fullPath = QDir(desktopPath).filePath(fileName);

        QFile file(fullPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            TOAST_TIP("无法创建数据文件: " + fullPath);
            return;
        }
        QTextStream stream(&file);

        // 写入表头
        if (sigleMeasureType == "点坐标测量") {
            stream << "\t\t\t\t\t\t动态测量数据记录\n";
            stream << "点号\tX(mm)\tY(mm)\tZ(mm)\t时间\n";
        } else if (sigleMeasureType == "定向点测量") {
            stream << "\t\t\t\t\t\t动态测量数据记录\n";
            stream << "点号\t水平角(弧度)\t垂直角(弧度)\t斜距(m)\t时间\n";
        }
        stream << "----------------------------------------\n";

        // 写入所有数据
        foreach (const QString& line, dynamicDataList) {
            stream << line;
        }

        file.close();
        TOAST_TIP(QString("动态数据已保存至桌面，共%1条记录").arg(dynamicDataList.count()));

        dynamicDataList.clear();
    });
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

void APILTM::coordinatePointMeasure()
{
    LoadingDialog::ShowLoading(tr("正在测量···"), false, [this]() {
        auto&& opt = TRACKER_INTERFACE->measure(API, false);
        if (opt.has_value()) {
            // 将数据处理移到主线程
            QMetaObject::invokeMethod(this, [this, data = opt.value()]() {
                processCoordinateMeasurement(data);
            });
        } else {
            QMetaObject::invokeMethod(this, "showToast", Qt::QueuedConnection,
                Q_ARG(QString, "测量失败"));
        }
    });
}

void APILTM::processCoordinateMeasurement(const QSharedPointer<TrackerPoint>& data)
{
    auto&& [s, h, v, d, rx, ry, rz, p, px, py, pz, t, hum, press, time] = *data;

    auto [X, Y, Z] = GetLkXYZR(h, v, d);
    Eigen::Vector3d point(X, Y, Z);
    // 显示当前坐标系点
    auto&& opt = coordinateSystemTransform(ui.coordinateSystem->currentText(), point);
    if (!opt.has_value()) {
        return;
    }
    auto&& [result, rs] = opt.value();

    // UI更新
    ui.X->setText(F3(rs.x()));
    ui.Y->setText(F3(rs.y()));
    ui.Z->setText(F3(rs.z()));
    ui.RMSX->setText(F3(px));
    ui.RMSY->setText(F3(py));
    ui.RMSZ->setText(F3(pz));
    ui.RMS->setText(F3(p));
    ui.tem->setText(QString::number(t));
    ui.press->setText(QString::number(press));
    // 弧度转角度显示
    double h_deg = RAD2DEG(h);
    double v_deg = RAD2DEG(v);
    h_deg = (h_deg < 0) ? h_deg + 360 : h_deg;
    ui.hz_value->setText(F3(h_deg));
    ui.v_value->setText(F3(v_deg));
    ui.dis_value->setText(F3(d));

    // 检查点是否存在
    if (MW::CheckPointExist(ui.workpieceName->currentText(), ui.piontname->text())) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "点已存在", "该点已存在，是否覆盖？",
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            TOAST_TIP("操作已取消");

            return;
        } else {
            MW::DeletePoint(ui.workpieceName->currentText(), ui.piontname->text());
        }
    }

    // 时间处理
    QDateTime instrumentTime = QDateTime::fromString(time, "yyyy-MM-dd hh:mm:ss.zzz");
    if (!instrumentTime.isValid()) {
        instrumentTime = QDateTime::currentDateTime();
    }

    QPair<QString, QString> dateTime = {
        instrumentTime.toString("yyyy-MM-dd"),
        instrumentTime.toString("hh:mm:ss")
    };
    qDebug() << result.x() << result.y() << result.z();
    // 保存点坐标
    bool success = MW::InsertWorkpiecePoint(
        ui.workpieceName->currentText(),
        ui.piontname->text(),
        QString::number(result.x()),
        QString::number(result.y()),
        QString::number(result.z()),
        QString::number(px),
        QString::number(py),
        QString::number(pz),
        QString::number(p),
        dateTime);

    if (success) {
        // 更新点名
        pointName = Utils::SuffixAddOne(ui.piontname->text());
        ui.piontname->setText(pointName);
        TOAST_TIP("点坐标测量成功");
    } else {
        TOAST_TIP("保存失败");
    }
}

void APILTM::orientationPiontMeasure()
{
    LoadingDialog::ShowLoading(tr("正在测量···"), false, [this]() {
        auto&& opt = TRACKER_INTERFACE->measure(API, false);
        if (opt.has_value()) {
            QMetaObject::invokeMethod(this, [this, data = opt.value()]() {
                processOrientationMeasurement(data);
            });
        } else {
            QMetaObject::invokeMethod(this, "showToast", Qt::QueuedConnection,
                Q_ARG(QString, "测量失败"));
        }
    });
}

void APILTM::processOrientationMeasurement(const QSharedPointer<TrackerPoint>& data)
{
    auto&& [s, h, v, d, rx, ry, rz, p, px, py, pz, t, hum, press, time] = *data;
    {
        auto [X, Y, Z] = GetLkXYZR(h, v, d);
        Eigen::Vector3d point(X, Y, Z);
        // 显示当前坐标系点
        auto&& opt = coordinateSystemTransform(ui.coordinateSystem->currentText(), point);
        if (!opt.has_value()) {
            return;
        }
        auto&& [result, rs] = opt.value();

        // UI更新
        ui.X->setText(F3(rs.x()));
        ui.Y->setText(F3(rs.y()));
        ui.Z->setText(F3(rs.z()));
        ui.RMSX->setText(F3(px));
        ui.RMSY->setText(F3(py));
        ui.RMSZ->setText(F3(pz));
        ui.RMS->setText(F3(p));
        // UI更新
        ui.tem->setText(QString::number(t));
        ui.press->setText(QString::number(press));
    }

    // 角度转角度显示
    double h_deg = RAD2DEG(h);
    double v_deg = RAD2DEG(v);
    double distance = d;
    h_deg = (h_deg < 0) ? h_deg + 360 : h_deg;
    ui.hz_value->setText(F3(h_deg));
    ui.v_value->setText(F3(v_deg));
    ui.dis_value->setText(F3(distance));

    // 时间处理
    QDateTime instrumentTime = QDateTime::fromString(time, "yyyy-MM-dd hh:mm:ss.zzz");
    if (!instrumentTime.isValid()) {
        instrumentTime = QDateTime::currentDateTime();
    }

    QPair<QString, QString> dateTime = {
        instrumentTime.toString("yyyy-MM-dd"),
        instrumentTime.toString("hh:mm:ss")
    };

    // 检查点是否存在
    if (MW::CheckObservationExist(ui.workpieceName->currentText(), ui.piontname->text(), ui.stations->currentText())) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "点已存在", "该点已存在，是否覆盖？",
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No) {
            TOAST_TIP("操作已取消");
            return;
        } else {
            MW::DeleteObservation(ui.workpieceName->currentText(), ui.piontname->text(), ui.stations->currentText());
        }
    }
    double h_rad = DEG2RAD(h_deg);
    // 存入观测值（弧度）
    bool success = MW::InsertObservation(
        ui.workpieceName->currentText(),
        ui.piontname->text(),
        ui.stations->currentText(),
        h_rad, v, d,
        dateTime, t, press);
    if (success) {
        TOAST_TIP("定向点保存成功");
        pointName = Utils::SuffixAddOne(ui.piontname->text());
        ui.piontname->setText(pointName);
    } else {
        TOAST_TIP("定向点保存失败");
        return;
    }
}

std::optional<std::pair<Eigen::Vector3d, Eigen::Vector3d>> APILTM ::coordinateSystemTransform(QString name, Eigen::Vector3d point)
{
    // 解析"工件名/坐标系名"格式
    QStringList parts = name.split('/');
    if (parts.size() != 2) {
        TOAST_TIP("坐标系格式错误: " + name);
        return { std::nullopt };
    }
    qDebug() << "point点坐标系" << point.x() << point.y() << point.z();
    QString workpieceName = parts[0];
    QString sysName = parts[1];
    // 获取测站坐标系
    QString pointName = ui.stations->currentText();
    qDebug() << "测站名称：" << pointName;
    auto&& gt = MW::GetStnAxis(ui.stations->currentText());
    if (!gt.has_value()) {
        TOAST_TIP("获取测站坐标系失败");
        return { std::nullopt };
    }
    auto&& axis = gt.value();

    Eigen::Vector3d origin(axis.getX(), axis.getY(), axis.getZ());
    Eigen::Vector3d route(axis.getRx(), axis.getRy(), axis.getRz());
    Eigen::Vector3d re = Utils::Geometry::Fun::Rotate::RightTransform(point, route, origin);
    // 转换的子坐标系
    auto&& ga = MW::GetAxis(ui.workpieceName->currentText(), sysName);
    if (!ga.has_value()) {
        TOAST_TIP("获取坐标系失败");
        return { std::nullopt };
    }
    auto&& axis2 = ga.value();
    Eigen::Vector3d origin2(axis2.getX(), axis2.getY(), axis2.getZ());
    Eigen::Vector3d route2(axis2.getRx(), axis2.getRy(), axis2.getRz());
    Eigen::Vector3d result = Utils::Geometry::Fun::Rotate::RightReTransform(re, route2, origin2);
    return { { re, result } };
}

void APILTM::handleDynamicData(const QString& ip, const QString& name, const QString& type, TrackerFilter::TrackerPoint point)
{

    if (!isDynamicMeasuring)
        return;
    // 加锁保护数据
    QMutexLocker locker(&dataMutex);
    // 在异步任务前生成新点名
    QString currentPointName = Utils::SuffixAddOne(dyPointName);
    dyPointName = currentPointName;

    // 存储原始数据点
    latestPoint = point;
    hasNewData = true;
    // 异步处理坐标转换
    auto [x, y, z] = GetLkXYZR(point.v1, point.v2, point.v3);
    Eigen::Vector3d originalPoint(x, y, z);

    QtConcurrent::run([this, originalPoint, currentPointName, point]() {
        auto opt = coordinateSystemTransform(ui.coordinateSystem->currentText(), originalPoint);

        QMutexLocker locker(&dataMutex);
        if (opt.has_value()) {
            latestTransformedPoint = opt.value().second;
        } else {
            latestTransformedPoint = originalPoint;
        }

        // 在转换完成后保存数据
        if (ui.savaDyPoint->isChecked()) {
            QDateTime timestamp = point.time.isEmpty() ? QDateTime::currentDateTime()
                                                       : QDateTime::fromString(point.time, "yyyy-MM-dd hh:mm:ss.zzz");

            // 使用转换后的坐标
            double wx = latestTransformedPoint.x();
            double wy = latestTransformedPoint.y();
            double wz = latestTransformedPoint.z();
            if (sigleMeasureType == "定向点测量") {
                wx = point.v1;
                wy = point.v2;
                wz = point.v3;
            }

            dynamicDataList.append(QString("%1\t%2\t%3\t%4\t%5\n")
                                       .arg(currentPointName)
                                       .arg(F4(wx))
                                       .arg(F4(wy))
                                       .arg(F4(wz))
                                       .arg(timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz")));
        }
    });
}
void APILTM::updateCoordinateSystems(const QString& workpiece)
{
    auto coordinateSystems = MW::GetWorkpiecesAxis(workpiece);

    if (coordinateSystems.has_value()) {
        for (const auto& system : coordinateSystems.value()) {
            QJsonObject obj = system.toObject();
            qDebug() << obj;
            QString sysName = workpiece + "/" + obj["name"].toString();
            qDebug() << "坐标系名称：" << sysName;
            ui.coordinateSystem->addItem(sysName);
        }
    } else {
        TOAST_TIP("获取坐标系失败");
    }
}
void APILTM::updateUI()
{
    QMutexLocker locker(&dataMutex);

    if (!hasNewData)
        return;

    auto&& [s, v1, v2, v3, rx, ry, rz, p, px, py, pz, t, hum, press, time] = latestPoint;

    // 使用转换后的坐标
    double x = latestTransformedPoint.x();
    double y = latestTransformedPoint.y();
    double z = latestTransformedPoint.z();

    // 更新UI
    ui.piontname->setText(dyPointName);
    ui.X->setText(F3(x));
    ui.Y->setText(F3(y));
    ui.Z->setText(F3(z));
    ui.RMSX->setText(F3(px));
    ui.RMSY->setText(F3(py));
    ui.RMSZ->setText(F3(pz));
    ui.RMS->setText(F3(p));
    ui.tem->setText(QString::number(t));
    ui.press->setText(QString::number(press));

    double h_deg = RAD2DEG(v1);
    double v_deg = RAD2DEG(v2);
    h_deg = (h_deg < 0) ? h_deg + 360 : h_deg;
    ui.hz_value->setText(F3(h_deg));
    ui.v_value->setText(F3(v_deg));
    ui.dis_value->setText(F3(v3));

    hasNewData = false;
}