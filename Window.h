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
        if (checkState() != other.checkState()) {
            return checkState() > other.checkState();
        }
        const bool isKnown = data(Qt::UserRole + 2).toBool();
        const bool otherKnown = other.data(Qt::UserRole + 2).toBool();
        if (isKnown != otherKnown) {
            return isKnown > otherKnown;
        }
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
    void onNetworkConnectedChanged(const QString &networkId, const bool connected);

    void onDeviceAdded(const QString &stationId, const QString &name);
    void onDeviceRemoved(const QString &stationId);
    void onDisconnectDevice();
    void onConnectDevice();

    void onVisibleNetworkRemoved(const QString &stationId, const QString &name);
    void onVisibleNetworkAdded(const QString &stationId, const QString &name, const bool connected, const bool isKnown);
    void onVisibleNetworkKnownChanged(const QString &id, const bool isKnown);

    void onStationSignalChanged(const QString &stationId, int newLevel);

    void onSelectionChanged();

    void onDeviceSelectionChanged();
    void onScanningChanged(const QString &station, bool isScanning);

    void onStationCurrentNetworkChanged(const QString &stationId, const QString &networkId);

private:
    QListWidget *m_knownNetworksList = nullptr;
    QListWidget *m_networkList = nullptr;
    QComboBox *m_deviceList = nullptr;

    Iwd m_iwd;
    QPointer<AuthUi> m_authUi;
    QPointer<SignalLevelAgent> m_signalAgent;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_scanButton = nullptr;

    QString m_currentNetworkId;
};

#endif // WINDOW_H
