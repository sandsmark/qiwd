#ifndef AUTHUI_H
#define AUTHUI_H

#include <QObject>
#include <QWidget>
#include "Iwd.h"

Q_DECLARE_LOGGING_CATEGORY(authLog)

class Window;

class AuthUi : public AuthAgent
{
    Q_OBJECT

public:
    explicit AuthUi(Iwd *iwd, QWidget *parentWindow = nullptr);

    // AuthAgent interface
protected:
    QString onRequestPrivateKeyPassphrase(const QString &networkId) override;
    QString onRequestPassphrase(const QString &networkId) override;
    QPair<QString, QString> onRequestUsernameAndPassword(const QString &networkId) override;
    QString onRequestUserPassword(const QString &username, const QString &networkId) override;

private:
    QWidget *m_parentWindow;
};

#endif // AUTHUI_H
