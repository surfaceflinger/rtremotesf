// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mounteddirectoriesutils.h"

#include "rpc.h"
#include "serversettings.h"
#include "servers.h"

#include <QFileInfo>
#include <QStringBuilder>

using namespace Qt::StringLiterals;

namespace tremotesf {
    bool isServerLocalOrTorrentIsMounted(const Rpc* rpc, const Torrent* torrent) {
        if (rpc->isLocal()) {
            return true;
        }
        if (!Servers::instance()->currentServerHasMountedDirectories()) {
            return false;
        }
        return !localTorrentDownloadDirectoryPath(rpc, torrent).isEmpty();
    }

    QString localTorrentDownloadDirectoryPath(const Rpc* rpc, const Torrent* torrent) {
        const auto serverSettings = rpc->serverSettings();
        QString directory = torrent->data().downloadDirectory;
        if (!rpc->isLocal() && !directory.isEmpty() && Servers::instance()->currentServerHasMountedDirectories()) {
            directory = Servers::instance()->fromRemoteToLocalDirectory(directory, serverSettings);
        }
        return directory;
    }

    QString localTorrentRootDirectoryPath(const Rpc* rpc, const Torrent* torrent) {
        const auto serverSettings = rpc->serverSettings();
        auto directory = torrent->data().rootDirectory;
        if (!rpc->isLocal() && !directory.isEmpty() && Servers::instance()->currentServerHasMountedDirectories()) {
            directory = Servers::instance()->fromRemoteToLocalDirectory(directory, serverSettings);
        }
        return directory;
    }

    QString localTorrentRootFilePath(const Rpc* rpc, const Torrent* torrent) {
        auto path = torrent->data().basePath;
        if (path.isEmpty()) {
            const QString downloadDirectoryPath = localTorrentDownloadDirectoryPath(rpc, torrent);
            if (downloadDirectoryPath.isEmpty()) {
                return {};
            }
            path = downloadDirectoryPath % '/' % torrent->data().name;
        } else if (!rpc->isLocal() && Servers::instance()->currentServerHasMountedDirectories()) {
            path = Servers::instance()->fromRemoteToLocalDirectory(path, rpc->serverSettings());
        }
        return path;
    }
}
