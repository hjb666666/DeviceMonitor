# DeviceMonitor - Modbus TCP 多设备监控上位机

基于 Qt/C++ 开发的工业设备监控上位机，支持多设备同时采集、实时曲线显示、异常报警与钉钉推送。

## 功能特性

- 多设备管理：每个设备独立采集线程，同时连接多台设备
- Modbus TCP 协议：标准 MBAP + PDU，功能码 0x03
- 实时曲线：QPainter 自绘电压/电流/功率，带时间轴和鼠标悬停
- 异常报警：超限蜂鸣+变红，钉钉机器人 webhook 推送
- 断线自动重连、CSV 数据记录、QSettings 配置持久化

## 技术栈

C++ / Qt 5.8 / QThread / 信号-槽 / Modbus TCP / QPainter

## 作者

黄金彪 - 杭州电子科技大学 信息与通信工程 硕士
