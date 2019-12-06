TEMPLATE = app
TARGET = qiwd

QT += dbus widgets

HEADERS += DbusInterface.h \
    Iwd.h \
    ManagedObject.h \
    Window.h
SOURCES += DbusInterface.cpp main.cpp \
    Iwd.cpp \
    Window.cpp
