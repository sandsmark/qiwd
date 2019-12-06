#ifndef IWD_H
#define IWD_H

#include <QObject>

#include "DbusInterface.h"
// Let's not get too carried away
using namespace net::connman;

class SignalLevelAgent;

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

signals:
    void knownNetworkAdded(const QString &name, const QString &id);
    void visibleNetworkAdded(const QString &name);
    void deviceAdded(const QString &name);

    void knownNetworkRemoved(const QString &name);
    void visibleNetworkRemoved(const QString &name);
    void deviceRemoved(const QString &name);

    void signalLevelChanged(const QString &networkName, int level);

private slots:
    void onManagedObjectsReceived(QDBusPendingCallWatcher *watcher);
    void onManagedObjectRemoved(const QDBusObjectPath &object_path, const QStringList &interfaces);
    void onManagedObjectAdded(const QDBusObjectPath &objectPath, const ManagedObject &object);

private:
    friend class SignalLevelAgent;
    void onSignalLevelChanged(const QDBusObjectPath &object, uint8_t signalLevel) {
        if (!m_networks.contains(object)) {
            return;
        }
        emit signalLevelChanged(m_networks[object]->name(), signalLevel);
    }

    template<class T>
    T* addObject(const QDBusObjectPath &path, QMap<QDBusObjectPath, QPointer<T>> &map, const QVariantMap &props) {
        if (map.contains(path)) {
            qWarning() << "Already has" << path.path();
            map.take(path)->deleteLater();
        }
        T *object = new T(m_iwd->service(), path.path(), m_iwd->connection(), this);
        map[path] = object;

        setProperties(object, props);

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
};

class SignalLevelAgent : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.iwd.SignalLevelAgent")

public:
    SignalLevelAgent(Iwd *parent);

public slots:
    QDBusVariant Changed(const QDBusObjectPath &object, uint8_t level) {
        m_iwd->onSignalLevelChanged(object, level);
        return QDBusVariant("");
    }
    Q_NOREPLY void Release(const QDBusObjectPath &object) {
        qDebug() << "deleting signal level listener" << object.path();
        deleteLater();
    }

    Iwd *m_iwd;
};

class AuthAgent : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.iwd.Agent")

public:
    AuthAgent(Iwd *parent);

public slots:
    /**
     * This method gets called when the service daemon
     * unregisters the agent. An agent can use it to do
     * cleanup tasks. There is no need to unregister the
     * agent, because when this method gets called it has
     * already been unregistered.
     */
    Q_NOREPLY void Release(const QDBusObjectPath &object) {
        qDebug() << "deleting auth agent" << object.path();
        deleteLater();
    }

    /**
     * This method gets called when trying to connect to a
     * network and passphrase is required.
     *
     * Possible Errors: net.connman.iwd.Agent.Error.Canceled
     */
    QDBusVariant RequestPassphrase(const QDBusObjectPath &network) {
        return QDBusVariant("TODO");
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
        return QDBusVariant("TODO");
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
        return QDBusVariant(QStringList({"TODO", "TODO"}));
    }

    /**
     * This method gets called when connecting to
     * a network that requires authentication with a
     * user password.  The user name is optionally passed
     * in the parameter.
     */
    QDBusVariant RequestUserPassword(const QDBusObjectPath &network, const QString &username) {
        return QDBusVariant("TODO");
    }

    /**
     * This method gets called to indicate that the agent
     * request failed before a reply was returned.  The
     * argument will indicate why the request is being
     * cancelled and may be "out-of-range", "user-canceled",
     * "timed-out" or "shutdown".
     */
    Q_NOREPLY void Cancel(const QString &reason) {
        qDebug() << "auth request was cancelled" << reason;
    }

    Iwd *m_iwd;
};


#endif // IWD_H
