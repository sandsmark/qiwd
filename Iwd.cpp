#include "Iwd.h"

Q_LOGGING_CATEGORY(iwdLog, "iwd.daemon", QtInfoMsg)

Iwd::Iwd(QObject *parent) : QObject(parent)
{
    m_iwd = new org::freedesktop::DBus::ObjectManager("net.connman.iwd", "/", QDBusConnection::systemBus(), this);
}

bool Iwd::init()
{
    if (!m_iwd->isValid()) {
        qCWarning(iwdLog) << "Failed to connect to iwd";
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
        qCDebug(iwdLog) << "Registering auth agent" << agentPath << "for" << manager->path();
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

void Iwd::connectNetwork(const QString &networkId)
{
    if (networkId.isEmpty()) {
        qCWarning(iwdLog) << "Can't connect to empty network id";
        return;
    }
    QPointer<iwd::Network> net = m_networks.value(QDBusObjectPath(networkId));
    if (!net) {
        qCWarning(iwdLog) << "Unknown network" << networkId;
        return;
    }

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(net->Connect());
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &Iwd::onPendingCallComplete);
}

void Iwd::disconnectStation(const QString &stationId)
{
    QDBusObjectPath dbusPath(stationId);
    if (!m_stations.contains(dbusPath)) {
        qCWarning(iwdLog) << "Unknown station id" << stationId;
        return;
    }
    qCDebug(iwdLog) << "Disconnecting" << stationId << m_devices[dbusPath]->name();

    m_stations[dbusPath]->Disconnect();
}

void Iwd::setKnownNetworkEnabled(const QString &networkId, const bool enabled)
{
    QDBusObjectPath dbusPath(networkId);
    if (!m_knownNetworks.contains(dbusPath)) {
        qCWarning(iwdLog) << "Unknown network id" << networkId;
        return;
    }
    qCDebug(iwdLog) << "Setting" << networkId << "as enabled?:" << enabled;

    m_knownNetworks[dbusPath]->setAutoConnect(enabled);
}

void Iwd::scan()
{
    QMapIterator<QDBusObjectPath, QPointer<iwd::Station>> it(m_stations);
    while (it.hasNext()) {
        it.next();
        qCDebug(iwdLog) << "Requesting scan for" << it.value()->path();
        it.value()->Scan();
    }
}

void Iwd::onManagedObjectsReceived(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<ManagedObjectList> reply = *watcher;
    if (reply.isError()) {
        qCDebug(iwdLog) << reply.error().name() << reply.error().message();
    }
    QMapIterator<QDBusObjectPath,ManagedObject> it(reply.value());
    while (it.hasNext()) {
        it.next();
        onManagedObjectAdded(it.key(), it.value());
    }
    QMapIterator<QDBusObjectPath, QPointer<iwd::Station>> netIterator(m_stations);
    while (netIterator.hasNext()) {
        netIterator.next();
        qCDebug(iwdLog) << it.value();
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(netIterator.value()->GetOrderedNetworks());
        connect(watcher, &QDBusPendingCallWatcher::finished, this, &Iwd::onGetOrderedNetworksReply);
    }
    watcher->deleteLater();
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
                qCWarning(iwdLog) << "No adapter with path" << object_path.path() << "registered";
                continue;
            }
            m_adapters.take(object_path)->deleteLater();
            continue;
        }
        if (interfaceName == iwd::Device::staticInterfaceName()) {
            if (!m_devices.contains(object_path)) {
                qCWarning(iwdLog) << "No device with path" << object_path.path() << "registered";
                continue;
            }
            QPointer<iwd::Device> device = m_devices.take(object_path);
            emit deviceRemoved(device->path());
            device->deleteLater();
            continue;
        }
        if (interfaceName == iwd::Network::staticInterfaceName()) {
            if (!m_networks.contains(object_path)) {
                qCWarning(iwdLog) << "No network with path" << object_path.path() << "registered";
                continue;
            }
            QPointer<iwd::Network> network = m_networks.take(object_path);
            emit visibleNetworkRemoved(network->path(), network->name());
            network->deleteLater();
            continue;
        }

        if (interfaceName == iwd::KnownNetwork::staticInterfaceName()) {
            if (!m_knownNetworks.contains(object_path)) {
                qCWarning(iwdLog) << "No known network with path" << object_path.path() << "registered";
                continue;
            }
            QPointer<iwd::KnownNetwork> network = m_knownNetworks.take(object_path);
            emit knownNetworkRemoved(network->path(), network->name());
            network->deleteLater();
            continue;
        }

        if (interfaceName == iwd::AgentManager::staticInterfaceName()) {
            if (!m_agentManagers.contains(object_path)) {
                qCWarning(iwdLog) << "No known agent manager with path" << object_path.path() << "registered";
                continue;
            }
            m_agentManagers.take(object_path)->deleteLater();
            continue;
        }

        if (interfaceName == iwd::Station::staticInterfaceName()) {
            if (!m_stations.contains(object_path)) {
                qCWarning(iwdLog) << "No known stations with path" << object_path.path() << "registered";
                continue;
            }
            m_stations.take(object_path)->deleteLater();
            continue;
        }

        if (interfaceName == iwd::SimpleConfiguration::staticInterfaceName()) {
            if (!m_wps.contains(object_path)) {
                qCWarning(iwdLog) << "No known simple configuration with path" << object_path.path() << "registered";
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
            continue;
        }

        if (interfaceName == iwd::Device::staticInterfaceName()) {
            qCDebug(iwdLog) << "got interface" << "Props" << props;
            qCDebug(iwdLog) << "adapter" << qvariant_cast<QDBusObjectPath>(props["Adapter"]).path();
            qCDebug(iwdLog) << "path:" << objectPath.path();
            QString name = props.value("Name").toString();
            if (name.isEmpty()) {
                name = tr("Interface %1").arg(QString::number(m_devices.count() + 1));
            }
            iwd::Device *device = addObject<iwd::Device>(objectPath, m_devices, props);
            //watchProperties(device);
//            connect(device, &iwd::Device::propertiesChanged, this, &Iwd::onPropertiesChanged);
            emit deviceAdded(device->path(), name);
            continue;
        }

        if (interfaceName == iwd::KnownNetwork::staticInterfaceName()) {
            iwd::KnownNetwork *network = addObject<iwd::KnownNetwork>(objectPath, m_knownNetworks, props);
//            connect(network, &iwd::KnownNetwork::propertiesChanged, this, &Iwd::onPropertiesChanged);
            emit knownNetworkAdded(objectPath.path(), network->name(), network->autoConnect());
            //watchProperties(network);
            continue;
        }

        if (interfaceName == iwd::Network::staticInterfaceName()) {
            iwd::Network *network = addObject<iwd::Network>(objectPath, m_networks, props);
            //watchProperties(network);
//            connect(network, &iwd::Network::propertiesChanged, this, &Iwd::onPropertiesChanged);
            emit visibleNetworkAdded(network->path(), network->name(), network->connected(), !network->knownNetwork().path().isEmpty());
            qCDebug(iwdLog) << "Visible network known network" << network->knownNetwork().path();
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
            qCDebug(iwdLog) << "Got station" << props;
            qCDebug(iwdLog) << objectPath.path();
            QPointer<iwd::Station> station = addObject<iwd::Station>(objectPath, m_stations, props);
            if (!m_signalAgent.path().isEmpty()) {
                QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(station->RegisterSignalLevelAgent(m_signalAgent, m_interestingSignalLevels));
                connect(watcher, &QDBusPendingCallWatcher::finished, this, &Iwd::onPendingCallComplete);
            }
            if (props.contains("ConnectedNetwork")) {
                emit stationCurrentNetworkChanged(station->path(), qvariant_cast<QDBusObjectPath>(props["ConnectedNetwork"]).path());
            }
            if (props.contains("State")) {
                qCDebug(iwdLog) << "State:" << props["State"];
                emit stationStateChanged(station->path(), props["State"].toString());
            }
            continue;
        }

        if (interfaceName == iwd::SimpleConfiguration::staticInterfaceName()) {
            QPointer<iwd::SimpleConfiguration> conf = addObject<iwd::SimpleConfiguration>(objectPath, m_wps, props);
            continue;
        }

        qCDebug(iwdLog) << "Unhandled interface" << interfaceName << object.keys() << objectPath.path();
    }
}

void Iwd::onPendingCallComplete(QDBusPendingCallWatcher *call)
{
    if (call->isError() || call->error().type() != QDBusError::NoError) {
        qCWarning(iwdLog) << "pending call failed" << call->error() << call->isError();
    }

    call->deleteLater();
}

void Iwd::onGetOrderedNetworksReply(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<OrderedNetworkList> reply = *watcher;
    if (!reply.isError()) {
        for (const QPair<QDBusObjectPath,int> &network : reply.value()) {
            qCDebug(iwdLog) << "signal level for" << network.first.path() << network.second;
            emit signalLevelChanged(network.first.path(), network.second);
        }
    } else {
        qCWarning(iwdLog) << "Error calling get ordered networks" << reply.error().message();
    }
    watcher->deleteLater();
}

void Iwd::onPropertiesChanged(QDBusAbstractInterface *intf, const QString &interfaceName, const QVariantMap &changedProperties, const QStringList &invalidatedProperties)
{
    setProperties(intf, changedProperties);
    qCDebug(iwdLog) << "properties changed" << intf << interfaceName << changedProperties << invalidatedProperties;

    if (interfaceName == iwd::Station::staticInterfaceName()) {
        iwd::Station *station = qobject_cast<iwd::Station*>(intf);
        if (!station) {
            qCWarning(iwdLog) << "Not station after all?";
            return;
        }

        if (changedProperties.contains("Scanning")) {
            const bool isScanning = changedProperties["Scanning"].toBool();

            if (!isScanning) {
                // Scanning finished, need to get updated list of signal strengths
                QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(station->GetOrderedNetworks());
                connect(watcher, &QDBusPendingCallWatcher::finished, this, &Iwd::onGetOrderedNetworksReply);
            }

            emit stationScanningChanged(intf->path(), isScanning);
        }
        if (changedProperties.contains("ConnectedNetwork")) {
            const QString network = qvariant_cast<QDBusObjectPath>(changedProperties["ConnectedNetwork"]).path();
            qCDebug(iwdLog) << "Connected changed" << network;
            emit stationCurrentNetworkChanged(station->path(), network);
        }

        if (changedProperties.contains("State")) {
            const QString state = changedProperties["State"].toString();
            emit stationStateChanged(intf->path(), state);
        }
    } else if (interfaceName == iwd::Network::staticInterfaceName()) {
        if (changedProperties.contains("LastConnectedTime")) {
            qCDebug(iwdLog) << "Last connected changed";
        }
        if (changedProperties.contains("Connected")) {
            const bool connected = changedProperties["Connected"].toBool();
            emit networkConnectedChanged(intf->path(), connected);
        }
        if (changedProperties.contains("KnownNetwork")) {
            const QString knownPath = qvariant_cast<QDBusObjectPath>(changedProperties["KnownNetwork"]).path();
            emit visibleNetworkKnownChanged(intf->path(), !knownPath.isEmpty());
        }
    } else if (interfaceName == iwd::KnownNetwork::staticInterfaceName()) {
        if (changedProperties.contains("AutoConnect")) {
            emit knownNetworkEnabledChanged(intf->path(), changedProperties["AutoConnect"].toBool());
        }
    } else {
        qCWarning(iwdLog) << "Unhandled property change";
    }
}

void Iwd::setProperties(QObject *object, const QVariantMap &properties)
{
    if (!object) {
        qCWarning(iwdLog) << "No object" << properties;
        return;
    }

    QMapIterator<QString, QVariant> it(properties);
    while (it.hasNext()) {
        it.next();
        const char *propertyName = it.key().toLatin1().constData();
        if (object->metaObject()->indexOfProperty(propertyName) == -1) {
            qCWarning(iwdLog) << "Invalid property name" << it.key() << "for" << object;
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

    qCDebug(iwdLog) << "Signal agent path" << m_path.path();
}

AuthAgent::AuthAgent(Iwd *parent) : QDBusAbstractAdaptor(parent),
    m_iwd(parent)
{
    // Need a unique ID, best I could come up with
    m_path.setPath("/qiwd/authagent/" + QString::number(std::uintptr_t(this)) + QString::number(QCoreApplication::applicationPid()));

    qCDebug(iwdLog) << "Auth Agent path" << m_path << m_path.path();
}
