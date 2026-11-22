// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_SAFEDELETE_H
#define TREMOTESF_RPC_SAFEDELETE_H

#include <QString>

namespace tremotesf::impl {
    /**
     * Whether it is safe to recursively delete path, which must be a child of the torrent's directory.
     */
    bool isSafeToDeletePath(const QString& path, const QString& directory);
}

#endif // TREMOTESF_RPC_SAFEDELETE_H
