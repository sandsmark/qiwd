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

void Iwd::setAuthAgent(const QDBusObjectPath &agentPath)
{
    for (const QPointer<iwd::AgentManager> &manager : m_agentManagers) {
        qDebug() << "Registering auth agent" << agentPath << "for" << manager->path();
        manager->RegisterAgent(agentPath);
    }
    m_authAgent = agentPath;
}

void Iwd::setSignalAgent(const QDBusObjectPath &agentPath, const LevelsList &interestingLevels)
{
    for (const QPointer<iwd::Station> &station : m_stations) {
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(station->RegisterSignalLevelAgent(agentPath, interestingLevels));
        connect(watcher, &QDBusPendingCallWatcher::finished, this, &Iwd::onPendingCallComplete);
    }
    m_signalAgent = agentPath;
    m_interestingSignalLevels = interestingLevels;
}

void Iwd::disconnectStation(const QString &stationId)
{
    QDBusObjectPath dbusPath(stationId);
    if (!m_stations.contains(dbusPath)) {
        qWarning() << "Unknown station id" << stationId;
        return;
    }
    qDebug() << "Disconnecting" << stationId << m_devices[dbusPath]->name();

    m_stations[dbusPath]->Disconnect();
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
            emit deviceRemoved(device->path());
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
            emit knownNetworkRemoved(network->path());
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

        if (interfaceName == iwd::Station::staticInterfaceName()) {
            if (!m_stations.contains(object_path)) {
                qWarning() << "No known stations with path" << object_path.path() << "registered";
                continue;
            }
            m_stations.take(object_path)->deleteLater();
            continue;
        }

        if (interfaceName == iwd::SimpleConfiguration::staticInterfaceName()) {
            if (!m_wps.contains(object_path)) {
                qWarning() << "No known simple configuration with path" << object_path.path() << "registered";
                continue;
            }
            m_wps.take(object_path)->deleteLater();
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
            QPointer<iwd::Adapter> adapter = addObject<iwd::Adapter>(objectPath, m_adapters, props);
            //watchProperties(adapter);
            connect(adapter, &iwd::Adapter::propertiesChanged, this, &Iwd::onPropertiesChanged);
            continue;
        }

        if (interfaceName == iwd::Device::staticInterfaceName()) {
            qDebug() << "got interface" << "Props" << props;
            qDebug() << "adapter" << qvariant_cast<QDBusObjectPath>(props["Adapter"]).path();
            qDebug() << "path:" << objectPath.path();
            QString name = props.value("Name").toString();
            if (name.isEmpty()) {
                name = tr("Interface %1").arg(QString::number(m_devices.count() + 1));
            }
            iwd::Device *device = addObject<iwd::Device>(objectPath, m_devices, props);
            //watchProperties(device);
            connect(device, &iwd::Device::propertiesChanged, this, &Iwd::onPropertiesChanged);
            emit deviceAdded(device->path(), name);
            continue;
        }

        if (interfaceName == iwd::KnownNetwork::staticInterfaceName()) {
            iwd::KnownNetwork *network = addObject<iwd::KnownNetwork>(objectPath, m_knownNetworks, props);
            connect(network, &iwd::KnownNetwork::propertiesChanged, this, &Iwd::onPropertiesChanged);
            emit knownNetworkAdded(objectPath.path(), network->name());
            //watchProperties(network);
            continue;
        }

        if (interfaceName == iwd::Network::staticInterfaceName()) {
            iwd::Network *network = addObject<iwd::Network>(objectPath, m_networks, props);
            //watchProperties(network);
            connect(network, &iwd::Network::propertiesChanged, this, &Iwd::onPropertiesChanged);
            emit visibleNetworkAdded(network->name());
            continue;
        }

        if (interfaceName == iwd::AgentManager::staticInterfaceName()) {
            iwd::AgentManager *manager = addObject<iwd::AgentManager>(objectPath, m_agentManagers, props);
            if (!m_authAgent.path().isEmpty()) {
                QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(manager->RegisterAgent(m_authAgent));
                connect(watcher, &QDBusPendingCallWatcher::finished, this, &Iwd::onPendingCallComplete);
            }
            continue;
        }

        if (interfaceName == iwd::Station::staticInterfaceName()) {
            qDebug() << "Got station" << props;
            qDebug() << objectPath.path();
            QPointer<iwd::Station> station = addObject<iwd::Station>(objectPath, m_stations, props);
            connect(station, &iwd::Station::propertiesChanged, this, &Iwd::onPropertiesChanged);
            if (!m_signalAgent.path().isEmpty()) {
                QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(station->RegisterSignalLevelAgent(m_signalAgent, m_interestingSignalLevels));
                connect(watcher, &QDBusPendingCallWatcher::finished, this, &Iwd::onPendingCallComplete);
            }
            continue;
        }

        if (interfaceName == iwd::SimpleConfiguration::staticInterfaceName()) {
            QPointer<iwd::SimpleConfiguration> conf = addObject<iwd::SimpleConfiguration>(objectPath, m_wps, props);
            continue;
        }

        qWarning() << "Unhandled interface" << interfaceName << object.keys() << objectPath.path();
    }
}

void Iwd::onPendingCallComplete(QDBusPendingCallWatcher *call)
{
    if (call->isError() || call->error().type() != QDBusError::NoError) {
        qWarning() << "pending call failed" << call->error() << call->isError();
    }

    call->deleteLater();
}

void Iwd::onPropertiesChanged(const QMap<QString, QVariant> &changedProperties, const QStringList &invalidatedProperties)
//void Iwd::onPropertiesChanged()
{
//    qDebug() << sender();
    qDebug() << "properties changed" << sender() << changedProperties << invalidatedProperties;
//    if (interface_name == iwd::AgentManager::staticInterfaceName()) {
//        setProperties(m_in)
//    }

}

void Iwd::setProperties(QObject *object, const QVariantMap &properties)
{
    if (!object) {
        qWarning() << "No object" << properties;
        return;
    }

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

SignalLevelAgent::SignalLevelAgent(Iwd *parent) :
    QDBusAbstractAdaptor(parent)
{
    // Need a unique ID, best I could come up with
    m_path.setPath("/qiwd/signalagent/" + QString::number(std::uintptr_t(this)) + QString::number(QCoreApplication::applicationPid()));

    qDebug() << "Signal agent path" << m_path.path();
}

AuthAgent::AuthAgent(Iwd *parent) : QDBusAbstractAdaptor(parent),
    m_iwd(parent)
{
    // Need a unique ID, best I could come up with
    m_path.setPath("/qiwd/authagent/" + QString::number(std::uintptr_t(this)) + QString::number(QCoreApplication::applicationPid()));

    qDebug() << "Auth Agent path" << m_path << m_path.path();
}
