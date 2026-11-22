// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_RPC_H
#define TREMOTESF_RPC_RPC_H

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <QByteArray>
#include <QObject>
#include <QVariantMap>

#include "coroutines/scope.h"
#include "requestrouter.h"
#include "torrent.h"

#ifdef Q_MOC_RUN
#    include "serversettings.h"
#    include "serverstats.h"
#else
namespace tremotesf {
    class ServerSettings;
    class ServerStats;
}
#endif

namespace tremotesf {
    Q_NAMESPACE

    namespace impl {
        class RequestRouter;
    }

    struct ConnectionConfiguration {
        Q_GADGET
    public:
        enum class ProxyType { Default, Http, Socks5, None };
        Q_ENUM(ProxyType)

        QString address{};
        int port{};
        QString socketPath{};

        ProxyType proxyType{ProxyType::Default};
        QString proxyHostname{};
        int proxyPort{};
        QString proxyUser{};
        QString proxyPassword{};

        int updateInterval{};
        int timeout{};

        bool autoReconnectEnabled{};
        int autoReconnectInterval{};

        bool operator==(const ConnectionConfiguration& rhs) const = default;
    };

    enum class RpcConnectionState { Disconnected, Connecting, Connected };
    Q_ENUM_NS(RpcConnectionState)

    enum class RpcError { NoError, TimedOut, ConnectionError, ParseError };
    Q_ENUM_NS(RpcError)

    class Rpc : public QObject {
        Q_OBJECT
    public:
        using ConnectionState = RpcConnectionState;
        using Error = RpcError;

        explicit Rpc(QObject* parent = nullptr);
        ~Rpc() override;
        Q_DISABLE_COPY_MOVE(Rpc)

        ServerSettings* serverSettings() const;
        ServerStats* serverStats() const;

        const std::vector<std::unique_ptr<Torrent>>& torrents() const;
        Torrent* torrentByHash(const QString& hash) const;
        Torrent* torrentById(int id) const;

        struct Status {
            ConnectionState connectionState{ConnectionState::Disconnected};
            Error error{Error::NoError};
            QString errorMessage{};
            QString detailedErrorMessage{};

            bool operator==(const Status& other) const = default;

            QString toString() const;
        };

        bool isConnected() const;
        const Status& status() const;
        ConnectionState connectionState() const;
        Error error() const;
        const QString& errorMessage() const;
        const QString& detailedErrorMessage() const;
        bool isLocal() const;

        int torrentsCount() const;

        void setConnectionConfiguration(const ConnectionConfiguration& configuration);
        void resetConnectionConfiguration();

        void connect();
        void disconnect();

        enum class DeleteFileMode { No, Delete, MoveToTrash };

        void addTorrentFile(
            QString filePath,
            QString downloadDirectory,
            std::vector<int> unwantedFiles,
            std::vector<int> highPriorityFiles,
            std::vector<int> lowPriorityFiles,
            TorrentData::Priority bandwidthPriority,
            bool start,
            DeleteFileMode deleteFileMode,
            std::vector<QString> labels
        );

        void addTorrentLinks(
            QStringList links,
            QString downloadDirectory,
            TorrentData::Priority bandwidthPriority,
            bool start,
            std::vector<QString> labels
        );

        void startTorrents(std::span<const int> ids);
        void startTorrentsNow(std::span<const int> ids);
        void pauseTorrents(std::span<const int> ids);
        void removeTorrents(std::span<const int> ids, bool deleteFiles);
        void checkTorrents(std::span<const int> ids);
        void reannounceTorrents(std::span<const int> ids);

        void setSessionProperty(QString command, QVariant value);
        void setSessionProperties(QVariantMap properties);
        void setTorrentsLabels(std::span<const int> ids, std::span<const QString> labels);
        void addTorrentTracker(int id, const QString& announce, int group);
        void setTorrentPriority(int id, TorrentData::Priority priority);
        void setTorrentPeersLimit(int id, int limit);
        void setTorrentFilesWanted(int id, std::span<const int> fileIds, bool wanted);
        void setTorrentFilesPriority(int id, std::span<const int> fileIds, TorrentFile::Priority priority);
        void getTorrentFiles(int torrentId);
        void getTorrentPeers(int torrentId);

        void shutdownServer();

    private:
        void setStatus(Status&& status);
        void resetStateOnConnectionStateChanged(ConnectionState oldConnectionState);
        void emitSignalsOnConnectionStateChanged(ConnectionState oldConnectionState);

        struct Command {
            QString method;
            QVariantList parameters;
        };

        Coroutine<impl::RequestRouter::Response> call(Command command, bool updateIfSuccessful = false);
        Coroutine<impl::RequestRouter::Response>
        callBatch(std::vector<Command> commands, bool updateIfSuccessful = false);

        QString targetForTorrent(int id, const QString& suffix = {}) const;
        std::vector<Command> commandsForTorrents(std::span<const int> ids, QString method) const;

        Coroutine<> addTorrentFileImpl(
            QString filePath,
            QString downloadDirectory,
            std::vector<int> unwantedFiles,
            std::vector<int> highPriorityFiles,
            std::vector<int> lowPriorityFiles,
            TorrentData::Priority bandwidthPriority,
            bool start,
            DeleteFileMode deleteFileMode,
            std::vector<QString> labels
        );
        Coroutine<> addTorrentLinksImpl(
            QStringList links,
            QString downloadDirectory,
            TorrentData::Priority bandwidthPriority,
            bool start,
            std::vector<QString> labels
        );
        Coroutine<> addTorrentLink(QString method, QVariantList parameters, QString link);
        Coroutine<> applyAddedTorrentSettings(
            const QString& hash,
            QString downloadDirectory,
            std::span<const int> unwantedFiles,
            std::span<const int> highPriorityFiles,
            std::span<const int> lowPriorityFiles,
            TorrentData::Priority bandwidthPriority,
            std::span<const QString> labels
        );
        Coroutine<> removeTorrentsImpl(std::vector<QString> hashes, bool deleteFiles);
        Coroutine<> getTorrentFile(QString hash, QStringList fields);
        Coroutine<> getTorrentsFiles(std::vector<QString> hashes);
        Coroutine<> getTorrentPeer(QString hash, QStringList fields);
        Coroutine<> getTorrentsPeers(std::vector<QString> hashes);
        Coroutine<> shutdownServerImpl();

        Coroutine<> getServerSettings();
        Coroutine<> getTorrents();
        Coroutine<> getServerStats();

        Coroutine<> connectAndPerformDataUpdates();
        Coroutine<> updateData();

        Coroutine<> checkIfServerIsLocal();

        void onRequestFailed(RpcError error, const QString& errorMessage, const QString& detailedErrorMessage);
        Coroutine<> autoReconnect();

        impl::RequestRouter* mRequestRouter{};

        bool mAutoReconnectEnabled{};
        bool mUpdateInProgress{};
        bool mUpdateRequested{};

        std::optional<bool> mServerIsLocal{};

        std::chrono::seconds mUpdateInterval{};
        std::chrono::seconds mAutoReconnectInterval{};

        ServerSettings* mServerSettings{};
        std::vector<std::unique_ptr<Torrent>> mTorrents{};
        ServerStats* mServerStats{};
        int mNextTorrentId{};

        Status mStatus{};

        // Coroutine scopes must be destroyed before the data they use.
        CoroutineScope mBackgroundRequestsCoroutineScope{};
        CoroutineScope mAutoReconnectCoroutineScope{};
        CoroutineScope mDeletingFilesCoroutineScope{};

    signals:
        void aboutToDisconnect();
        void statusChanged();
        void connectedChanged();
        void connectionStateChanged();

        void onAboutToRemoveTorrents(size_t first, size_t last);
        void onRemovedTorrents(size_t first, size_t last);
        void onChangedTorrents(size_t first, size_t last);
        void onAboutToAddTorrents(size_t count);
        void onAddedTorrents(size_t count);

        void torrentsUpdated();

        void torrentAdded(tremotesf::Torrent* torrent);
        void torrentFinished(tremotesf::Torrent* torrent);

        void torrentAddDuplicate();
        void torrentAddError(const QString& filePathOrUrl);
    };
}

#endif // TREMOTESF_RPC_RPC_H
