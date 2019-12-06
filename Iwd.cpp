#include "Iwd.h"

Iwd::Iwd(QObject *parent) : QObject(parent)
{
    m_iwd = new org::freedesktop::DBus::ObjectManager("net.connman.iwd", "/", QDBusConnection::systemBus(), this);
}

bool Iwd::init()
{
    if (!m_iwd->isValid()) {
        qWarning() << "Failed to connect to iwd";
        return false;
    }

    connect(m_iwd, &org::freedesktop::DBus::ObjectManager::InterfacesAdded, this, &Iwd::onManagedObjectAdded);
    connect(m_iwd, &org::freedesktop::DBus::ObjectManager::InterfacesRemoved, this, &Iwd::onManagedObjectRemoved);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(m_iwd->GetManagedObjects(), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &Iwd::onManagedObjectsReceived);
    return true;
}

void Iwd::onManagedObjectsReceived(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<ManagedObjectList> reply = *watcher;
    watcher->deleteLater();
    if (reply.isError()) {
        qDebug() << reply.error().name() << reply.error().message();
    }
    QMapIterator<QDBusObjectPath,ManagedObject> it(reply.value());
    while (it.hasNext()) {
        it.next();
        onManagedObjectAdded(it.key(), it.value());
    }
}

void Iwd::onManagedObjectRemoved(const QDBusObjectPath &object_path, const QStringList &interfaces)
{
    for (const QString &interfaceName : interfaces) {
        if (interfaceName == "org.freedesktop.DBus.Properties") {
            continue;
        }

        // todo refactor to not duplicate so much
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
            QPointer<iwd::Device> device = m_devices.take(object_path);
            emit deviceRemoved(device->name());
            device->deleteLater();
            continue;
        }
        if (interfaceName == iwd::Network::staticInterfaceName()) {
            if (!m_networks.contains(object_path)) {
                qWarning() << "No network with path" << object_path.path() << "registered";
                continue;
            }
            QPointer<iwd::Network> network = m_networks.take(object_path);
            emit visibleNetworkRemoved(network->name());
            network->deleteLater();
            continue;
        }

        if (interfaceName == iwd::KnownNetwork::staticInterfaceName()) {
            if (!m_knownNetworks.contains(object_path)) {
                qWarning() << "No known network with path" << object_path.path() << "registered";
                continue;
            }
            QPointer<iwd::KnownNetwork> network = m_knownNetworks.take(object_path);
            emit knownNetworkRemoved(network->name());
            network->deleteLater();
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
}

void Iwd::onManagedObjectAdded(const QDBusObjectPath &objectPath, const ManagedObject &object)
{
    if (objectPath.path() == "/") {
        return;
    }

    // We only support one interface type per object, FIXME if iwd ever changes
    for (const QString &interfaceName : object.keys()) {
        if (interfaceName == "org.freedesktop.DBus.Properties") { // generic that everything has
            continue;
        }

        const QVariantMap &props = object[interfaceName];
        if (interfaceName == iwd::Adapter::staticInterfaceName()) {
            addObject<iwd::Adapter>(objectPath, m_adapters, props);
            continue;
        }

        if (interfaceName == iwd::Device::staticInterfaceName()) {
            iwd::Device *device = addObject<iwd::Device>(objectPath, m_devices, props);
            emit deviceAdded(device->name());
            continue;
        }

        if (interfaceName == iwd::KnownNetwork::staticInterfaceName()) {
            iwd::KnownNetwork *network = addObject<iwd::KnownNetwork>(objectPath, m_knownNetworks, props);
            emit knownNetworkAdded(network->name());
            continue;
        }

        if (interfaceName == iwd::Network::staticInterfaceName()) {
            iwd::Network *network = addObject<iwd::Network>(objectPath, m_networks, props);
            emit visibleNetworkAdded(network->name());
            continue;
        }

        if (interfaceName == iwd::AgentManager::staticInterfaceName()) {
            addObject<iwd::AgentManager>(objectPath, m_agentManagers, props);
            continue;
        }

        if (interfaceName == iwd::Station::staticInterfaceName()) {
            addObject<iwd::Station>(objectPath, m_stations, props);
            continue;
        }

        if (interfaceName == iwd::SimpleConfiguration::staticInterfaceName()) {
            addObject<iwd::SimpleConfiguration>(objectPath, m_wps, props);
            continue;
        }

        qWarning() << "Unhandled interface" << interfaceName << object.keys() << objectPath.path();
    }
}

void Iwd::setProperties(QObject *object, const QVariantMap &properties)
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
