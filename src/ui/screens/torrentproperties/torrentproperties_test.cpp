// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <memory>

#include <QTest>

#include "rpc/rpc.h"
#include "rpc/torrent.h"
#include "torrentfilesmodel.h"
#include "torrentpropertiesdialog.h"

using namespace tremotesf;
using namespace Qt::StringLiterals;

namespace {
    std::unique_ptr<Torrent> makeTorrent(Rpc* rpc) {
        const auto fields = Torrent::updateFields();
        QVariantList values(fields.size());
        const auto set = [&](const QString& field, QVariant value) {
            const auto index = fields.indexOf(field);
            Q_ASSERT(index >= 0);
            values[index] = std::move(value);
        };
        set("d.name="_L1, QString("test torrent"));
        set("d.hash="_L1, QString("0123456789012345678901234567890123456789"));
        set("d.size_bytes="_L1, qint64(1024 * 1024));
        set("d.completed_bytes="_L1, qint64(512 * 1024));
        set("d.left_bytes="_L1, qint64(512 * 1024));
        set("d.is_multi_file="_L1, true);
        set("d.directory="_L1, QString("/downloads/test torrent"));
        set("d.base_path="_L1, QString("/downloads/test torrent"));
        set("d.custom1="_L1, QString("movies"));
        set("d.peers_max="_L1, 50);
        set("d.priority="_L1, 2);
        set("d.state="_L1, 1);
        set("d.complete="_L1, true);
        set("d.chunk_size="_L1, qint64(16384));
        set("d.size_chunks="_L1, 64);
        values.back() = QVariantList{};
        return std::make_unique<Torrent>(42, "0123456789012345678901234567890123456789"_L1, values, rpc);
    }

    QVariantList fileRows() {
        QVariantList rows;
        rows.reserve(1000);
        for (int i = 0; i < 1000; ++i) {
            rows.append(QVariantList{QString("dir/file%1.bin").arg(i), qint64(1024), 0, 1, 1});
        }
        return rows;
    }
}

class TorrentPropertiesTest final : public QObject {
    Q_OBJECT

private slots:
    void opensDialogWithManyFiles() {
        Rpc rpc;
        auto torrent = makeTorrent(&rpc);
        torrent->updateFiles(fileRows());

        TorrentPropertiesDialog dialog(torrent.get(), &rpc, nullptr);
        dialog.show();

        for (int i = 0; i < 10; ++i) {
            torrent->updateFiles(fileRows());
            QTest::qWait(20);
        }

        QVERIFY(dialog.isVisible());
    }

    void keepsRootLevelFilesOfMultiFileTorrent() {
        Rpc rpc;
        auto torrent = makeTorrent(&rpc);
        TorrentFilesModel model(&rpc);
        model.setTorrent(torrent.get(), false);

        torrent->updateFiles(
            QVariantList{
                QVariantList{QString("RARBG.txt"), qint64(30), 1, 1, 0},
                QVariantList{QString("RARBG_DO_NOT_MIRROR.exe"), qint64(99), 1, 1, 0},
                QVariantList{QString("Subs/English.srt"), qint64(135825), 1, 1, 1},
                QVariantList{QString("Movie.mp4"), qint64(1804198899), 1, 1, 1},
            }
        );

        QTRY_COMPARE(model.rowCount(QModelIndex{}), 1);
        const auto root = model.index(0, 0, QModelIndex{});
        QCOMPARE(root.data().toString(), "test torrent"_L1);
        QTRY_COMPARE(model.rowCount(root), 4);

        QModelIndex rarbgIndex{};
        for (int row = 0; row < model.rowCount(root); ++row) {
            const auto index = model.index(row, 0, root);
            if (index.data().toString() == "RARBG.txt"_L1) {
                rarbgIndex = index;
                break;
            }
        }
        QVERIFY(rarbgIndex.isValid());
        QCOMPARE(model.localFilePath(rarbgIndex), "/downloads/test torrent/RARBG.txt"_L1);
    }
};

QTEST_MAIN(TorrentPropertiesTest)

#include "torrentproperties_test.moc"
