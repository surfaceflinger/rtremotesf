// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QList>

#include "scgi.h"

using namespace Qt::StringLiterals;

namespace tremotesf::scgi {
    QByteArray makeRequest(const QByteArray& body) {
        QByteArray headers{"CONTENT_LENGTH"};
        headers.append('\0');
        headers.append(QByteArray::number(body.size()));
        headers.append('\0');
        headers.append("SCGI"_ba);
        headers.append('\0');
        headers.append('1');
        headers.append('\0');

        QByteArray request{};
        request.reserve(headers.size() + body.size() + 16);
        request.append(QByteArray::number(headers.size()));
        request.append(':');
        request.append(headers);
        request.append(',');
        request.append(body);
        return request;
    }

    bool isResponseComplete(const QByteArray& data) {
        auto headerEnd = data.indexOf("\r\n\r\n");
        qsizetype separatorSize = 4;
        if (headerEnd == -1) {
            headerEnd = data.indexOf("\n\n");
            separatorSize = 2;
        }
        if (headerEnd == -1) {
            return false;
        }
        for (const auto& line : data.first(headerEnd).split('\n')) {
            const auto trimmed = line.trimmed();
            if (!trimmed.toLower().startsWith("content-length:")) {
                continue;
            }
            bool ok{};
            const auto contentLength = trimmed.sliced(15).trimmed().toLongLong(&ok);
            return ok && data.size() >= headerEnd + separatorSize + contentLength;
        }
        return false;
    }

    Response parseResponse(const QByteArray& data) {
        auto headerEnd = data.indexOf("\r\n\r\n");
        qsizetype separatorSize = 4;
        if (headerEnd == -1) {
            headerEnd = data.indexOf("\n\n");
            separatorSize = 2;
        }
        if (headerEnd == -1) {
            return {.errorMessage = "Incomplete response headers"_L1};
        }

        qsizetype contentLength = -1;
        for (const auto& line : data.first(headerEnd).split('\n')) {
            const auto trimmed = line.trimmed();
            const auto colon = trimmed.indexOf(':');
            if (colon == -1) {
                continue;
            }
            const auto name = trimmed.first(colon).trimmed();
            const auto value = trimmed.sliced(colon + 1).trimmed();
            if (name.compare("Content-Length"_L1, Qt::CaseInsensitive) == 0) {
                bool ok{};
                contentLength = value.toLongLong(&ok);
                if (!ok || contentLength < 0) {
                    return {.errorMessage = "Invalid Content-Length header"_L1};
                }
            }
        }

        const auto bodyStart = headerEnd + separatorSize;
        if (contentLength == -1) {
            return {.body = data.sliced(bodyStart)};
        }
        if (data.size() - bodyStart < contentLength) {
            return {.errorMessage = "Incomplete response body"_L1};
        }
        return {.body = data.sliced(bodyStart, contentLength)};
    }
}
