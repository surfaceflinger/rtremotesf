// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "scgi.h"

using namespace tremotesf::scgi;
using namespace Qt::StringLiterals;

class ScgiTest final : public QObject {
    Q_OBJECT

private slots:
    void request() {
        const auto request = makeRequest("hello");
        constexpr char expected[] = "24:CONTENT_LENGTH\x00"
                                    "5\x00"
                                    "SCGI\x00"
                                    "1\x00"
                                    ",hello";
        QCOMPARE(request, QByteArray(expected, sizeof(expected) - 1));
    }

    void emptyRequest() {
        const auto request = makeRequest({});
        constexpr char expected[] = "24:CONTENT_LENGTH\x00"
                                    "0\x00"
                                    "SCGI\x00"
                                    "1\x00"
                                    ",";
        QCOMPARE(request, QByteArray(expected, sizeof(expected) - 1));
    }

    void response() {
        const auto response =
            parseResponse("Status: 200 OK\r\nContent-Type: text/xml\r\nContent-Length: 5\r\n\r\nhello");
        QVERIFY(response.errorMessage.isEmpty());
        QCOMPARE(response.body, QByteArray("hello"));
        QVERIFY(isResponseComplete("Status: 200 OK\r\nContent-Length: 5\r\n\r\nhello"));
        QVERIFY(!isResponseComplete("Status: 200 OK\r\nContent-Length: 5\r\n\r\nhel"));
    }

    void incompleteResponse() {
        const auto noContentLength = parseResponse("Status: 200 OK\r\n\r\n");
        QVERIFY(noContentLength.errorMessage.isEmpty());
        QVERIFY(noContentLength.body.isEmpty());
        QVERIFY(!isResponseComplete("Status: 200 OK\r\n\r\n"));
        QVERIFY(!parseResponse("Status: 200 OK\r\nContent-Length: 5\r\n\r\nhel").errorMessage.isEmpty());
    }
};

QTEST_GUILESS_MAIN(ScgiTest)

#include "scgi_test.moc"
