// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "xmlrpc.h"

using namespace tremotesf::xmlrpc;
using namespace Qt::StringLiterals;

class XmlRpcTest final : public QObject {
    Q_OBJECT

private slots:
    void methodCall() {
        const auto data = makeMethodCall("d.name"_L1, {QString("abc"), 42, true});
        QCOMPARE(
            data,
            QByteArray(
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?><methodCall><methodName>d.name</methodName><params>"
                "<param><value><string>abc</string></value></param>"
                "<param><value><i8>42</i8></value></param>"
                "<param><value><boolean>1</boolean></value></param>"
                "</params></methodCall>"
            )
        );
    }

    void methodCallBase64() {
        const QByteArray binary("\x01\x02\xff", 3);
        const auto data = makeMethodCall("load.raw_start"_L1, {QString{}, binary});
        QVERIFY(data.contains("<base64>"_ba + binary.toBase64() + "</base64>"_ba));
    }

    void methodResponse() {
        const auto response = parseMethodResponse(
            "<?xml version=\"1.0\"?><methodResponse><params><param><value><string>0.16.28</string></value>"
            "</param></params></methodResponse>"
        );
        QVERIFY(response.parseError.isEmpty());
        QVERIFY(!response.fault);
        QCOMPARE(response.value.toString(), "0.16.28"_L1);
    }

    void multicallResponse() {
        const auto response = parseMethodResponse(
            "<?xml version=\"1.0\"?><methodResponse><params><param><value><array><data>"
            "<value><array><data>"
            "<value><string>hash</string></value>"
            "<value><i8>353370112</i8></value>"
            "<value><boolean>1</boolean></value>"
            "</data></array></value>"
            "<value><array><data><value><string>other</string></value></data></array></value>"
            "</data></array></value></param></params></methodResponse>"
        );
        QVERIFY(response.parseError.isEmpty());
        const auto rows = response.value.toList();
        QCOMPARE(rows.size(), 2);
        const auto first = rows.at(0).toList();
        QCOMPARE(first.size(), 3);
        QCOMPARE(first.at(0).toString(), "hash"_L1);
        QCOMPARE(first.at(1).toLongLong(), 353370112);
        QVERIFY(first.at(2).toBool());
    }

    void structResponse() {
        const auto response = parseMethodResponse(
            "<?xml version=\"1.0\"?><methodResponse><params><param><value><struct>"
            "<member><name>dht</name><value><string>auto</string></value></member>"
            "<member><name>active</name><value><i8>12</i8></value></member>"
            "</struct></value></param></params></methodResponse>"
        );
        QVERIFY(response.parseError.isEmpty());
        const auto map = response.value.toMap();
        QCOMPARE(map.value("dht"_L1).toString(), "auto"_L1);
        QCOMPARE(map.value("active"_L1).toLongLong(), 12);
    }

    void faultResponse() {
        const auto response = parseMethodResponse(
            "<?xml version=\"1.0\"?><methodResponse><fault><value><struct>"
            "<member><name>faultCode</name><value><i8>-506</i8></value></member>"
            "<member><name>faultString</name><value><string>method not found</string></value></member>"
            "</struct></value></fault></methodResponse>"
        );
        QVERIFY(response.parseError.isEmpty());
        QVERIFY(response.fault);
        QCOMPARE(response.faultCode, -506);
        QCOMPARE(response.faultString, "method not found"_L1);
    }

    void malformedResponse() {
        const auto response = parseMethodResponse("<methodResponse><params>");
        QVERIFY(!response.parseError.isEmpty());
    }
};

QTEST_GUILESS_MAIN(XmlRpcTest)

#include "xmlrpc_test.moc"
