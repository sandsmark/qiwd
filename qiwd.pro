TEMPLATE = app
TARGET = qiwd

QT += dbus widgets

HEADERS += Iwd.h \
    ManagedObject.h \
    Window.h
SOURCES += Iwd.cpp main.cpp \
    Window.cpp
