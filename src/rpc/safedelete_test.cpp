// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "safedelete.h"

using namespace tremotesf::impl;

class SafeDeleteTest final : public QObject {
    Q_OBJECT

private slots:
    void normalChild() {
        QVERIFY(isSafeToDeletePath("/home/user/downloads/foo", "/home/user/downloads"));
        QVERIFY(isSafeToDeletePath("/home/user/downloads/foo/bar", "/home/user/downloads"));
    }

    void directoryItself() { QVERIFY(!isSafeToDeletePath("/home/user/downloads", "/home/user/downloads")); }

    void outsideDirectory() {
        QVERIFY(!isSafeToDeletePath("/home/user/other/foo", "/home/user/downloads"));
        QVERIFY(!isSafeToDeletePath("/etc/passwd", "/home/user/downloads"));
    }

    void pathTraversal() {
        QVERIFY(!isSafeToDeletePath("/home/user/downloads/../etc", "/home/user/downloads"));
        QVERIFY(!isSafeToDeletePath("/home/user/downloads/foo/../../etc", "/home/user/downloads"));
    }

    void rootAndSystemDirectories() {
        QVERIFY(!isSafeToDeletePath("/", "/"));
        QVERIFY(!isSafeToDeletePath("/foo", "/"));
        QVERIFY(!isSafeToDeletePath("/tmp/foo", "/tmp"));
        QVERIFY(!isSafeToDeletePath("/etc/foo", "/etc"));
        QVERIFY(!isSafeToDeletePath("/home/foo", "/home"));
    }

    void relativePaths() {
        QVERIFY(!isSafeToDeletePath("foo", "downloads"));
        QVERIFY(!isSafeToDeletePath("./foo", "./downloads"));
    }

    void emptyPaths() {
        QVERIFY(!isSafeToDeletePath({}, "/home/user/downloads"));
        QVERIFY(!isSafeToDeletePath("/home/user/downloads/foo", {}));
    }

    void trailingSlashes() {
        QVERIFY(isSafeToDeletePath("/home/user/downloads/foo/", "/home/user/downloads/"));
        QVERIFY(!isSafeToDeletePath("/home/user/downloads/", "/home/user/downloads/"));
    }
};

QTEST_GUILESS_MAIN(SafeDeleteTest)

#include "safedelete_test.moc"
