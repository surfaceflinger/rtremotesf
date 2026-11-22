// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_XMLRPC_H
#define TREMOTESF_RPC_XMLRPC_H

#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QVariantList>

namespace tremotesf::xmlrpc {
    QByteArray makeMethodCall(const QString& methodName, const QVariantList& parameters);

    struct Response {
        bool fault{};
        QVariant value{};
        int faultCode{};
        QString faultString{};
        QString parseError{};
    };

    Response parseMethodResponse(const QByteArray& data);
}

#endif // TREMOTESF_RPC_XMLRPC_H
