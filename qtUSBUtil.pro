QT += core
QT -= gui

CONFIG += c++11

TARGET = qtUSBUtil
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp

LIBS    += -lsetupapi -lwinusb
