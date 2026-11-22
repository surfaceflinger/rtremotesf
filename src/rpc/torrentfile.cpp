// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "torrentfile.h"

#include <algorithm>

#include "stdutils.h"

namespace tremotesf {
    namespace {
        qint64 completedSizeFromChunks(const QVariantList& row, qint64 size, qint64 chunkSize) {
            if (row.size() < 3) {
                return 0;
            }
            const auto completedChunks = row.at(2).toLongLong();
            return std::min(size, completedChunks * chunkSize);
        }
    }

    TorrentFile::TorrentFile(const QVariantList& row, qint64 chunkSize) {
        if (row.size() >= 2) {
            path = row.at(0).toString();
            size = row.at(1).toLongLong();
        }
        completedSize = completedSizeFromChunks(row, size, chunkSize);
        if (row.size() >= 5) {
            const auto priorityValue = row.at(4).toInt();
            wanted = priorityValue > 0;
            switch (priorityValue) {
            case 2:
                priority = Priority::High;
                break;
            case 1:
                priority = Priority::Normal;
                break;
            default:
                priority = Priority::Low;
                break;
            }
        }
    }

    bool TorrentFile::update(const QVariantList& row, qint64 chunkSize) {
        bool changed = false;
        setChanged(completedSize, completedSizeFromChunks(row, size, chunkSize), changed);
        if (row.size() >= 5) {
            const auto priorityValue = row.at(4).toInt();
            setChanged(wanted, priorityValue > 0, changed);
            Priority newPriority{};
            switch (priorityValue) {
            case 2:
                newPriority = Priority::High;
                break;
            case 1:
                newPriority = Priority::Normal;
                break;
            default:
                newPriority = Priority::Low;
                break;
            }
            setChanged(priority, newPriority, changed);
        }
        return changed;
    }
}
