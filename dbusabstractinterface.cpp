/*
 * Copyright 2010  Alex Merry <alex.merry@kdemail.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "dbusabstractinterface.h"
#include <QDebug>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusMetaType>
#include <QtCore/QMetaProperty>

//Q_GLOBAL_STATIC_WITH_ARGS(QByteArray, \
//                          propertiesChangedSignature, \
//                          (QMetaObject::normalizedSignature(SIGNAL(propertiesChanged(QMap<QString,QVariant>,QStringList)))))
//Q_GLOBAL_STATIC_WITH_ARGS(QByteArray, \
//                          propertyChangedSignature, \
//                          (QMetaObject::normalizedSignature(SIGNAL(propertyChanged(QString,QVariant)))))

QMetaMethod propertiesChangedSignature = QMetaMethod::fromSignal(&DBusAbstractInterface::propertiesChanged);
QMetaMethod propertyChangedSignature = QMetaMethod::fromSignal(&DBusAbstractInterface::propertyChanged);

DBusAbstractInterface::DBusAbstractInterface(
        const QString &service,
        const QString &path,
        const char *interface,
        const QDBusConnection &connection,
        QObject *parent)
        : QDBusAbstractInterface( service, path, interface, connection, parent )
        , m_connected( false )
        , m_includeErrorMessages( false )
{

}

void DBusAbstractInterface::connectNotify(const QMetaMethod &signal)
{
    if ( signal == propertiesChangedSignature ||
         signal == propertyChangedSignature )
    {
        if ( !m_connected )
        {
#if QT_VERSION >= 0x040600
            QStringList matchArgs;
            matchArgs << interface();
#endif
            connection().connect( service(),
                                  path(),
                                  "org.freedesktop.DBus.Properties",
                                  "PropertiesChanged",
#if QT_VERSION >= 0x040600
                                  matchArgs,
                                  QString(),
#endif
                                  this,
                                  SLOT(_m_propertiesChanged(QString,QMap<QString,QVariant>,QStringList)) );
            m_connected = true;
            return;
        }
    }
    else
    {
        QDBusAbstractInterface::connectNotify( signal );
    }
}

void DBusAbstractInterface::disconnectNotify(const QMetaMethod &signal)
{
    if ( signal == propertiesChangedSignature ||
         signal == propertyChangedSignature )
    {
        if ( m_connected &&
             receivers(propertiesChangedSignature.methodSignature()) == 0 &&
             receivers(propertyChangedSignature.methodSignature()) == 0 )
        {
#if QT_VERSION >= 0x040600
            QStringList matchArgs;
            matchArgs << interface();
#endif
            connection().disconnect( service(),
                                     path(),
                                     "org.freedesktop.DBus.Properties",
                                     "PropertiesChanged",
#if QT_VERSION >= 0x040600
                                     matchArgs,
                                     QString(),
#endif
                                     this,
                                     SLOT(_m_propertiesChanged(QString,QMap<QString,QVariant>,QStringList)) );
            m_connected = false;
            return;
        }
    }
    else
    {
        QDBusAbstractInterface::disconnectNotify( signal );
    }
}

QVariant DBusAbstractInterface::demarshall( const QByteArray& property,
                                            const QVariant& value,
                                            bool* ok )
{
    if ( ok )
        *ok = false;
    int propIndex = metaObject()->indexOfProperty( property.constData() );
    QMetaProperty mp = metaObject()->property( propIndex );
    if ( value.userType() == qMetaTypeId<QDBusArgument>() )
    {
        QDBusArgument arg = value.value<QDBusArgument>();
        if ( mp.isValid() )
        {
            const char * propSig = QDBusMetaType::typeToSignature( mp.userType() );
            if ( arg.currentSignature().toLatin1() == propSig )
            {
                QVariant output = QVariant( mp.userType(), (void*)0 );
                QDBusMetaType::demarshall( arg, mp.userType(), output.data() );
                if ( ok && output.isValid() )
                    *ok = true;
                return output;
            }
            else if ( m_includeErrorMessages )
            {
                QString errorMessage = "Received invalid property \"%1\" in "
                                       "PropertiesChanged signal: "
                                       "expected %2 (%3), got user type (%4)";
                errorMessage.arg( QString::fromLatin1(property) )
                            .arg( QString::fromLatin1(mp.typeName()) )
                            .arg( QString::fromLatin1(propSig) )
                            .arg( arg.currentSignature() );
                qDebug() << errorMessage;
                return qVariantFromValue(QDBusError(
                        QDBusError::InvalidSignature, errorMessage
                        ));
            }
        }
        else if ( m_includeErrorMessages )
        {
            QString errorMessage = "Received unknown property \"%1\" in "
                                   "PropertiesChanged signal, with D-Bus type "
                                   "%2: cannot convert";
            errorMessage.arg( QString::fromLatin1(property) )
                        .arg( arg.currentSignature() );
            qDebug() << errorMessage;
            return qVariantFromValue(QDBusError(
                    QDBusError::InvalidSignature, errorMessage
                    ));
        }
    }
    else if ( value.userType() == mp.userType() )
    {
        if ( ok )
            *ok = true;
        return value;
    }
    else if ( m_includeErrorMessages )
    {
        const char * propSig = QDBusMetaType::typeToSignature( mp.userType() );
        const char * valueSig = QDBusMetaType::typeToSignature( value.userType() );
        QString errorMessage = "Received invalid property \"%1\" in "
                               "PropertiesChanged signal: "
                               "expected %2 (%3), got %4 (%5)";
        errorMessage.arg( QString::fromLatin1(property) )
                    .arg( QString::fromLatin1(mp.typeName()) )
                    .arg( QString::fromLatin1(propSig) )
                    .arg( QString::fromLatin1(value.typeName()) )
                    .arg( QString::fromLatin1(valueSig) );
        qDebug() << errorMessage;
        return qVariantFromValue(QDBusError(
                QDBusError::InvalidSignature, errorMessage
                ));
    }
    return QVariant();
}

void DBusAbstractInterface::_m_propertiesChanged(
        const QString& interfaceName,
        const QMap<QString, QVariant>& changedProperties,
        const QStringList& invalidatedProperties )
{
    if ( interfaceName == interface() )
    {
        QVariantMap propsMap = changedProperties;
        QMutableMapIterator<QString, QVariant> i( propsMap );
        while ( i.hasNext() ) {
            i.next();
            bool demarshalled = false;
            QVariant dmValue = demarshall( i.key().toLatin1(), i.value(), &demarshalled );
            if ( m_includeErrorMessages || demarshalled )
            {
                i.setValue( dmValue );
                emit propertyChanged( i.key(), dmValue );
            }
            else
            {
                i.remove();
            }
        }
        QListIterator<QString> j( invalidatedProperties );
        while ( j.hasNext() ) {
            QString propName = j.next();
            if ( metaObject()->indexOfProperty( propName.toLatin1().constData() ) == -1 )
            {
                qDebug() << "Got unknown invalidated property" <<  propName;
            }
            else
            {
                emit propertyInvalidated( j.next() );
            }
        }
        emit propertiesChanged( propsMap, invalidatedProperties );
    }
}
