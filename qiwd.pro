TEMPLATE = app
TARGET = qiwd

QT += dbus widgets

HEADERS += DbusInterface.h \
    AuthUi.h \
    Iwd.h \
    CustomTypes.h \
    Window.h
SOURCES += DbusInterface.cpp main.cpp \
    AuthUi.cpp \
    Iwd.cpp \
    Window.cpp
