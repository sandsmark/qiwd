#include "Window.h"

#include "AuthUi.h"

#include <QDBusConnection>
#include <QVBoxLayout>
#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>

void Window::onKnownNetworkRemoved(const QString &networkId, const QString &name)
{
    qDebug() << "known network removed" << networkId << name;

    // ugly, but don't know a better way
    bool found = false;
    do {
        for (int i=0; i<m_knownNetworksList->count(); i++) {
            QListWidgetItem *net = m_knownNetworksList->item(i);
            if (net->data(Qt::UserRole).toString() != networkId) {
                continue;
            }
            delete m_knownNetworksList->takeItem(i);
            found = true;
            break;
        }
    } while(found);
}

void Window::onKnownNetworkAdded(const QString &networkId, const QString &name)
{
    qDebug() << "known network added" << networkId << name;
    QListWidgetItem *item = new NetworkItem(QIcon::fromTheme("network-wireless-disconnected"), name);
    item->setData(Qt::UserRole, networkId);
    item->setData(Qt::UserRole + 1, 100);
    m_knownNetworksList->addItem(item);
}

void Window::onDeviceAdded(const QString &stationId, const QString &name)
{
    m_deviceList->addItem(name, stationId);
}

void Window::onDeviceRemoved(const QString &stationId)
{
    for (int i=0; i<m_deviceList->count(); i++) {
        if (m_deviceList->itemData(i) == stationId) {
            m_deviceList->removeItem(i);
        }
    }
}

void Window::onDisconnectDevice()
{
    m_iwd.disconnectStation(m_deviceList->currentData().toString());
}

void Window::onConnectDevice()
{
    QList<QListWidgetItem*> selected = m_networkList->selectedItems();
    QString id;
    if (!selected.isEmpty()) {
        qDebug() << "no generic selected";
        id = selected.first()->data(Qt::UserRole).toString();
    } else {
        selected = m_knownNetworksList->selectedItems();
        if (selected.isEmpty()) {
            qDebug() << "No known selected";
            return;
        }
        id = m_iwd.networkId(selected.first()->text());
    }
    if (id.isEmpty()) {
        qWarning() << "No network selected";
        return;
    }
    m_iwd.connectNetwork(id);


}

void Window::onVisibleNetworkRemoved(const QString &stationId, const QString &name)
{
    // ugly, but don't know a better way
    bool found = false;
    do {
        for (int i=0; i<m_networkList->count(); i++) {
            QListWidgetItem *net = m_networkList->item(i);
            if (net->data(Qt::UserRole).toString() != stationId) {
                continue;
            }
            delete m_networkList->takeItem(i);
            found = true;
            break;
        }
    } while(found);
}

void Window::onVisibleNetworkAdded(const QString &stationId, const QString &name)
{
    QListWidgetItem *item = new NetworkItem(QIcon::fromTheme("network-wireless-symbolic"), name);
    item->setData(Qt::UserRole, stationId);
    item->setData(Qt::UserRole + 1, -1000);
    m_networkList->addItem(item);
}

void Window::onStationSignalChanged(const QString &stationId, int newLevel)
{
    float strength = newLevel / 100.;
    if (strength == 0) { // special case in QtBluetooth apparently
        strength = 0.f;
    } else  if(strength <= -100) {
        strength = 0.f;
    } else if(strength >= -50) {
        strength = -100.f;
    } else {
        strength = -2.f * (strength/100. + 1.f);
    }
    strength *= -100;
    qDebug() << "New level for" << m_iwd.networkName(stationId) << strength;


    QIcon signalIcon;
    // Meh, should have better thresholds
    if (strength > 90) {
        signalIcon = QIcon::fromTheme("network-wireless-signal-excellent");
    } else if (strength > 70) {
        signalIcon = QIcon::fromTheme("network-wireless-signal-good");
    } else if (strength > 50) {
        signalIcon = QIcon::fromTheme("network-wireless-signal-ok");
    } else if (strength > 20) {
        signalIcon = QIcon::fromTheme("network-wireless-signal-weak");
    } else  {
        signalIcon = QIcon::fromTheme("network-wireless-signal-none");
    }
    int found = 0;
    for (int i=0; i<m_networkList->count(); i++) {
        QListWidgetItem *net = m_networkList->item(i);
        if (net->data(Qt::UserRole).toString() != stationId) {
            continue;
        }
        found++;
        net->setIcon(signalIcon);
        net->setData(Qt::UserRole+1, -strength);
    }

    for (QListWidgetItem *net : m_knownNetworksList->findItems(m_iwd.networkName(stationId), Qt::MatchExactly)) {
        net->setIcon(signalIcon);
        net->setData(Qt::UserRole+1, -strength);
    }

    m_networkList->sortItems();
    m_knownNetworksList->sortItems();
    if (!found) {
        qDebug() << "Failed to find" << stationId;
    } else if (found > 1) {
        qDebug() << "Found too many:" << stationId;
    }

}

void Window::onSelectionChanged()
{
    m_connectButton->setEnabled(!m_knownNetworksList->selectedItems().isEmpty() || !m_networkList->selectedItems().isEmpty());
}

Window::Window(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout;
    setLayout(mainLayout);
    m_networkList = new QListWidget;
    m_knownNetworksList = new QListWidget;

    QHBoxLayout *devicesLayout = new QHBoxLayout;
    m_deviceList = new QComboBox;
    QPushButton *disconnectButton = new QPushButton(tr("Disconnect"));
    m_connectButton = new QPushButton(tr("Connect"));
    m_connectButton->setEnabled(false);

    devicesLayout->addWidget(m_deviceList);
    devicesLayout->addWidget(disconnectButton);
    devicesLayout->addStretch();
    devicesLayout->addWidget(m_connectButton);

    mainLayout->addLayout(devicesLayout);
    mainLayout->addWidget(m_networkList);
    mainLayout->addWidget(m_knownNetworksList);

    connect(m_networkList, &QListWidget::itemClicked, m_knownNetworksList, &QListWidget::clearSelection);
    connect(m_knownNetworksList, &QListWidget::itemClicked, m_networkList, &QListWidget::clearSelection);

    connect(m_networkList, &QListWidget::itemSelectionChanged, this, &Window::onSelectionChanged);
    connect(m_knownNetworksList, &QListWidget::itemSelectionChanged, this, &Window::onSelectionChanged);

    connect(disconnectButton, &QPushButton::clicked, this, &Window::onDisconnectDevice);
    connect(m_connectButton, &QPushButton::clicked, this, &Window::onConnectDevice);

    connect(&m_iwd, &Iwd::visibleNetworkAdded, this, &Window::onVisibleNetworkAdded);
    connect(&m_iwd, &Iwd::visibleNetworkRemoved, this, &Window::onVisibleNetworkRemoved);
    connect(&m_iwd, &Iwd::knownNetworkAdded, this, &Window::onKnownNetworkAdded);
    connect(&m_iwd, &Iwd::knownNetworkRemoved, this, &Window::onKnownNetworkRemoved);
    connect(&m_iwd, &Iwd::deviceAdded, this, &Window::onDeviceAdded);
    connect(&m_iwd, &Iwd::deviceRemoved, this, &Window::onDeviceRemoved);
    connect(&m_iwd, &Iwd::signalLevelChanged, this, &Window::onStationSignalChanged);

    m_signalAgent = new SignalLevelAgent(&m_iwd);
    if (QDBusConnection::systemBus().registerObject(m_signalAgent->objectPath().path(), this)) {
        m_iwd.setSignalAgent(m_signalAgent->objectPath(), {-20, -40, -49, -50, -51, -60, -80});
        qDebug() << "set signal agent";
    } else {
        qWarning() << "Failed to register signal agent";
    }
    connect(m_signalAgent, &SignalLevelAgent::signalLevelChanged, this, &Window::onStationSignalChanged);

    m_authUi = new AuthUi(&m_iwd);
    if (QDBusConnection::systemBus().registerObject(m_authUi->objectPath().path(), this)) {
        m_iwd.setAuthAgent(m_authUi->objectPath());
    } else {
        qWarning() << "Failed to register auth agent";
    }

    m_iwd.init();
}
