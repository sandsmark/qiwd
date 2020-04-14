#include <QApplication>
#include <QDBusMetaType>

#include "Window.h"
#include "CustomTypes.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qRegisterMetaType<ManagedObject>("ManagedObject");
    qDBusRegisterMetaType<ManagedObject>();

    qRegisterMetaType<ManagedObjectList>("ManagedObjectList");
    qDBusRegisterMetaType<ManagedObjectList>();

    qRegisterMetaType<OrderedNetworkList>("OrderedNetworkList");
    qDBusRegisterMetaType<OrderedNetworkList>();

    qRegisterMetaType<LevelsList>("LevelsList");
    qDBusRegisterMetaType<LevelsList>();

    Window window;
    window.show();

    return app.exec();
}
