#include "AuthUi.h"

#include <QInputDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>

Q_LOGGING_CATEGORY(authLog, "iwd.auth", QtInfoMsg)

AuthUi::AuthUi(Iwd *iwd, QWidget *parentWindow) : AuthAgent(iwd),
    m_parentWindow(parentWindow)
{
    connect(this, &AuthAgent::released, this, [this](){
        qCDebug(authLog) << this << "Released";
    });
    connect(this, &AuthAgent::released, this, &QObject::deleteLater);
}

QString AuthUi::onRequestPrivateKeyPassphrase(const QString &networkId)
{
    qCDebug(authLog) << "Requesting private key passphrase for" << networkId;

    return QInputDialog::getText(m_parentWindow,
                                 tr("Private key passphrase"),
                                 tr("Please enter private key passphrase for '%1'").arg(m_iwd->networkName(networkId)),
                                 QLineEdit::Password);
}

QString AuthUi::onRequestPassphrase(const QString &networkId)
{
    qCDebug(authLog) << "Requesting passphrase for" << networkId;

    return QInputDialog::getText(m_parentWindow,
                                 tr("Network passphrase"),
                                 tr("Please enter passphrase for '%1'").arg(m_iwd->networkName(networkId)),
                                 QLineEdit::Password);
}

QPair<QString, QString> AuthUi::onRequestUsernameAndPassword(const QString &networkId)
{
    qCDebug(authLog) << "Requesting username and password for" << networkId;

    QPointer<QDialog> dialog = new QDialog(m_parentWindow);
    dialog->setWindowTitle(tr("Username and password"));

    QVBoxLayout *layout = new QVBoxLayout;
    dialog->setLayout(layout);

    layout->addWidget(new QLabel(tr("Please enter credentials for '%1'").arg(m_iwd->networkName(networkId))));

    layout->addStretch();

    layout->addWidget(new QLabel(tr("Username:")));
    QPointer<QLineEdit> usernameEdit = new QLineEdit;
    layout->addWidget(usernameEdit);

    layout->addWidget(new QLabel(tr("Password:")));
    QPointer<QLineEdit> passwordEdit = new QLineEdit;
    passwordEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(passwordEdit);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accepted);
    connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::rejected);
    layout->addWidget(buttonBox);


    // Because we can't user open(), the dialog might get deleted for us if the application quits while it is open
    int result = dialog->exec();
    if (!dialog || !usernameEdit || !passwordEdit) {
        qCWarning(authLog) << "Dialog or widgets deleted while executing, app might be terminated";
        return {};
    }

    const QString username = usernameEdit->text();
    const QString password = passwordEdit->text();
    dialog->deleteLater();

    if (result == QDialog::Accepted) {
        return {username, password};
    } else {
        return {};
    }
}

QString AuthUi::onRequestUserPassword(const QString &username, const QString &networkId)
{
    qCDebug(authLog) << "Requesting user password for" << username << "for" << networkId;
    return QInputDialog::getText(m_parentWindow,
                                 tr("Password for username"),
                                 tr("Please enter password for user %1 for '%2'").arg(username, m_iwd->networkName(networkId)),
                                 QLineEdit::Password);
}
