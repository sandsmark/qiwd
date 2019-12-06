#ifndef WINDOW_H
#define WINDOW_H

#include <QWidget>
#include <QPointer>

#include "Iwd.h"
// Let's not get too carried away
using namespace net::connman;

class QListWidget;
class QComboBox;

class Window : public QWidget
{
    Q_OBJECT
public:
    explicit Window(QWidget *parent = nullptr);

private slots:
    void onManagedObjectsReceived(QDBusPendingCallWatcher *watcher);
    void onManagedObjectRemoved(const QDBusObjectPath &object_path, const QStringList &interfaces);
    void onManagedObjectAdded(const QDBusObjectPath &object, const ManagedObject &interface);

private:
    void updateUi(); // todo fancy signals and stuff when stuff is discovered

    QObject *addAdapter(const QDBusObjectPath &object);
    QObject *addDevice(const QDBusObjectPath &object);
    QObject *addKnownNetwork(const QDBusObjectPath &object);
    QObject *addNetwork(const QDBusObjectPath &object);
    QObject *addAgentManager(const QDBusObjectPath &object);
    void setProperties(QObject *object, const QVariantMap &properties);

    QPointer<org::freedesktop::DBus::ObjectManager> m_iwd;

    QMap<QDBusObjectPath, QPointer<iwd::Adapter>> m_adapters;
    QMap<QDBusObjectPath, QPointer<iwd::Device>> m_devices;
    QMap<QDBusObjectPath, QPointer<iwd::AgentManager>> m_agentManagers;
    QMap<QDBusObjectPath, QPointer<iwd::Network>> m_networks;
    QMap<QDBusObjectPath, QPointer<iwd::KnownNetwork>> m_knownNetworks;

    QListWidget *m_knownNetworksList = nullptr;
    QListWidget *m_networkList = nullptr;
    QComboBox *m_deviceList = nullptr;

//    QMap<QDBusObjectPath, QPointer<iwd::SimpleConfiguration>> m_wps;
    // To support making iwd act as an access point
//    QMap<QDBusObjectPath, QPointer<iwd::Station>> m_stations;
};

#endif // WINDOW_H
