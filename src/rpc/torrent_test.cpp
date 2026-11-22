// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "rpc.h"
#include "serversettings.h"
#include "torrent.h"

using namespace tremotesf;
using namespace Qt::StringLiterals;

namespace {
    QVariantList torrentValues() {
        return {
            QString("0123456789012345678901234567890123456789"), // d.hash
            QString("test torrent"), // d.name
            qint64(1000), // d.load_date
            qint64(2000), // d.timestamp.started
            qint64(1000), // d.size_bytes
            qint64(250), // d.completed_bytes
            qint64(750), // d.left_bytes
            qint64(100), // d.down.rate
            qint64(10), // d.up.rate
            qint64(500), // d.down.total
            qint64(100), // d.up.total
            500, // d.ratio (per mille)
            false, // d.is_multi_file
            QString("/downloads"), // d.directory
            QString("/downloads/test torrent"), // d.base_path
            QString("movies"), // d.custom1
            QString(), // d.message
            50, // d.peers_max
            2, // d.priority
            1, // d.state
            false, // d.hashing
            false, // d.is_hash_checking
            false, // d.complete
            false, // d.hashing_failed
            false, // d.is_meta
            7, // d.peers_connected
            qint64(0), // d.timestamp.finished
            qint64(500), // d.creation_date
            qint64(16384), // d.chunk_size
            2, // d.chunks_hashed
            10, // d.size_chunks
            QVariantList{
                QVariantList{QString("https://tracker.example/announce"), 0, true, true, false, 10, 20, 5, 0, 1, 0}
            }
        };
    }
}

class TorrentTest final : public QObject {
    Q_OBJECT

private slots:
    void fieldListMatchesValues() {
        Rpc rpc;
        QCOMPARE(Torrent::updateFields().size(), torrentValues().size());
    }

    void multiFileDownloadDirectory() {
        Rpc rpc;
        auto values = torrentValues();
        values[12] = true; // d.is_multi_file
        values[13] = QString("/downloads/test torrent"); // d.directory
        values[14] = QString("/downloads/test torrent"); // d.base_path
        Torrent torrent(42, "0123456789012345678901234567890123456789"_L1, values, &rpc);
        QCOMPARE(torrent.data().downloadDirectory, "/downloads"_L1);
        QCOMPARE(torrent.data().rootDirectory, "/downloads/test torrent"_L1);
        QCOMPARE(torrent.data().basePath, "/downloads/test torrent"_L1);
    }

    void mapping() {
        Rpc rpc;
        Torrent torrent(42, "0123456789012345678901234567890123456789"_L1, torrentValues(), &rpc);
        const auto& data = torrent.data();
        QCOMPARE(data.id, 42);
        QCOMPARE(data.hashString, "0123456789012345678901234567890123456789"_L1);
        QCOMPARE(data.name, "test torrent"_L1);
        QCOMPARE(data.totalSize, 1000);
        QCOMPARE(data.completedSize, 250);
        QCOMPARE(data.leftUntilDone, 750);
        QCOMPARE(data.sizeWhenDone, 1000);
        QVERIFY(qFuzzyCompare(data.percentDone, 0.25));
        QCOMPARE(data.eta, 7);
        QCOMPARE(data.status, TorrentData::Status::Downloading);
        QVERIFY(data.labels == std::vector<QString>{"movies"_L1});
        QCOMPARE(data.peersLimit, 50);
        QCOMPARE(data.bandwidthPriority, TorrentData::Priority::Normal);
        QCOMPARE(data.downloadDirectory, "/downloads"_L1);
        QCOMPARE(data.magnetLink, "magnet:?xt=urn:btih:0123456789012345678901234567890123456789"_L1);
        QCOMPARE(data.trackers.size(), size_t(1));
        QCOMPARE(data.totalSeedersFromTrackersCount, 10);
        QCOMPARE(data.totalLeechersFromTrackersCount, 20);
    }
};

QTEST_GUILESS_MAIN(TorrentTest)

#include "torrent_test.moc"
