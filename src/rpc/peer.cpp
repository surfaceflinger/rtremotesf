// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "peer.h"

#include "stdutils.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        QString flagsFromRow(const QVariantList& row) {
            QString flags{};
            const auto appendFlag = [&](int index, QChar flag) {
                if (index < row.size() && (row.at(index).toBool() || row.at(index).toInt() != 0)) {
                    flags += flag;
                }
            };
            appendFlag(5, u'E');
            appendFlag(6, u'I');
            appendFlag(7, u'S');
            appendFlag(8, u'P');
            appendFlag(9, u'U');
            return flags;
        }
    }

    Peer::Peer(QString address, const QVariantList& row) : address(std::move(address)) { update(row); }

    bool Peer::update(const QVariantList& row) {
        bool changed = false;
        if (row.size() >= 2) {
            setChanged(client, row.at(1).toString(), changed);
        }
        if (row.size() >= 3) {
            setChanged(downloadSpeed, row.at(2).toLongLong(), changed);
        }
        if (row.size() >= 4) {
            setChanged(uploadSpeed, row.at(3).toLongLong(), changed);
        }
        if (row.size() >= 5) {
            setChanged(progress, row.at(4).toInt() / 100.0, changed);
        }
        setChanged(flags, flagsFromRow(row), changed);
        return changed;
    }
}
