// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_SCGI_H
#define TREMOTESF_RPC_SCGI_H

#include <QByteArray>
#include <QString>

namespace tremotesf::scgi {
    QByteArray makeRequest(const QByteArray& body);

    struct Response {
        QByteArray body{};
        QString errorMessage{};
    };

    bool isResponseComplete(const QByteArray& data);
    Response parseResponse(const QByteArray& data);
}

#endif // TREMOTESF_RPC_SCGI_H
