// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "serversettings.h"

#include <QVariantMap>

#include "log/log.h"
#include "rpc.h"
#include "stdutils.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        constexpr int bytesInKilobyte = 1024;

        ServerSettingsData::EncryptionMode encryptionModeFromString(const QString& value) {
            if (value.contains("prefer"_L1)) {
                return ServerSettingsData::EncryptionMode::Preferred;
            }
            if (value.contains("require"_L1)) {
                return ServerSettingsData::EncryptionMode::Required;
            }
            return ServerSettingsData::EncryptionMode::Allowed;
        }

        QString encryptionModeToString(ServerSettingsData::EncryptionMode mode) {
            switch (mode) {
            case ServerSettingsData::EncryptionMode::Allowed:
                return "allow"_L1;
            case ServerSettingsData::EncryptionMode::Preferred:
                return "prefer"_L1;
            case ServerSettingsData::EncryptionMode::Required:
                return "require"_L1;
            }
            return "allow"_L1;
        }
    }

    QStringList ServerSettings::requestCommands() {
        return {
            "session.path"_L1,
            "system.api_version"_L1,
            "directory.default"_L1,
            "throttle.global_down.max_rate"_L1,
            "throttle.global_up.max_rate"_L1,
            "network.listen.port"_L1,
            "network.listen.port.random"_L1,
            "protocol.encryption"_L1,
            "protocol.pex"_L1,
            "dht.statistics"_L1,
            "throttle.max_peers.normal"_L1
        };
    }

    ServerSettings::ServerSettings(Rpc* rpc, QObject* parent) : QObject(parent), mRpc(rpc), mSaveOnSet(true) {}

    void ServerSettings::setDownloadDirectory(const QString& directory) {
        if (directory != mData.downloadDirectory) {
            mData.downloadDirectory = directory;
            if (mSaveOnSet) save();
        }
    }

    void ServerSettings::setDownloadSpeedLimited(bool limited) {
        if (limited != mData.downloadSpeedLimited) {
            mData.downloadSpeedLimited = limited;
            if (mSaveOnSet) save();
        }
    }

    void ServerSettings::setDownloadSpeedLimit(int limit) {
        if (limit != mData.downloadSpeedLimit) {
            mData.downloadSpeedLimit = limit;
            if (mSaveOnSet) save();
        }
    }

    void ServerSettings::setUploadSpeedLimited(bool limited) {
        if (limited != mData.uploadSpeedLimited) {
            mData.uploadSpeedLimited = limited;
            if (mSaveOnSet) save();
        }
    }

    void ServerSettings::setUploadSpeedLimit(int limit) {
        if (limit != mData.uploadSpeedLimit) {
            mData.uploadSpeedLimit = limit;
            if (mSaveOnSet) save();
        }
    }

    void ServerSettings::setPeerPort(int port) {
        if (port != mData.peerPort) {
            mData.peerPort = port;
            if (mSaveOnSet) save();
        }
    }

    void ServerSettings::setRandomPortEnabled(bool enabled) {
        if (enabled != mData.randomPortEnabled) {
            mData.randomPortEnabled = enabled;
            if (mSaveOnSet) save();
        }
    }

    void ServerSettings::setEncryptionMode(ServerSettingsData::EncryptionMode mode) {
        if (mode != mData.encryptionMode) {
            mData.encryptionMode = mode;
            if (mSaveOnSet) save();
        }
    }

    void ServerSettings::setPexEnabled(bool enabled) {
        if (enabled != mData.pexEnabled) {
            mData.pexEnabled = enabled;
            if (mSaveOnSet) save();
        }
    }

    void ServerSettings::setDhtEnabled(bool enabled) {
        if (enabled != mData.dhtEnabled) {
            mData.dhtEnabled = enabled;
            if (mSaveOnSet) save();
        }
    }

    void ServerSettings::setMaximumPeersPerTorrent(int peers) {
        if (peers != mData.maximumPeersPerTorrent) {
            mData.maximumPeersPerTorrent = peers;
            if (mSaveOnSet) save();
        }
    }

    bool ServerSettings::saveOnSet() const { return mSaveOnSet; }

    void ServerSettings::setSaveOnSet(bool save) { mSaveOnSet = save; }

    void ServerSettings::update(const QVariantList& values) {
        if (values.size() < requestCommands().size()) {
            warning().log("Server settings update has fewer values than expected: {}", values.size());
            return;
        }

        bool changed = false;

        const auto apiVersion = values.at(1).toInt();
        setChanged(mData.apiVersion, apiVersion, changed);

        if (const auto configDirectory = values.at(0).toString(); configDirectory != mData.configDirectory) {
            mData.configDirectory = configDirectory;
            mData.pathOs = PathOs::Unix;
            changed = true;
        }

        setChanged(mData.downloadDirectory, normalizePath(values.at(2).toString(), PathOs::Unix), changed);

        const auto downloadLimit = values.at(3).toLongLong() / bytesInKilobyte;
        setChanged(mData.downloadSpeedLimited, downloadLimit > 0, changed);
        setChanged(mData.downloadSpeedLimit, static_cast<int>(downloadLimit), changed);

        const auto uploadLimit = values.at(4).toLongLong() / bytesInKilobyte;
        setChanged(mData.uploadSpeedLimited, uploadLimit > 0, changed);
        setChanged(mData.uploadSpeedLimit, static_cast<int>(uploadLimit), changed);

        setChanged(mData.peerPort, values.at(5).toInt(), changed);
        setChanged(mData.randomPortEnabled, values.at(6).toBool() || values.at(6).toInt() != 0, changed);
        setChanged(mData.encryptionMode, encryptionModeFromString(values.at(7).toString()), changed);
        setChanged(mData.pexEnabled, values.at(8).toBool() || values.at(8).toInt() != 0, changed);

        const auto dht = values.at(9).toMap().value("dht"_L1).toString();
        setChanged(mData.dhtEnabled, dht != "disable"_L1 && dht != "off"_L1, changed);

        setChanged(mData.maximumPeersPerTorrent, values.at(10).toInt(), changed);

        if (changed) {
            emit this->changed();
        }
    }

    void ServerSettings::save() const {
        mRpc->setSessionProperties(
            {{"directory.default.set"_L1, mData.downloadDirectory},
             {"throttle.global_down.max_rate.set"_L1,
              mData.downloadSpeedLimited ? mData.downloadSpeedLimit * bytesInKilobyte : 0},
             {"throttle.global_up.max_rate.set"_L1,
              mData.uploadSpeedLimited ? mData.uploadSpeedLimit * bytesInKilobyte : 0},
             {"network.listen.port.set"_L1, mData.peerPort},
             {"network.listen.port.random.set"_L1, mData.randomPortEnabled},
             {"protocol.encryption.set"_L1, encryptionModeToString(mData.encryptionMode)},
             {"protocol.pex.set"_L1, mData.pexEnabled},
             {"dht.mode.set"_L1, mData.dhtEnabled ? "auto"_L1 : "disable"_L1},
             {"throttle.max_peers.normal.set"_L1, mData.maximumPeersPerTorrent}}
        );
    }
}
