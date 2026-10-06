QT       += core gui network widgets

CONFIG += c++17

TARGET = DeviceMonitor
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    modbusclient.cpp \
    monitorworker.cpp \
    datacurve.cpp

HEADERS += \
    mainwindow.h \
    modbusclient.h \
    monitorworker.h \
    datacurve.h
