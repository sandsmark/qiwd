#ifndef WINDOW_H
#define WINDOW_H

#include <QWidget>
#include <QPointer>
#include <QListWidgetItem>

#include "Iwd.h"

class QListWidget;
class QComboBox;
class QPushButton;
class AuthUi;
class SignalLevelAgent;

struct NetworkItem : public QListWidgetItem {
    using QListWidgetItem::QListWidgetItem;
    bool operator<(const QListWidgetItem &other) const override {
        return data(Qt::UserRole + 1).toInt() < other.data(Qt::UserRole + 1).toInt();
    }
};

class Window : public QWidget
{
    Q_OBJECT
public:
    explicit Window(QWidget *parent = nullptr);

private slots:
    void onKnownNetworkRemoved(const QString &networkId, const QString &name);
    void onKnownNetworkAdded(const QString &networkId, const QString &name);

    void onDeviceAdded(const QString &stationId, const QString &name);
    void onDeviceRemoved(const QString &stationId);
    void onDisconnectDevice();
    void onConnectDevice();

    void onVisibleNetworkRemoved(const QString &stationId, const QString &name);
    void onVisibleNetworkAdded(const QString &stationId, const QString &name);

    void onStationSignalChanged(const QString &stationId, int newLevel);

    void onSelectionChanged();

private:
    QListWidget *m_knownNetworksList = nullptr;
    QListWidget *m_networkList = nullptr;
    QComboBox *m_deviceList = nullptr;

    Iwd m_iwd;
    QPointer<AuthUi> m_authUi;
    QPointer<SignalLevelAgent> m_signalAgent;
    QPushButton *m_connectButton = nullptr;
};

#endif // WINDOW_H
