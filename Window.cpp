#include "Window.h"

#include <QDBusConnection>
#include <QVBoxLayout>
#include <QComboBox>
#include <QListWidget>

Window::Window(QWidget *parent) : QWidget(parent)
{
    setLayout(new QVBoxLayout);
    m_deviceList = new QComboBox;
    m_networkList = new QListWidget;
    m_knownNetworksList = new QListWidget;

    layout()->addWidget(m_deviceList);
    layout()->addWidget(m_networkList);
    layout()->addWidget(m_knownNetworksList);

    m_iwd = new org::freedesktop::DBus::ObjectManager("net.connman.iwd", "/", QDBusConnection::systemBus(), this);
    if (!m_iwd->isValid()) {
        qWarning() << "Failed to connect to iwd";
        return;
    }

//    connect(m_iwd, &org::freedesktop::DBus::ObjectManager::InterfacesAdded, this, &Window::onManagedObjectAdded);
    connect(m_iwd, &org::freedesktop::DBus::ObjectManager::InterfacesAdded, this, [=](const QDBusObjectPath &object, const ManagedObject &interface){
        // just a hack until we do updating the ui properly
        onManagedObjectAdded(object, interface);
        updateUi();
    });
    connect(m_iwd, &org::freedesktop::DBus::ObjectManager::InterfacesRemoved, this, &Window::onManagedObjectRemoved);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(m_iwd->GetManagedObjects(), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &Window::onManagedObjectsReceived);
}

void Window::onManagedObjectsReceived(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<ManagedObjectList> reply = *watcher;
    if (reply.isError()) {
        qDebug() << reply.error().name() << reply.error().message();
    }
    QMapIterator<QDBusObjectPath,ManagedObject> it(reply.value());
    while (it.hasNext()) {
        it.next();
        onManagedObjectAdded(it.key(), it.value());
    }

    updateUi();
}

void Window::onManagedObjectRemoved(const QDBusObjectPath &object_path, const QStringList &interfaces)
{
    for (const QString &interfaceName : interfaces) {
        if (interfaceName == "org.freedesktop.DBus.Properties") {
            continue;
        }
        if (interfaceName == iwd::Adapter::staticInterfaceName()) {
            if (!m_adapters.contains(object_path)) {
                qWarning() << "No adapter with path" << object_path.path() << "registered";
                continue;
            }
            m_adapters.take(object_path)->deleteLater();
            continue;
        }
        if (interfaceName == iwd::Device::staticInterfaceName()) {
            if (!m_devices.contains(object_path)) {
                qWarning() << "No device with path" << object_path.path() << "registered";
                continue;
            }
            m_devices.take(object_path)->deleteLater();
            continue;
        }
        if (interfaceName == iwd::Network::staticInterfaceName()) {
            if (!m_networks.contains(object_path)) {
                qWarning() << "No network with path" << object_path.path() << "registered";
                continue;
            }
            m_networks.take(object_path)->deleteLater();
            continue;
        }

        if (interfaceName == iwd::KnownNetwork::staticInterfaceName()) {
            if (!m_knownNetworks.contains(object_path)) {
                qWarning() << "No known network with path" << object_path.path() << "registered";
                continue;
            }
            m_knownNetworks.take(object_path)->deleteLater();
            continue;
        }

        if (interfaceName == iwd::AgentManager::staticInterfaceName()) {
            if (!m_agentManagers.contains(object_path)) {
                qWarning() << "No known agent manager with path" << object_path.path() << "registered";
                continue;
            }
            m_agentManagers.take(object_path)->deleteLater();
            continue;
        }
    }

    updateUi();
}

void Window::onManagedObjectAdded(const QDBusObjectPath &object, const ManagedObject &interface)
{
    if (object.path() == "/") {
        return;
    }

    // We only support one interface type per object, FIXME if iwd ever changes
    for (const QString &interfaceName : interface.keys()) {
        if (interfaceName == "org.freedesktop.DBus.Properties") {
            continue;
        }

        if (interfaceName == iwd::Adapter::staticInterfaceName()) {
            setProperties(addAdapter(object), interface[interfaceName]);
            return;
        }

        if (interfaceName == iwd::Device::staticInterfaceName()) {
            setProperties(addDevice(object), interface[interfaceName]);
            return;
        }

        if (interfaceName == iwd::KnownNetwork::staticInterfaceName()) {
            setProperties(addKnownNetwork(object), interface[interfaceName]);
            return;
        }

        if (interfaceName == iwd::Network::staticInterfaceName()) {
            setProperties(addNetwork(object), interface[interfaceName]);
            return;
        }
        if (interfaceName == iwd::AgentManager::staticInterfaceName()) {
            setProperties(addAgentManager(object), interface[interfaceName]);
            return;
        }
    }

    qDebug() << object.path();
    qWarning() << "Unknown object" << interface.keys() << object.path();
}

void Window::updateUi()
{
    m_deviceList->clear();
    for (const QPointer<iwd::Device> device : m_devices.values()) {
        m_deviceList->addItem(device->name());
    }

    m_networkList->clear();
    for (const QPointer<iwd::Network> network : m_networks.values()) {
        m_networkList->addItem(network->name());
    }

    m_knownNetworksList->clear();
    for (const QPointer<iwd::KnownNetwork> network : m_knownNetworks.values()) {
        m_knownNetworksList->addItem(network->name());
    }
}

QObject *Window::addAdapter(const QDBusObjectPath &object)
{
    if (m_adapters.contains(object)) {
        qWarning() << "Duplicate adapter object!" << object.path();
        m_adapters[object]->deleteLater();
    }

    m_adapters[object] = new iwd::Adapter(m_iwd->service(), object.path(), m_iwd->connection(), this);
    qDebug() << "Added adapter" << m_adapters[object]->name();
    return m_adapters[object];
}

QObject *Window::addDevice(const QDBusObjectPath &object)
{
    if (m_devices.contains(object)) {
        qWarning() << "Duplicate device object!" << object.path();
        m_devices[object]->deleteLater();
    }

    m_devices[object] = new iwd::Device(m_iwd->service(), object.path(), m_iwd->connection(), this);
    qDebug() << "Added device" << m_devices[object]->name();
    return m_devices[object];
}

QObject *Window::addKnownNetwork(const QDBusObjectPath &object)
{
    if (m_knownNetworks.contains(object)) {
        qWarning() << "Duplicate known network object!" << object.path();
        m_knownNetworks[object]->deleteLater();
    }

    m_knownNetworks[object] = new iwd::KnownNetwork(m_iwd->service(), object.path(), m_iwd->connection(), this);
    return m_knownNetworks[object];

}

QObject *Window::addNetwork(const QDBusObjectPath &object)
{
    if (m_networks.contains(object)) {
        qWarning() << "Duplicate network object!" << object.path();
        m_networks[object]->deleteLater();
    }

    m_networks[object] = new iwd::Network(m_iwd->service(), object.path(), m_iwd->connection(), this);
    return m_networks[object];
}

QObject *Window::addAgentManager(const QDBusObjectPath &object)
{
    if (m_agentManagers.contains(object)) {
        qWarning() << "Duplicate agent manager object!" << object.path();
        m_agentManagers[object]->deleteLater();
    }

    m_agentManagers[object] = new iwd::AgentManager(m_iwd->service(), object.path(), m_iwd->connection(), this);
    return m_agentManagers[object];
}

void Window::setProperties(QObject *object, const QVariantMap &properties)
{
    QMapIterator<QString, QVariant> it(properties);
    while (it.hasNext()) {
        it.next();
        const char *propertyName = it.key().toLatin1().constData();
        if (object->metaObject()->indexOfProperty(propertyName) == -1) {
            qWarning() << "Invalid property name" << it.key();
            continue;
        }
        object->setProperty(propertyName, it.value());
    }
}
