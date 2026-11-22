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
            1, // d.wanted_chunks
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
        values[11] = true; // d.is_multi_file
        values[12] = QString("/downloads/test torrent"); // d.directory
        values[13] = QString("/downloads/test torrent"); // d.base_path
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
        QVERIFY(qFuzzyCompare(data.ratio, 0.2));
        QCOMPARE(data.trackers.size(), size_t(1));
        QCOMPARE(data.totalSeedersFromTrackersCount, 10);
        QCOMPARE(data.totalLeechersFromTrackersCount, 20);
    }

    void partiallySelectedTorrentCountsOnlyWantedChunks() {
        Rpc rpc;
        auto values = torrentValues();
        values[30] = 0; // d.wanted_chunks
        Torrent torrent(42, "0123456789012345678901234567890123456789"_L1, values, &rpc);
        const auto& data = torrent.data();
        QCOMPARE(data.leftUntilDone, 0);
        QCOMPARE(data.sizeWhenDone, 250);
        QVERIFY(qFuzzyCompare(data.percentDone, 1.0));
        QCOMPARE(data.eta, 0);
        QCOMPARE(data.status, TorrentData::Status::Seeding);
    }

    void partiallySelectedTorrentLeftSizeUsesWantedChunks() {
        Rpc rpc;
        auto values = torrentValues();
        values[27] = 100; // d.chunk_size
        values[30] = 1; // d.wanted_chunks
        Torrent torrent(42, "0123456789012345678901234567890123456789"_L1, values, &rpc);
        const auto& data = torrent.data();
        QCOMPARE(data.leftUntilDone, 100);
        QCOMPARE(data.sizeWhenDone, 350);
        QVERIFY(qFuzzyCompare(data.percentDone, 250.0 / 350.0));
        QCOMPARE(data.eta, 1);
        QCOMPARE(data.status, TorrentData::Status::Downloading);
    }

    void localErrorIsNotTrackerError() {
        Rpc rpc;
        auto values = torrentValues();
        values[15] = QString("Could not resume download: Could not open file: No such file or directory");
        Torrent torrent(42, "0123456789012345678901234567890123456789"_L1, values, &rpc);
        QCOMPARE(torrent.data().error, TorrentData::Error::LocalError);
    }

    void trackerErrorIsTrackerError() {
        Rpc rpc;
        auto values = torrentValues();
        values[15] = QString("Tracker: [v4 : 403 Forbidden]");
        Torrent torrent(42, "0123456789012345678901234567890123456789"_L1, values, &rpc);
        QCOMPARE(torrent.data().error, TorrentData::Error::TrackerError);
    }

    void multipleLabelsAreSplit() {
        Rpc rpc;
        auto values = torrentValues();
        values[14] = QString("movies\nanime");
        Torrent torrent(42, "0123456789012345678901234567890123456789"_L1, values, &rpc);
        const std::vector<QString> expected{"movies"_L1, "anime"_L1};
        QVERIFY(torrent.data().labels == expected);
    }

    void rtorrentOffFileHasNormalPriority() {
        const TorrentFile file(QVariantList{QString("file.mkv"), qint64(100), 0, 1, 0}, 100);
        QVERIFY(!file.wanted);
        QCOMPARE(file.priority, TorrentFile::Priority::Normal);
    }
};

QTEST_GUILESS_MAIN(TorrentTest)

#include "torrent_test.moc"
