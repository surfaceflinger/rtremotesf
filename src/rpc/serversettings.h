// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_SERVERSETTINGS_H
#define TREMOTESF_RPC_SERVERSETTINGS_H

#include <QObject>
#include <QStringList>
#include <QVariantList>

#include "pathutils.h"

namespace tremotesf {
    class Rpc;

    struct ServerSettingsData {
        Q_GADGET
    public:
        enum class EncryptionMode { Allowed, Preferred, Required };
        Q_ENUM(EncryptionMode)

        int apiVersion = 0;

        QString configDirectory;
        PathOs pathOs = PathOs::Unix;

        QString downloadDirectory;

        bool downloadSpeedLimited = false;
        int downloadSpeedLimit = 0;
        bool uploadSpeedLimited = false;
        int uploadSpeedLimit = 0;

        int peerPort = 0;
        bool randomPortEnabled = false;

        EncryptionMode encryptionMode{};
        bool pexEnabled = false;
        bool dhtEnabled = false;

        int maximumPeersPerTorrent = 0;
    };

    class ServerSettings final : public QObject {
        Q_OBJECT

    public:
        explicit ServerSettings(Rpc* rpc = nullptr, QObject* parent = nullptr);

        static QStringList requestCommands();

        void setDownloadDirectory(const QString& directory);

        void setDownloadSpeedLimited(bool limited);
        void setDownloadSpeedLimit(int limit);
        void setUploadSpeedLimited(bool limited);
        void setUploadSpeedLimit(int limit);

        void setPeerPort(int port);
        void setRandomPortEnabled(bool enabled);
        void setEncryptionMode(ServerSettingsData::EncryptionMode mode);
        void setPexEnabled(bool enabled);
        void setDhtEnabled(bool enabled);
        void setMaximumPeersPerTorrent(int peers);

        [[nodiscard]] bool saveOnSet() const;
        void setSaveOnSet(bool save);

        void update(const QVariantList& values);
        void save() const;

        [[nodiscard]] const ServerSettingsData& data() const { return mData; };

    private:
        Rpc* mRpc;
        ServerSettingsData mData;
        bool mSaveOnSet;

    signals:
        void changed();
    };
}

#endif // TREMOTESF_RPC_SERVERSETTINGS_H
