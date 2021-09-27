#ifndef IWD_H
#define IWD_H

#include <QObject>
#include <QDBusPendingCall>

#include "DbusInterface.h"
// Let's not get too carried away
using namespace net::connman;

Q_DECLARE_LOGGING_CATEGORY(iwdLog)

class SignalLevelAgent;
class AuthAgent;

class Iwd : public QObject
{
    Q_OBJECT
public:
    explicit Iwd(QObject *parent = nullptr);

    bool init();

    QString networkName(const QString &id) {
        QDBusObjectPath path(id);
        if (!m_networks.contains(path)) {
            return QString();
        }
        return m_networks[path]->name();
    }

    QString networkId(const QString &name) {
        // TODO: match on the last part of the object path ID
        QMapIterator<QDBusObjectPath, QPointer<iwd::Network>> it(m_networks);
        while (it.hasNext()) {
            it.next();
            if (it.value()->name() == name) {
                return it.value()->path();
            }
        }
        qCDebug(iwdLog) << "Failed to find" << name;

        return QString();
    }
    bool isScanning(const QString &device) {
        QPointer<iwd::Station> station = m_stations.value(QDBusObjectPath(device));
        if (!station) {
            return false;
        }
        return station->scanning();
    }

    void setAuthAgent(const QDBusObjectPath &agentPath);

    void setSignalAgent(const QDBusObjectPath &agentPath, const LevelsList &interestingLevels);

public slots:
    void connectNetwork(const QString &networkId);
    void disconnectStation(const QString &stationId);
    void scan();
    void setKnownNetworkEnabled(const QString &id, const bool enabled);

signals:
    void knownNetworkAdded(const QString &id, const QString &name, const bool enabled);
    void visibleNetworkAdded(const QString &id, const QString &name, const bool connected, const bool isKnown);
    void deviceAdded(const QString &stationId, const QString &name);

    void knownNetworkRemoved(const QString &id, const QString &name);
    void visibleNetworkRemoved(const QString &id, const QString &name);
    void deviceRemoved(const QString &name);

    void signalLevelChanged(const QString &station, int level);
    void stationScanningChanged(const QString &station, bool scanning);

    void stationStateChanged(const QString &stationId, const QString &state);
    void stationCurrentNetworkChanged(const QString &stationId, const QString &networkId);
    void networkConnectedChanged(const QString &networkId, bool connected);
    void visibleNetworkKnownChanged(const QString &id, const bool isKnown);
    void knownNetworkEnabledChanged(const QString &id, const bool enabled);

private slots:
    void onManagedObjectsReceived(QDBusPendingCallWatcher *watcher);
    void onManagedObjectRemoved(const QDBusObjectPath &object_path, const QStringList &interfaces);
    void onManagedObjectAdded(const QDBusObjectPath &objectPath, const ManagedObject &object);

    void onPendingCallComplete(QDBusPendingCallWatcher *call);
    void onGetOrderedNetworksReply(QDBusPendingCallWatcher *watcher);

//    void onPropertiesChanged();
    void onPropertiesChanged(QDBusAbstractInterface *intf, const QString &interfaceName, const QVariantMap &changedProperties,
                            const QStringList& invalidatedProperties);

private:
    void watchProperties(QDBusAbstractInterface *intf) {
        org::freedesktop::DBus::Properties *props = new org::freedesktop::DBus::Properties(intf->service(), intf->path(), intf->connection(), intf);
        connect(props, &org::freedesktop::DBus::Properties::PropertiesChanged, intf, [=](const QString &interfaceName, const QVariantMap &changedProperties, const QStringList& invalidatedProperties) {
            if (interfaceName != intf->interface()) {
                return;
            }
            onPropertiesChanged(intf, interfaceName, changedProperties, invalidatedProperties);
        });
    }
    template<class T>
    T* addObject(const QDBusObjectPath &path, QMap<QDBusObjectPath, QPointer<T>> &map, const QVariantMap &props) {
        if (map.contains(path)) {
            qCWarning(iwdLog) << "Already has" << path.path();
            map.take(path)->deleteLater();
        }
        T *object = new T(m_iwd->service(), path.path(), m_iwd->connection(), this);
        map[path] = object;

        setProperties(object, props);
        watchProperties(object);

        return object;
    }

    void setProperties(QObject *object, const QVariantMap &properties);

    QPointer<org::freedesktop::DBus::ObjectManager> m_iwd;

    QMap<QDBusObjectPath, QPointer<iwd::Adapter>> m_adapters;
    QMap<QDBusObjectPath, QPointer<iwd::Device>> m_devices;
    QMap<QDBusObjectPath, QPointer<iwd::AgentManager>> m_agentManagers;
    QMap<QDBusObjectPath, QPointer<iwd::Network>> m_networks;
    QMap<QDBusObjectPath, QPointer<iwd::KnownNetwork>> m_knownNetworks;
    QMap<QDBusObjectPath, QPointer<iwd::Station>> m_stations;
    QMap<QDBusObjectPath, QPointer<iwd::SimpleConfiguration>> m_wps;

    QDBusObjectPath m_authAgent;
    QDBusObjectPath m_signalAgent;
    LevelsList m_interestingSignalLevels;
};

class SignalLevelAgent : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.iwd.SignalLevelAgent")

public:
    SignalLevelAgent(Iwd *parent);
    QDBusObjectPath objectPath() const { return m_path; }

signals:
    void signalLevelChanged(const QString &stationId, int newLevel);

public slots:
    QDBusVariant Changed(const QDBusObjectPath &object, uchar level) {
        qCDebug(iwdLog) << "Signal changed" << level;
        emit signalLevelChanged(object.path(), level);
        return QDBusVariant("");
    }
    Q_NOREPLY void Release(const QDBusObjectPath &object) {
        qCDebug(iwdLog) << "deleting signal level listener" << object.path();
        deleteLater();
    }

private:
    Iwd *m_iwd;
    QDBusObjectPath  m_path;
};

class AuthAgent : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.iwd.Agent")

public:
    AuthAgent(Iwd *parent);

    QDBusObjectPath objectPath() const { return m_path; }

protected:
    virtual QString onRequestPrivateKeyPassphrase(const QString &networkId) = 0;
    virtual QString onRequestPassphrase(const QString &networkId) = 0;
    virtual QPair<QString, QString> onRequestUsernameAndPassword(const QString &networkId) = 0;
    virtual QString onRequestUserPassword(const QString &username, const QString &networkId) = 0;

signals:
    void authenticationCancelled(const QString &reason);
    void released(const QString &networkId);

public slots:
    /**
     * This method gets called when the service daemon
     * unregisters the agent. An agent can use it to do
     * cleanup tasks. There is no need to unregister the
     * agent, because when this method gets called it has
     * already been unregistered.
     */
    Q_NOREPLY void Release(const QDBusObjectPath &object) {
        emit released(object.path());
    }

    /**
     * This method gets called when trying to connect to a
     * network and passphrase is required.
     *
     * Possible Errors: net.connman.iwd.Agent.Error.Canceled
     */
    QDBusVariant RequestPassphrase(const QDBusObjectPath &network) {
        return QDBusVariant(onRequestPassphrase(network.path()));
    }

    /**
     * This method gets called when connecting to
     * a network that requires authentication using a
     * locally-stored encrypted private key file, to
     * obtain that private key's encryption passphrase.
     *
     * Possible Errors: net.connman.iwd.Agent.Error.Canceled
     *
     * @return a string
     */
    QDBusVariant RequestPrivateKeyPassphrase(const QDBusObjectPath &network) {
        return QDBusVariant(onRequestPrivateKeyPassphrase(network.path()));
    }

    /**
     * This method gets called when connecting to
     * a network that requires authentication using a
     * user name and password.
     *
     * Possible Errors: net.connman.iwd.Agent.Error.Canceled
     *
     * @return two strings
     */
    QDBusVariant RequestUserNameAndPassword(const QDBusObjectPath &network) {
        const QPair<QString, QString> usernameAndPassword = onRequestUsernameAndPassword(network.path());
        return QDBusVariant(QStringList({usernameAndPassword.first, usernameAndPassword.second}));
    }

    /**
     * This method gets called when connecting to
     * a network that requires authentication with a
     * user password.  The user name is optionally passed
     * in the parameter.
     */
    QDBusVariant RequestUserPassword(const QDBusObjectPath &network, const QString &username) {
        return QDBusVariant(onRequestUserPassword(username, network.path()));
    }

    /**
     * This method gets called to indicate that the agent
     * request failed before a reply was returned.  The
     * argument will indicate why the request is being
     * cancelled and may be "out-of-range", "user-canceled",
     * "timed-out" or "shutdown".
     */
    Q_NOREPLY void Cancel(const QString &reason) {
        emit authenticationCancelled(reason);
    }

protected:
    Iwd *m_iwd;

private:
    QDBusObjectPath m_path;
};

#endif // IWD_H
