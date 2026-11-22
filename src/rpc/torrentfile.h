// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_TORRENTFILE_H
#define TREMOTESF_RPC_TORRENTFILE_H

#include <QObject>
#include <QString>
#include <QVariantList>

namespace tremotesf {
    struct TorrentFile {
        Q_GADGET
    public:
        enum class Priority { Low, Normal, High };
        Q_ENUM(Priority)

        explicit TorrentFile(const QVariantList& row, qint64 chunkSize);
        bool update(const QVariantList& row, qint64 chunkSize);

        QString path{};
        qint64 size{};
        qint64 completedSize{};
        Priority priority{};
        bool wanted{};

        auto pathParts() const { return path.tokenize(u'/', Qt::SkipEmptyParts); }
    };
}

#endif // TREMOTESF_RPC_TORRENTFILE_H
