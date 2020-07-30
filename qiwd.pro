TEMPLATE = app
TARGET = qiwd

QT += dbus widgets

HEADERS += DbusInterface.h \
    AuthUi.h \
    Iwd.h \
    CustomTypes.h \
    Window.h \
dbusabstractinterface.h
SOURCES += DbusInterface.cpp main.cpp \
    AuthUi.cpp \
    Iwd.cpp \
    dbusabstractinterface.cpp \
    Window.cpp
