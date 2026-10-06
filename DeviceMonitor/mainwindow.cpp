#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QDateTime>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QTextCursor>
#include <QStatusBar>
#include <QInputDialog>
#include <QMessageBox>
#include <QProcess>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    m_settings = new QSettings("DeviceMonitor", "DeviceMonitor", this);
    setupUI();
    loadSettings();
}

MainWindow::~MainWindow()
{
    saveSettings();
    for (auto &dev : m_devices) {
        if (dev.simTimer) {
            dev.simTimer->stop();
            delete dev.simTimer;
            dev.simTimer = nullptr;
        }
        if (dev.thread) {
            dev.thread->quit();
            dev.thread->wait(2000);
            delete dev.thread;
            dev.thread = nullptr;
        }
    }
}

void MainWindow::setupUI()
{
    setWindowTitle("DeviceMonitor - Modbus TCP 多设备监控上位机");
    resize(1200, 750);

    auto *central = new QWidget;
    setCentralWidget(central);
    auto *mainLayout = new QHBoxLayout(central);

    // ===== 左侧设备列表 =====
    auto *leftGroup = new QGroupBox("设备列表");
    auto *leftLayout = new QVBoxLayout(leftGroup);
    m_deviceList = new QListWidget;
    leftLayout->addWidget(m_deviceList);

    auto *btnLayout = new QHBoxLayout;
    m_addBtn = new QPushButton("添加设备");
    m_removeBtn = new QPushButton("删除");
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_removeBtn);
    leftLayout->addLayout(btnLayout);

    m_connectBtn = new QPushButton("连接");
    m_disconnectBtn = new QPushButton("断开");
    m_disconnectBtn->setEnabled(false);
    auto *allBtn = new QPushButton("全部连接");
    leftLayout->addWidget(m_connectBtn);
    leftLayout->addWidget(m_disconnectBtn);
    leftLayout->addWidget(allBtn);

    connect(allBtn, &QPushButton::clicked, this, [this, allBtn](){
        bool anyConnected = false;
        for (auto &dev : m_devices) if (dev.connected) anyConnected = true;

        if (anyConnected) {
            // 全部断开
            for (int i = 0; i < m_devices.size(); i++) {
                if (m_devices[i].connected) {
                    m_currentDevice = i;
                    onDisconnectClicked();
                }
            }
            allBtn->setText("全部连接");
        } else {
            // 全部连接
            for (int i = 0; i < m_devices.size(); i++) {
                if (!m_devices[i].connected) {
                    m_currentDevice = i;
                    m_nameEdit->setText(m_devices[i].name);
                    m_hostEdit->setText(m_devices[i].host);
                    m_portSpin->setValue(m_devices[i].port);
                    m_unitSpin->setValue(m_devices[i].unitId);
                    onConnectClicked();
                }
            }
            allBtn->setText("全部断开");
        }
        updateDeviceList();
    });

    mainLayout->addWidget(leftGroup, 1);

    // ===== 右侧主区域 =====
    auto *rightLayout = new QVBoxLayout;

    // 连接参数区
    auto *connGroup = new QGroupBox("设备配置");
    auto *connLayout = new QGridLayout(connGroup);

    connLayout->addWidget(new QLabel("名称:"), 0, 0);
    m_nameEdit = new QLineEdit("设备1");
    connLayout->addWidget(m_nameEdit, 0, 1);

    connLayout->addWidget(new QLabel("IP:"), 0, 2);
    m_hostEdit = new QLineEdit("127.0.0.1");
    connLayout->addWidget(m_hostEdit, 0, 3);

    connLayout->addWidget(new QLabel("端口:"), 0, 4);
    m_portSpin = new QSpinBox;
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(502);
    connLayout->addWidget(m_portSpin, 0, 5);

    connLayout->addWidget(new QLabel("从站:"), 1, 0);
    m_unitSpin = new QSpinBox;
    m_unitSpin->setRange(1, 247);
    m_unitSpin->setValue(1);
    connLayout->addWidget(m_unitSpin, 1, 1);

    connLayout->addWidget(new QLabel("电压报警(V):"), 1, 2);
    m_voltAlarmSpin = new QSpinBox;
    m_voltAlarmSpin->setRange(100, 1000);
    m_voltAlarmSpin->setValue(550);
    connLayout->addWidget(m_voltAlarmSpin, 1, 3);

    connLayout->addWidget(new QLabel("电流报警(A):"), 1, 4);
    m_currAlarmSpin = new QSpinBox;
    m_currAlarmSpin->setRange(10, 300);
    m_currAlarmSpin->setValue(120);
    connLayout->addWidget(m_currAlarmSpin, 1, 5);

    m_autoLogCheck = new QCheckBox("自动记录CSV");
    m_autoLogCheck->setChecked(true);
    connLayout->addWidget(m_autoLogCheck, 1, 6);

    m_simulateCheck = new QCheckBox("模拟模式");
    m_simulateCheck->setStyleSheet("color: #d35400; font-weight: bold;");
    connLayout->addWidget(m_simulateCheck, 0, 6);

    rightLayout->addWidget(connGroup);

    // 钉钉配置
    auto *dingGroup = new QGroupBox("钉钉告警推送");
    auto *dingLayout = new QHBoxLayout(dingGroup);
    m_webhookEdit = new QLineEdit;
    m_webhookEdit->setPlaceholderText("钉钉机器人webhook地址");
    m_testWebhookBtn = new QPushButton("测试推送");
    dingLayout->addWidget(m_webhookEdit);
    dingLayout->addWidget(m_testWebhookBtn);
    rightLayout->addWidget(dingGroup);

    // 数据表格+曲线
    auto *midLayout = new QHBoxLayout;

    m_table = new QTableWidget(10, 4);
    m_table->setHorizontalHeaderLabels({"寄存器", "名称", "值", "单位"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setColumnWidth(0, 60);
    m_table->setColumnWidth(1, 100);
    m_table->setColumnWidth(2, 100);
    m_table->setColumnWidth(3, 60);
    midLayout->addWidget(m_table, 1);

    auto *curveLayout = new QVBoxLayout;
    m_voltageCurve = new DataCurve;
    m_voltageCurve->setTitle("充电电压(V)");
    m_voltageCurve->setLineColor(QColor("#2980b9"));
    m_voltageCurve->setYRange(0, 600);
    m_currentCurve = new DataCurve;
    m_currentCurve->setTitle("充电电流(A)");
    m_currentCurve->setLineColor(QColor("#27ae60"));
    m_currentCurve->setYRange(0, 150);
    m_powerCurve = new DataCurve;
    m_powerCurve->setTitle("充电功率(kW)");
    m_powerCurve->setLineColor(QColor("#c0392b"));
    m_powerCurve->setYRange(0, 100);
    curveLayout->addWidget(m_voltageCurve);
    curveLayout->addWidget(m_currentCurve);
    curveLayout->addWidget(m_powerCurve);
    midLayout->addLayout(curveLayout, 2);

    rightLayout->addLayout(midLayout, 3);

    // 事件记录表
    m_eventTable = new QTableWidget(0, 3);
    m_eventTable->setHorizontalHeaderLabels({"时间", "设备", "事件"});
    m_eventTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_eventTable->setMaximumHeight(80);
    rightLayout->addWidget(m_eventTable);

    // 日志
    m_logEdit = new QTextEdit;
    m_logEdit->setMaximumHeight(80);
    m_logEdit->setReadOnly(true);
    rightLayout->addWidget(m_logEdit);

    mainLayout->addLayout(rightLayout, 4);

    // 状态栏
    m_sampleCountLabel = new QLabel("采样: 0");
    m_runTimeLabel = new QLabel("运行: --:--:--");
    m_alarmLabel = new QLabel("正常");
    m_alarmLabel->setStyleSheet("color: green; font-weight: bold;");
    statusBar()->addWidget(m_sampleCountLabel);
    statusBar()->addWidget(m_runTimeLabel);
    statusBar()->addPermanentWidget(m_alarmLabel);

    // 信号槽
    connect(m_addBtn, &QPushButton::clicked, this, &MainWindow::onAddDevice);
    connect(m_removeBtn, &QPushButton::clicked, this, &MainWindow::onRemoveDevice);
    connect(m_deviceList, &QListWidget::currentRowChanged, this, &MainWindow::onDeviceSelected);
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(m_testWebhookBtn, &QPushButton::clicked, this, [this](){
        sendDingTalk("【测试】DeviceMonitor 告警推送已启用");
    });

    // 运行计时器
    QTimer *uiTimer = new QTimer(this);
    connect(uiTimer, &QTimer::timeout, this, [this](){
        if (m_currentDevice >= 0 && m_devices[m_currentDevice].sampleCount > 0) {
            qint64 secs = m_devices[m_currentDevice].startTime.elapsed() / 1000;
            m_runTimeLabel->setText(QString("运行: %1:%2:%3")
                .arg(secs/3600, 2, 10, QChar('0'))
                .arg((secs%3600)/60, 2, 10, QChar('0'))
                .arg(secs%60, 2, 10, QChar('0')));
        }
    });
    uiTimer->start(1000);

    // QSS样式美化
    setStyleSheet(R"(
        QGroupBox { font-weight: bold; color: #2c3e50; border: 1px solid #bdc3c7; border-radius: 5px; margin-top: 8px; padding-top: 10px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }
        QPushButton { background-color: #3498db; color: white; border: none; padding: 6px 16px; border-radius: 3px; }
        QPushButton:hover { background-color: #2980b9; }
        QPushButton:disabled { background-color: #bdc3c7; }
        QTableWidget { gridline-color: #ddd; alternate-background-color: #f5f7fa; }
        QHeaderView::section { background-color: #34495e; color: white; padding: 4px; border: none; }
        QListWidget::item { padding: 4px; }
        QTextEdit { background-color: #fafafa; }
    )");

    // 系统托盘
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(this->style()->standardIcon(QStyle::SP_MessageBoxInformation));
    auto *trayMenu = new QMenu(this);
    auto *showAction = trayMenu->addAction("显示");
    auto *quitAction = trayMenu->addAction("退出");
    connect(showAction, &QAction::triggered, this, &QWidget::showNormal);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
    m_trayIcon->setContextMenu(trayMenu);
    m_trayIcon->show();
}

void MainWindow::onAddDevice()
{
    QString name = QInputDialog::getText(this, "添加设备", "设备名称:", QLineEdit::Normal,
                                         QString("设备%1").arg(m_devices.size()+1));
    if (name.isEmpty()) return;

    DeviceInfo dev;
    dev.name = name;
    dev.host = "127.0.0.1";
    m_devices.append(dev);
    updateDeviceList();
    m_deviceList->setCurrentRow(m_devices.size() - 1);
}

void MainWindow::onRemoveDevice()
{
    int row = m_deviceList->currentRow();
    if (row < 0) return;
    auto &dev = m_devices[row];
    if (dev.connected) {
        QMessageBox::warning(this, "提示", "请先断开该设备再删除");
        return;
    }
    if (dev.simTimer) {
        dev.simTimer->stop();
        delete dev.simTimer;
        dev.simTimer = nullptr;
    }
    if (dev.thread) {
        dev.thread->quit();
        dev.thread->wait(1000);
        delete dev.thread;
    }
    m_devices.removeAt(row);
    if (m_currentDevice >= m_devices.size()) m_currentDevice = m_devices.size() - 1;
    updateDeviceList();
}

void MainWindow::updateDeviceList()
{
    int prevRow = m_currentDevice;
    m_deviceList->clear();
    int onlineCount = 0;
    for (int i = 0; i < m_devices.size(); i++) {
        if (m_devices[i].connected) onlineCount++;
        QString status = m_devices[i].connected ? "●在线" : "○离线";
        m_deviceList->addItem(QString("%1  [%2] %3").arg(i+1).arg(status, m_devices[i].name));
    }
    if (prevRow >= 0 && prevRow < m_devices.size()) {
        m_deviceList->setCurrentRow(prevRow);
    }
    setWindowTitle(QString("DeviceMonitor - Modbus TCP 上位机 (在线 %1/%2)").arg(onlineCount).arg(m_devices.size()));
}

void MainWindow::onDeviceSelected(int row)
{
    // 先保存当前设备的配置
    if (m_currentDevice >= 0 && m_currentDevice < m_devices.size()) {
        auto &oldDev = m_devices[m_currentDevice];
        oldDev.name = m_nameEdit->text();
        oldDev.host = m_hostEdit->text();
        oldDev.port = m_portSpin->value();
        oldDev.unitId = m_unitSpin->value();
        oldDev.voltAlarm = m_voltAlarmSpin->value();
        oldDev.currAlarm = m_currAlarmSpin->value();
    }

    m_currentDevice = row;
    if (row < 0) return;
    refreshCurrentDeviceUI();

    // 清空曲线和表格，加载选中设备的历史数据
    m_voltageCurve->clear();
    m_currentCurve->clear();
    m_powerCurve->clear();
    for (int i = 0; i < m_table->rowCount(); i++) {
        for (int j = 0; j < m_table->columnCount(); j++) {
            if (m_table->item(i, j)) m_table->item(i, j)->setText("");
        }
    }
    auto &dev = m_devices[row];
    // 加载历史曲线
    for (double v : dev.voltageHistory) m_voltageCurve->appendValue(v);
    for (double v : dev.currentHistory) m_currentCurve->appendValue(v);
    for (double v : dev.powerHistory) m_powerCurve->appendValue(v);
    if (!dev.lastData.isEmpty()) {
        onDataReady(row, dev.lastData);
    }
}

void MainWindow::refreshCurrentDeviceUI()
{
    if (m_currentDevice < 0) return;
    auto &dev = m_devices[m_currentDevice];
    m_nameEdit->setText(dev.name);
    m_hostEdit->setText(dev.host);
    m_portSpin->setValue(dev.port);
    m_unitSpin->setValue(dev.unitId);
    m_voltAlarmSpin->setValue(dev.voltAlarm);
    m_currAlarmSpin->setValue(dev.currAlarm);
    m_sampleCountLabel->setText("采样: " + QString::number(dev.sampleCount));
    m_connectBtn->setEnabled(!dev.connected);
    m_disconnectBtn->setEnabled(dev.connected);
}

void MainWindow::onConnectClicked()
{
    if (m_currentDevice < 0) return;
    auto &dev = m_devices[m_currentDevice];

    // 保存配置
    dev.name = m_nameEdit->text();
    dev.host = m_hostEdit->text();
    dev.port = m_portSpin->value();
    dev.unitId = m_unitSpin->value();

    dev.sampleCount = 0;
    dev.startTime = QTime::currentTime();
    dev.notified95 = false;
    dev.notifiedFull = false;
    m_voltageCurve->setStartTime(dev.startTime);
    m_currentCurve->setStartTime(dev.startTime);
    m_powerCurve->setStartTime(dev.startTime);

    // 模拟模式
    if (m_simulateCheck->isChecked()) {
        dev.connected = true;
        dev.simSoc = 80.0;
        dev.simTimer = new QTimer(this);
        int devId = m_currentDevice;
        connect(dev.simTimer, &QTimer::timeout, this, [this, devId](){
            if (devId < 0 || devId >= m_devices.size()) return;
            auto &d = m_devices[devId];
            QVector<quint16> regs;
            d.simSoc = qMin(100.0, d.simSoc + 0.05);

            double voltage, current;
            int chargeStatus = 1;  // 1=充电中
            if (d.simSoc >= 100.0) {
                // 充满：电流电压归零
                voltage = 0;
                current = 0;
                chargeStatus = 2;  // 2=充电完成
            } else if (d.simSoc >= 95.0) {
                // 即将充满：涓流，电流降到20A左右
                voltage = 495 + qrand()%5;
                current = 15 + qrand()%10;
            } else {
                // 正常快充
                voltage = 490 + qrand()%30;
                current = 95 + qrand()%20;
            }
            regs << (quint16)voltage;
            regs << (quint16)current;
            regs << (quint16)(voltage * current / 100);
            regs << (quint16)d.simSoc;
            regs << 35;
            regs << (quint16)chargeStatus;
            regs << 0;
            regs << 0 << 0 << 0;
            onDataReady(devId, regs);
        });
        dev.simTimer->start(500);
        updateDeviceList();
        m_connectBtn->setEnabled(false);
        m_disconnectBtn->setEnabled(true);
        appendLog(QString("[%1] 模拟模式启动").arg(dev.name));
        return;
    }

    dev.thread = new QThread;
    dev.worker = new MonitorWorker;
    dev.worker->moveToThread(dev.thread);

    int devId = m_currentDevice;
    connect(dev.thread, &QThread::started, dev.worker, [this, devId](){
        if (devId < 0 || devId >= m_devices.size()) return;
        auto &d = m_devices[devId];
        d.worker->start(d.host, d.port, d.unitId, 0, 10, 500);
    });
    connect(dev.worker, &MonitorWorker::dataReady, this, [this, devId](const QVector<quint16> &regs){
        onDataReady(devId, regs);
    });
    connect(dev.worker, &MonitorWorker::logMessage, this, [this, devId](const QString &msg){
        onLogMessage(devId, msg);
    });
    connect(dev.worker, &MonitorWorker::connectionChanged, this, [this, devId](bool connected){
        onConnectionChanged(devId, connected);
    });
    connect(dev.thread, &QThread::finished, dev.worker, &QObject::deleteLater);

    dev.thread->start();
    dev.sampleCount = 0;
    dev.startTime = QTime::currentTime();
    dev.notified95 = false;
    dev.notifiedFull = false;
    m_voltageCurve->setStartTime(dev.startTime);
    m_currentCurve->setStartTime(dev.startTime);
    m_powerCurve->setStartTime(dev.startTime);

    m_connectBtn->setEnabled(false);
    m_disconnectBtn->setEnabled(true);
    appendLog(QString("正在连接 %1 (%2:%3)...").arg(dev.name).arg(dev.host).arg(dev.port));
}

void MainWindow::onDisconnectClicked()
{
    if (m_currentDevice < 0) return;
    auto &dev = m_devices[m_currentDevice];

    // 停模拟模式
    if (dev.simTimer) {
        dev.simTimer->stop();
        delete dev.simTimer;
        dev.simTimer = nullptr;
        dev.connected = false;
        updateDeviceList();
    }

    if (dev.worker) {
        QMetaObject::invokeMethod(dev.worker, "stop", Qt::BlockingQueuedConnection);
    }
    if (dev.thread) {
        dev.thread->quit();
        dev.thread->wait(2000);
        delete dev.thread;
        dev.thread = nullptr;
        dev.worker = nullptr;
    }

    m_connectBtn->setEnabled(true);
    m_disconnectBtn->setEnabled(false);
    appendLog(QString("设备 %1 已断开").arg(dev.name));
}

void MainWindow::onDataReady(int devId, const QVector<quint16> &regs)
{
    if (devId < 0 || devId >= m_devices.size()) return;
    auto &dev = m_devices[devId];
    dev.lastData = regs;
    dev.sampleCount++;
    // 去掉每次采样的日志，只记事件

    // 保存每个设备的曲线历史
    double voltage = regs.size() > 0 ? regs.at(0) : 0;
    double current = regs.size() > 1 ? regs.at(1) : 0;
    double power = regs.size() > 2 ? regs.at(2) * 0.1 : 0;
    dev.voltageHistory.append(voltage);
    dev.currentHistory.append(current);
    dev.powerHistory.append(power);
    if (dev.voltageHistory.size() > 200) {
        dev.voltageHistory.removeFirst();
        dev.currentHistory.removeFirst();
        dev.powerHistory.removeFirst();
    }

    // 如果是当前选中设备，刷新UI
    if (devId == m_currentDevice) {

        m_sampleCountLabel->setText("采样: " + QString::number(dev.sampleCount));

        // 报警
        bool alarm = (voltage > dev.voltAlarm) || (current > dev.currAlarm);
        if (alarm && !dev.alarmActive) {
            dev.alarmActive = true;
            m_alarmLabel->setText(QString("报警! V=%1 I=%2").arg(voltage, 0, 'f', 0).arg(current, 0, 'f', 0));
            m_alarmLabel->setStyleSheet("color: white; background: red; font-weight: bold; padding: 2px 8px;");
            QApplication::beep();
            sendDingTalk(QString("【设备报警】%1 电压%2V 电流%3A 时间%4")
                .arg(dev.name).arg(voltage, 0, 'f', 0).arg(current, 0, 'f', 0)
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
        } else if (!alarm && dev.alarmActive) {
            dev.alarmActive = false;
            m_alarmLabel->setText("正常");
            m_alarmLabel->setStyleSheet("color: green; font-weight: bold;");
        }

        // SOC通知
        double soc = regs.size() > 3 ? regs.at(3) : 0;
        if (soc >= 95.0 && !dev.notified95) {
            dev.notified95 = true;
            appendLog(QString("[%1] SOC=%2%%，即将充满").arg(dev.name).arg(soc, 0, 'f', 0));
            sendDingTalk(QString("【设备报警】%1 电量已达%2%%，即将充满，请关注").arg(dev.name).arg(soc, 0, 'f', 0));
        }
        if (soc >= 100.0 && !dev.notifiedFull) {
            dev.notifiedFull = true;
            appendLog(QString("[%1] 充电完成").arg(dev.name));
            sendDingTalk(QString("【设备报警】%1 充电已完成，电流已断开").arg(dev.name));
        }

        // 更新表格
        QStringList names = {"电压", "电流", "功率", "SOC", "温度", "充电状态", "故障码", "预留", "预留", "预留"};
        for (int i = 0; i < qMin(regs.size(), 10); i++) {
            if (!m_table->item(i, 0)) m_table->setItem(i, 0, new QTableWidgetItem());
            if (!m_table->item(i, 1)) m_table->setItem(i, 1, new QTableWidgetItem());
            if (!m_table->item(i, 2)) m_table->setItem(i, 2, new QTableWidgetItem());
            if (!m_table->item(i, 3)) m_table->setItem(i, 3, new QTableWidgetItem());

            m_table->item(i, 0)->setText(QString::number(i));
            m_table->item(i, 1)->setText(names[i]);
            QString valStr, unitStr;
            bool isFault = false;
            if (i == 0) { valStr = QString::number(regs.at(i), 'f', 1); unitStr = "V"; }
            else if (i == 1) { valStr = QString::number(regs.at(i), 'f', 1); unitStr = "A"; }
            else if (i == 2) { valStr = QString::number(regs.at(i) * 0.1, 'f', 1); unitStr = "kW"; }
            else if (i == 3) { valStr = QString::number(regs.at(i)); unitStr = "%"; }
            else if (i == 4) { valStr = QString::number(regs.at(i)); unitStr = "℃"; }
            else if (i == 5) { valStr = regs.at(i) == 1 ? "充电中" : (regs.at(i) == 2 ? "充电完成" : "待机"); unitStr = "-"; }
            else if (i == 6) { isFault = regs.at(i) != 0; valStr = isFault ? "故障" : "正常"; unitStr = "-"; }
            else { valStr = "-"; unitStr = "-"; }
            m_table->item(i, 2)->setText(valStr);
            m_table->item(i, 3)->setText(unitStr);

            // 故障行变红
            QColor bgColor = isFault ? QColor(255, 200, 200) : QColor(255, 255, 255);
            for (int j = 0; j < 4; j++) {
                m_table->item(i, j)->setBackground(bgColor);
                m_table->item(i, j)->setTextAlignment(Qt::AlignCenter);
            }
        }

        // 更新曲线
        m_voltageCurve->appendValue(voltage);
        m_currentCurve->appendValue(current);
        m_powerCurve->appendValue(power);
    }
}

void MainWindow::onLogMessage(int devId, const QString &msg)
{
    if (devId < 0 || devId >= m_devices.size()) return;
    appendLog(QString("[%1] %2").arg(m_devices[devId].name, msg));
}

void MainWindow::onConnectionChanged(int devId, bool connected)
{
    if (devId < 0 || devId >= m_devices.size()) return;
    m_devices[devId].connected = connected;
    updateDeviceList();
    if (devId == m_currentDevice) {
        m_connectBtn->setEnabled(!connected);
        m_disconnectBtn->setEnabled(connected);
    }
    appendLog(QString("[%1] %2").arg(m_devices[devId].name, connected ? "已连接" : "连接断开"));
}

void MainWindow::appendLog(const QString &msg)
{
    QString line = QString("[%1] %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss")).arg(msg);
    m_logEdit->append(line);
    m_logEdit->moveCursor(QTextCursor::End);

    // 事件记录表
    int row = m_eventTable->rowCount();
    m_eventTable->insertRow(row);
    m_eventTable->setItem(row, 0, new QTableWidgetItem(QDateTime::currentDateTime().toString("MM-dd HH:mm:ss")));
    m_eventTable->setItem(row, 1, new QTableWidgetItem(m_currentDevice >= 0 ? m_devices[m_currentDevice].name : ""));
    m_eventTable->setItem(row, 2, new QTableWidgetItem(msg));
    m_eventTable->scrollToBottom();
}

void MainWindow::sendDingTalk(const QString &text)
{
    QString url = m_webhookEdit->text().trimmed();
    if (url.isEmpty()) return;

    QString body = QString("{\"msgtype\":\"text\",\"text\":{\"content\":\"%1\"}}").arg(text);
    QProcess *proc = new QProcess(this);
    proc->start("curl", QStringList() << "-s" << "-X" << "POST"
                << "-H" << "Content-Type: application/json"
                << "-d" << body << url);
    appendLog("告警已推送钉钉");
}

void MainWindow::loadSettings()
{
    m_webhookEdit->setText(m_settings->value("webhook", "").toString());
    m_voltAlarmSpin->setValue(m_settings->value("voltAlarm", 550).toInt());
    m_currAlarmSpin->setValue(m_settings->value("currAlarm", 120).toInt());

    // 加载设备列表
    int count = m_settings->beginReadArray("devices");
    for (int i = 0; i < count; i++) {
        m_settings->setArrayIndex(i);
        DeviceInfo dev;
        dev.name = m_settings->value("name", "设备").toString();
        dev.host = m_settings->value("host", "127.0.0.1").toString();
        dev.port = m_settings->value("port", 502).toInt();
        dev.unitId = m_settings->value("unitId", 1).toInt();
        m_devices.append(dev);
    }
    m_settings->endArray();
    // 默认添加一个设备
    if (m_devices.isEmpty()) {
        DeviceInfo dev;
        dev.name = "充电桩1";
        dev.host = "127.0.0.1";
        m_devices.append(dev);
    }
    updateDeviceList();
    if (!m_devices.isEmpty()) m_deviceList->setCurrentRow(0);
}

void MainWindow::saveSettings()
{
    m_settings->setValue("webhook", m_webhookEdit->text());
    m_settings->setValue("voltAlarm", m_voltAlarmSpin->value());
    m_settings->setValue("currAlarm", m_currAlarmSpin->value());

    m_settings->beginWriteArray("devices");
    for (int i = 0; i < m_devices.size(); i++) {
        m_settings->setArrayIndex(i);
        m_settings->setValue("name", m_devices[i].name);
        m_settings->setValue("host", m_devices[i].host);
        m_settings->setValue("port", m_devices[i].port);
        m_settings->setValue("unitId", m_devices[i].unitId);
    }
    m_settings->endArray();
}
