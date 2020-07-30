#ifndef WINDOW_H
#define WINDOW_H

#include <QWidget>
#include <QPointer>

#include "Iwd.h"

class QListWidget;
class QComboBox;
class AuthUi;
class SignalLevelAgent;

class Window : public QWidget
{
    Q_OBJECT
public:
    explicit Window(QWidget *parent = nullptr);

private slots:
    void onKnownNetworkRemoved(const QString &networkId);
    void onKnownNetworkAdded(const QString &networkId, const QString &name);

    void onDeviceAdded(const QString &stationId, const QString &name);
    void onDeviceRemoved(const QString &stationId);
    void onDisconnectDevice();

    void onVisibleNetworkRemoved(const QString &name);
    void onVisibleNetworkAdded(const QString &name);

private:
    QListWidget *m_knownNetworksList = nullptr;
    QListWidget *m_networkList = nullptr;
    QComboBox *m_deviceList = nullptr;

    Iwd m_iwd;
    QPointer<AuthUi> m_authUi;
    QPointer<SignalLevelAgent> m_signalAgent;
};

#endif // WINDOW_H
