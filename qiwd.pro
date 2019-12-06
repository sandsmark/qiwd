TEMPLATE = app
TARGET = qiwd

QT += dbus widgets

HEADERS += DbusInterface.h \
    ManagedObject.h \
    Window.h
SOURCES += DbusInterface.cpp main.cpp \
    Window.cpp
