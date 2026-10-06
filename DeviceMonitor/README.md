# DeviceMonitor - Modbus TCP 多设备监控上位机

基于 Qt/C++ 开发的工业设备监控上位机，支持多设备同时采集、实时曲线显示、异常报警与钉钉推送。

## 功能特性

- **多设备管理**：左侧设备列表，每个设备独立采集线程，支持同时连接多台设备
- **Modbus TCP 协议**：标准 MBAP 头 + PDU 数据帧，功能码 0x03 读保持寄存器
- **实时数据可视化**：QPainter 自绘电压/电流/功率三条曲线，带时间轴和鼠标悬停数值
- **异常报警**：电压/电流超限触发蜂鸣 + 界面变红，通过钉钉机器人 webhook 推送告警到手机
- **断线自动重连**：网络断开 3 秒后自动恢复连接
- **数据记录**：CSV 自动存储历史数据，配置项 QSettings 持久化
- **系统托盘**：最小化后台运行，不占用任务栏

## 技术栈

- **语言**：C++
- **框架**：Qt 5.8（QWidget、信号-槽、QThread）
- **协议**：Modbus TCP、TCP/UDP Socket
- **数据**：CSV 文件、QSettings 配置持久化
- **推送**：钉钉群机器人 webhook（curl 命令行）

## 项目结构

```
DeviceMonitor/
├── main.cpp              # 程序入口
├── mainwindow.h/cpp      # 主窗口 UI
├── modbusclient.h/cpp    # Modbus TCP 协议封装
├── monitorworker.h/cpp   # 采集工作线程（QThread）
├── datacurve.h/cpp       # 自绘实时曲线控件
├── DeviceMonitor.pro      # qmake 工程文件
└── README.md
```

## 如何运行

1. 用 Qt Creator 打开 `DeviceMonitor.pro`
2. 选择 Kit（Desktop Qt 5.8 MinGW 32bit）
3. 编译运行
4. 勾选"模拟模式"即可无设备演示

## 模拟模式

无需真实 Modbus 设备，勾选"模拟模式"后自动生成电压/电流/功率/SOC 数据：
- SOC < 95%：正常快充 ~500V / ~100A
- SOC ≥ 95%：涓流充电，电流降至 ~20A
- SOC = 100%：充电完成，电流归零

## 钉钉告警配置

1. 钉钉群 → 群设置 → 机器人 → 添加自定义机器人
2. 安全设置关键词填"设备报警"
3. 复制 webhook 地址粘贴到程序里
4. 报警时自动推送消息到群

## 作者

黄金彪 - 杭州电子科技大学 信息与通信工程 硕士
