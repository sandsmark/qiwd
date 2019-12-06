#include <QApplication>
#include <QDBusMetaType>

#include "Window.h"
#include "ManagedObject.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qRegisterMetaType<ManagedObject>("ManagedObject");
    qDBusRegisterMetaType<ManagedObject>();
    qRegisterMetaType<ManagedObjectList>("ManagedObjectList");
    qDBusRegisterMetaType<ManagedObjectList>();

    Window window;
    window.show();

    return app.exec();
}
