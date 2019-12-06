#ifndef IWD_H
#define IWD_H

#include <QObject>

#include "DbusInterface.h"
// Let's not get too carried away
using namespace net::connman;

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

private slots:
    void onManagedObjectsReceived(QDBusPendingCallWatcher *watcher);
    void onManagedObjectRemoved(const QDBusObjectPath &object_path, const QStringList &interfaces);
    void onManagedObjectAdded(const QDBusObjectPath &objectPath, const ManagedObject &object);

private:
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

#endif // IWD_H
