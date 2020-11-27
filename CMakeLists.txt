cmake_minimum_required(VERSION 3.5)
project(qiwd)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(${CMAKE_BINARY_DIR})

find_package(Qt5 REQUIRED COMPONENTS Core DBus Gui Widgets)

set_source_files_properties(dbusinterface.xml
    PROPERTIES
    INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/CustomTypes.h
    )

set(qiwd_SRCS
    main.cpp
    Iwd.cpp
    AuthUi.cpp
    Window.cpp

    CustomTypes.h
    )

qt5_add_dbus_interface(interface_SRCS
    dbusinterface.xml
    DbusInterface
    )

set(AUTOMOC ON) # Doesn't work for some reason
qt5_wrap_cpp(moc_SRCS
    AuthUi.h
    Iwd.h
    Window.h
    )

add_executable(qiwd
    ${qiwd_SRCS}
    ${interface_SRCS}
    ${moc_SRCS}
)

target_link_libraries(qiwd
    Qt5::Widgets
    Qt5::DBus
    )
