#include "Window.h"

#include "AuthUi.h"

#include <QDBusConnection>
#include <QVBoxLayout>
#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QSystemTrayIcon>

Q_LOGGING_CATEGORY(windowLog, "iwd.window", QtInfoMsg)

void Window::onKnownNetworkRemoved(const QString &networkId, const QString &name)
{
    qCDebug(windowLog) << "known network removed" << networkId << name;

    // ugly, but don't know a better way
    bool found = false;
    do {
        found = false;
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
    m_knownNetworksList->sortItems();
}

void Window::onKnownNetworkAdded(const QString &networkId, const QString &name, const bool enabled)
{
    qCDebug(windowLog) << "known network added" << networkId << name;
    QListWidgetItem *item = new NetworkItem(QIcon::fromTheme("network-wireless-disconnected"), name);
    item->setData(Qt::UserRole, networkId);
    item->setData(Qt::UserRole + 1, 100);
    item->setData(Qt::UserRole + 2, enabled);
    item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    m_knownNetworksList->addItem(item);
    m_knownNetworksList->sortItems();
}

void Window::onKnownNetworkEnabledChanged(const QString &networkId, const bool enabled)
{
    for (int i=0; i<m_knownNetworksList->count(); i++) {
        QListWidgetItem *net = m_knownNetworksList->item(i);
        if (net->data(Qt::UserRole).toString() != networkId) {
            continue;
        }
        net->setData(Qt::UserRole + 2, enabled);
        net->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    }
}

void Window::onKnownNetworkCheckedChanged(QListWidgetItem *item)
{
    const QString networkId = item->data(Qt::UserRole).toString();
    if (networkId.isEmpty()) {
        qCWarning(windowLog) << "Missing network id in" << item;
        return;
    }

    // Don't know of any better way to see if it was the user who clicked
    const bool netEnabled = item->data(Qt::UserRole + 2).toBool();
    const bool checked = item->checkState() == Qt::Checked;
    if (netEnabled == checked) {
        return;
    }

    m_iwd.setKnownNetworkEnabled(networkId, checked);
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

void Window::onDeviceStateChanged(const QString &stationId, const QString &state)
{
    if (m_deviceList->currentData().toString() != stationId) {
        return;
    }
    m_deviceStateLabel->setText(state);
}

void Window::onStationCurrentNetworkChanged(const QString &stationId, const QString &networkId)
{
    if (m_deviceList->currentData().toString() != stationId) {
        return;
    }
    m_currentNetworkId = networkId;
    qCDebug(windowLog) << "current network" << m_currentNetworkId;
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
        qCDebug(windowLog) << "no generic selected";
        id = selected.first()->data(Qt::UserRole).toString();
    } else {
        selected = m_knownNetworksList->selectedItems();
        if (selected.isEmpty()) {
            qCDebug(windowLog) << "No known selected";
            return;
        }
        id = m_iwd.networkId(selected.first()->text());
    }
    if (id.isEmpty()) {
        qCWarning(windowLog) << "No network selected";
        return;
    }
    m_iwd.connectNetwork(id);


}

void Window::onVisibleNetworkRemoved(const QString &stationId, const QString &name)
{
    qCDebug(windowLog) << "Visible network removed" << stationId << name;
    // ugly, but don't know a better way
    bool found = false;
    do {
        found = false;
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
    m_networkList->sortItems();
}

void Window::onVisibleNetworkKnownChanged(const QString &networkId, const bool isKnown)
{
    for (int i=0; i<m_networkList->count(); i++) {
        QListWidgetItem *net = m_networkList->item(i);
        if (net->data(Qt::UserRole).toString() != networkId) {
            continue;
        }
        net->setData(Qt::UserRole + 2, isKnown);
        QFont font = net->font();
        font.setItalic(!isKnown);
        net->setFont(font);
        return;
    }
}

void Window::onNetworkConnectedChanged(const QString &networkId, const bool connected)
{
    for (int i=0; i<m_networkList->count(); i++) {
        QListWidgetItem *net = m_networkList->item(i);
        if (net->data(Qt::UserRole).toString() != networkId) {
            continue;
        }

        QFont font = net->font();
        font.setBold(connected);
        net->setFont(font);
        net->setData(Qt::UserRole + 3, connected);
        return;
    }
}

void Window::onVisibleNetworkAdded(const QString &stationId, const QString &name, const bool connected, const bool isKnown)
{
    qCDebug(windowLog) << "Visible network added" << stationId << name;
    QListWidgetItem *item = new NetworkItem(QIcon::fromTheme("network-wireless-symbolic"), name);
    item->setData(Qt::UserRole, stationId);
    item->setData(Qt::UserRole + 1, -1000);
    item->setData(Qt::UserRole + 2, isKnown);
    item->setData(Qt::UserRole + 3, connected);

    QFont font = item->font();
    font.setItalic(!isKnown);
    font.setBold(connected);
    item->setFont(font);

    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    m_networkList->addItem(item);
    m_networkList->sortItems();
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
    qCDebug(windowLog) << "New level for" << m_iwd.networkName(stationId) << strength;


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
    if (stationId == m_currentNetworkId) {
        qCDebug(windowLog) << "Updating tray";
        m_tray->setIcon(signalIcon);
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
        qCDebug(windowLog) << "Failed to find" << stationId;
    } else if (found > 1) {
        qCDebug(windowLog) << "Found too many:" << stationId;
    }

}

void Window::onSelectionChanged()
{
    m_connectButton->setEnabled(!m_knownNetworksList->selectedItems().isEmpty() || !m_networkList->selectedItems().isEmpty());
}

void Window::onNetworkDoubleClicked(QListWidgetItem *item)
{
    const QString networkId = item->data(Qt::UserRole).toString();
    if (networkId.isEmpty()) {
        qCWarning(windowLog) << "no network in" << item;
        return;
    }

    m_iwd.connectNetwork(networkId);
}

void Window::onDeviceSelectionChanged()
{
    m_scanButton->setEnabled(!m_iwd.isScanning(m_deviceList->currentData().toString()));
}

void Window::onScanningChanged(const QString &station, bool isScanning)
{
    if (m_deviceList->currentData().toString() != station) {
        return;
    }

    m_scanButton->setEnabled(!isScanning);
}

Window::Window(QWidget *parent) : QWidget(parent)
{
    setWindowFlag(Qt::Dialog);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    setLayout(mainLayout);
    m_networkList = new QListWidget;
    m_knownNetworksList = new QListWidget;

    QHBoxLayout *devicesLayout = new QHBoxLayout;
    m_deviceList = new QComboBox;
    m_deviceStateLabel = new QLabel;
    QPushButton *disconnectButton = new QPushButton(tr("Disconnect"));
    disconnectButton->setIcon(QIcon::fromTheme("network-disconnect"));
    m_scanButton = new QPushButton(tr("Scan"));
    m_scanButton->setIcon(QIcon::fromTheme("view-refresh"));
    m_connectButton = new QPushButton(tr("Connect"));
    m_connectButton->setEnabled(false);
    m_connectButton->setIcon(QIcon::fromTheme("network-connect"));

    devicesLayout->addWidget(m_deviceList);
    devicesLayout->addWidget(m_deviceStateLabel);
    devicesLayout->addWidget(disconnectButton);
    devicesLayout->addWidget(m_scanButton);
    devicesLayout->addStretch();
    devicesLayout->addWidget(m_connectButton);

    mainLayout->addLayout(devicesLayout);
    mainLayout->addWidget(new QLabel(tr("Visible networks:")));
    mainLayout->addWidget(m_networkList);
    mainLayout->addSpacing(10);
    mainLayout->addWidget(new QLabel(tr("Known networks:")));
    mainLayout->addWidget(m_knownNetworksList);

    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(QIcon::fromTheme("network-wireless-acquiring"));
    m_tray->show();

    connect(m_networkList, &QListWidget::itemClicked, m_knownNetworksList, &QListWidget::clearSelection);
    connect(m_knownNetworksList, &QListWidget::itemClicked, m_networkList, &QListWidget::clearSelection);
    connect(m_knownNetworksList, &QListWidget::itemChanged, this, &Window::onKnownNetworkCheckedChanged);

    connect(m_networkList, &QListWidget::itemSelectionChanged, this, &Window::onSelectionChanged);
    connect(m_knownNetworksList, &QListWidget::itemSelectionChanged, this, &Window::onSelectionChanged);
    connect(m_deviceList, &QComboBox::currentTextChanged, this, &Window::onDeviceSelectionChanged);

    connect(m_networkList, &QListWidget::itemDoubleClicked, this, &Window::onNetworkDoubleClicked);

    connect(disconnectButton, &QPushButton::clicked, this, &Window::onDisconnectDevice);
    connect(m_scanButton, &QPushButton::clicked, &m_iwd, &Iwd::scan);
    connect(m_connectButton, &QPushButton::clicked, this, &Window::onConnectDevice);

    connect(&m_iwd, &Iwd::visibleNetworkAdded, this, &Window::onVisibleNetworkAdded);
    connect(&m_iwd, &Iwd::visibleNetworkRemoved, this, &Window::onVisibleNetworkRemoved);
    connect(&m_iwd, &Iwd::knownNetworkAdded, this, &Window::onKnownNetworkAdded);
    connect(&m_iwd, &Iwd::knownNetworkRemoved, this, &Window::onKnownNetworkRemoved);
    connect(&m_iwd, &Iwd::knownNetworkEnabledChanged, this, &Window::onKnownNetworkEnabledChanged);
    connect(&m_iwd, &Iwd::deviceAdded, this, &Window::onDeviceAdded);
    connect(&m_iwd, &Iwd::deviceRemoved, this, &Window::onDeviceRemoved);
    connect(&m_iwd, &Iwd::signalLevelChanged, this, &Window::onStationSignalChanged);
    connect(&m_iwd, &Iwd::stationScanningChanged, this, &Window::onScanningChanged);
    connect(&m_iwd, &Iwd::stationStateChanged, this, &Window::onDeviceStateChanged);
    connect(&m_iwd, &Iwd::stationCurrentNetworkChanged, this, &Window::onStationCurrentNetworkChanged);
    connect(&m_iwd, &Iwd::networkConnectedChanged, this, &Window::onNetworkConnectedChanged);
    connect(&m_iwd, &Iwd::visibleNetworkKnownChanged, this, &Window::onVisibleNetworkKnownChanged);

    connect(m_tray, &QSystemTrayIcon::activated, this, [this]() { setVisible(!isVisible()); });


    m_signalAgent = new SignalLevelAgent(&m_iwd);
    if (QDBusConnection::systemBus().registerObject(m_signalAgent->objectPath().path(), this)) {
        m_iwd.setSignalAgent(m_signalAgent->objectPath(), {-20, -40, -49, -50, -51, -60, -80});
        qCDebug(windowLog) << "set signal agent";
    } else {
        qCWarning(windowLog) << "Failed to register signal agent";
    }
    connect(m_signalAgent, &SignalLevelAgent::signalLevelChanged, this, &Window::onStationSignalChanged);

    m_authUi = new AuthUi(&m_iwd);
    if (QDBusConnection::systemBus().registerObject(m_authUi->objectPath().path(), this)) {
        m_iwd.setAuthAgent(m_authUi->objectPath());
    } else {
        qCWarning(windowLog) << "Failed to register auth agent";
    }

    m_iwd.init();
}
