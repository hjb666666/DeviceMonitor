#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QLabel>
#include <QCheckBox>
#include <QTimer>
#include <QSettings>
#include <QThread>
#include <QListWidget>
#include <QFile>
#include <QTextStream>
#include <QTime>
#include <QVector>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QLineEdit>
#include <QFormLayout>
#include <QDialogButtonBox>
#include "monitorworker.h"
#include "datacurve.h"

// 设备信息结构体
struct DeviceInfo {
    QString name;
    QString host;
    int port = 502;
    int unitId = 1;
    bool connected = false;
    QThread *thread = nullptr;
    MonitorWorker *worker = nullptr;
    QVector<quint16> lastData;
    int sampleCount = 0;
    QTime startTime;
    bool alarmActive = false;
    QTimer *simTimer = nullptr;
    double simSoc = 80.0;
    int voltAlarm = 550;
    int currAlarm = 120;
    bool notified95 = false;
    bool notifiedFull = false;
    QVector<double> voltageHistory;
    QVector<double> currentHistory;
    QVector<double> powerHistory;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onAddDevice();
    void onRemoveDevice();
    void onDeviceSelected(int row);
    void onConnectClicked();
    void onDisconnectClicked();
    void onDataReady(int deviceId, const QVector<quint16> &regs);
    void onLogMessage(int deviceId, const QString &msg);
    void onConnectionChanged(int deviceId, bool connected);

private:
    // 左侧设备列表
    QListWidget *m_deviceList;
    QPushButton *m_addBtn;
    QPushButton *m_removeBtn;
    QPushButton *m_connectBtn;
    QPushButton *m_disconnectBtn;

    // 连接参数区（对应当前选中设备）
    QLineEdit *m_nameEdit;
    QLineEdit *m_hostEdit;
    QSpinBox  *m_portSpin;
    QSpinBox  *m_unitSpin;
    QSpinBox  *m_voltAlarmSpin;
    QSpinBox  *m_currAlarmSpin;
    QCheckBox *m_autoLogCheck;
    QCheckBox *m_simulateCheck;

    // 钉钉
    QLineEdit *m_webhookEdit;
    QPushButton *m_testWebhookBtn;

    // 数据表格
    QTableWidget *m_table;

    // 曲线
    DataCurve *m_voltageCurve;
    DataCurve *m_currentCurve;
    DataCurve *m_powerCurve;

    // 日志
    QTextEdit *m_logEdit;
    QTableWidget *m_eventTable;  // 事件记录表

    // 系统托盘
    QSystemTrayIcon *m_trayIcon;

    // 状态栏
    QLabel *m_sampleCountLabel;
    QLabel *m_runTimeLabel;
    QLabel *m_alarmLabel;

    // 设备列表
    QList<DeviceInfo> m_devices;
    int m_currentDevice = -1;

    QSettings *m_settings;

    void setupUI();
    void loadSettings();
    void saveSettings();
    void appendLog(const QString &msg);
    void sendDingTalk(const QString &text);
    void updateDeviceList();
    void refreshCurrentDeviceUI();
};

#endif // MAINWINDOW_H
