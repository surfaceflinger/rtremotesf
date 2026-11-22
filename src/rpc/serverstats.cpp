// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "serverstats.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    QStringList ServerStats::requestCommands() {
        return {
            "throttle.global_down.rate"_L1,
            "throttle.global_up.rate"_L1,
            "throttle.global_down.total"_L1,
            "throttle.global_up.total"_L1
        };
    }

    void ServerStats::update(const QVariantList& values) {
        if (values.size() >= 1) {
            mDownloadSpeed = values.at(0).toLongLong();
        }
        if (values.size() >= 2) {
            mUploadSpeed = values.at(1).toLongLong();
        }
        if (values.size() >= 3) {
            mDownloaded = values.at(2).toLongLong();
        }
        if (values.size() >= 4) {
            mUploaded = values.at(3).toLongLong();
        }
        emit updated();
    }
}
