"""
Modbus TCP 从站模拟器 —— 用于测试 DeviceMonitor 上位机
用法：
  1. pip install pymodbus==2.5.3
  2. python modbus_simulator.py
  3. 在 DeviceMonitor 里连接 127.0.0.1:502，起始地址 0，数量 10
"""
from pymodbus.server.sync import StartTcpServer
from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext
import threading
import time
import random

# 初始寄存器值（地址 0~9）
store = ModbusSlaveContext(
    hr=ModbusSequentialDataBlock(0, [512, 120, 65, 80, 35, 1, 0, 0, 0, 0])
)
context = ModbusServerContext(slaves=store, single=True)

def update_registers():
    """每 500ms 模拟电压/电流/功率波动"""
    while True:
        hr = context[0].getValues(3, 0, count=10)  # 3 = 保持寄存器(hr)
        hr[0] = 500 + random.randint(-10, 10)   # 电压 500V 左右波动
        hr[1] = 100 + random.randint(-15, 15)   # 电流 100A 左右波动
        hr[2] = hr[0] * hr[1] // 100             # 功率 = 电压*电流/100 (kW)
        hr[3] = min(100, hr[3] + random.randint(0, 1))  # SOC 缓慢上升
        context[0].setValues(3, 0, hr)
        time.sleep(0.5)

if __name__ == "__main__":
    print("Modbus TCP 模拟器启动: 127.0.0.1:502")
    print("寄存器 0=电压(V) 1=电流(A) 2=功率(0.1kW) 3=SOC(%)")
    print("按 Ctrl+C 停止")
    t = threading.Thread(target=update_registers, daemon=True)
    t.start()
    StartTcpServer(context, address=("0.0.0.0", 502))
