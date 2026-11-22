// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_TORRENT_H
#define TREMOTESF_RPC_TORRENT_H

#include <optional>
#include <set>
#include <span>
#include <vector>

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QTimeZone>
#include <QVariant>

#include "log/formatters.h"
#include "peer.h"
#include "torrentfile.h"
#include "tracker.h"

namespace tremotesf {
    class Rpc;
    class ServerSettings;

    struct TorrentData {
        Q_GADGET
    public:
        enum class Status {
            Paused,
            QueuedForChecking,
            Checking,
            QueuedForDownloading,
            Downloading,
            QueuedForSeeding,
            Seeding
        };
        Q_ENUM(Status)

        enum class Error { None, TrackerWarning, TrackerError, LocalError };
        Q_ENUM(Error)

        enum class Priority { Low, Normal, High };
        Q_ENUM(Priority)
        static int priorityToInt(Priority value);

        [[nodiscard]] bool update(const QVariantList& values, bool firstTime, const Rpc* rpc);

        enum class UpdateKey;

        int id{};
        QString hashString{};
        QString name{};
        QString magnetLink{};

        Status status{};
        Error error{};
        QString errorString{};

        qint64 totalSize{};
        qint64 completedSize{};
        qint64 leftUntilDone{};
        qint64 sizeWhenDone{};

        double percentDone{};
        double recheckProgress{};
        int eta{};

        bool metadataComplete{};

        qint64 downloadSpeed{};
        qint64 uploadSpeed{};

        qint64 totalDownloaded{};
        qint64 totalUploaded{};
        double ratio{};

        int totalSeedersFromTrackersCount{};
        int peersSendingToUsCount{};
        int webSeedersSendingToUsCount{};
        int totalLeechersFromTrackersCount{};
        int peersGettingFromUsCount{};

        int peersLimit{};

        QDateTime addedDate{{}, {}, QTimeZone::utc()};
        QDateTime activityDate{{}, {}, QTimeZone::utc()};
        QDateTime doneDate{{}, {}, QTimeZone::utc()};

        QString downloadDirectory{};
        QString rootDirectory{};
        QString basePath{};
        bool isMultiFile{};
        QDateTime creationDate{{}, {}, QTimeZone::utc()};
        Priority bandwidthPriority{};

        std::vector<QString> labels{};

        std::vector<Tracker> trackers{};

        qint64 chunkSize{};
        int chunksHashed{};
        int chunksTotal{};
        int wantedChunks{};

        int state{};
        bool hashing{};
        bool hashChecking{};
        bool complete{};
        bool hashingFailed{};
        bool isMeta{};

        [[nodiscard]] bool hasError() const { return error != Error::None; }
        [[nodiscard]] bool isFinished() const { return leftUntilDone == 0; }
        [[nodiscard]] bool isDownloadingStalled() const {
            return (peersSendingToUsCount == 0 && webSeedersSendingToUsCount == 0);
        }
        [[nodiscard]] bool isSeedingStalled() const { return peersGettingFromUsCount == 0; }

    private:
        void updateProperty(
            TorrentData::UpdateKey key, const QVariant& value, bool& changed, bool firstTime, const Rpc* rpc
        );
        void recomputeDerived(bool& changed);
    };

    class Torrent final : public QObject {
        Q_OBJECT

    public:
        explicit Torrent(int id, const QString& hash, const QVariantList& values, Rpc* rpc, QObject* parent = nullptr);
        // For testing only
        explicit Torrent() = default;

        [[nodiscard]] static QStringList updateFields();

        void setPeersLimit(int limit);
        void setBandwidthPriority(TorrentData::Priority priority);

        void addTrackers(std::span<const std::set<QString>> announceUrls);

        [[nodiscard]] const TorrentData& data() const { return mData; };

        [[nodiscard]] bool isFilesEnabled() const { return mFilesEnabled; };
        void setFilesEnabled(bool enabled);
        [[nodiscard]] const std::vector<TorrentFile>& files() const { return mFiles; };

        void setFilesWanted(std::span<const int> fileIds, bool wanted);
        void setFilesPriority(std::span<const int> fileIds, TorrentFile::Priority priority);

        [[nodiscard]] bool isPeersEnabled() const { return mPeersEnabled; };
        void setPeersEnabled(bool enabled);
        [[nodiscard]] const std::vector<Peer>& peers() const { return mPeers; };

        [[nodiscard]] bool update(const QVariantList& values);
        void updateFiles(const QVariantList& rows);
        void updatePeers(const QVariantList& rows);

    private:
        Rpc* mRpc{};

        TorrentData mData{};

        std::vector<TorrentFile> mFiles{};
        bool mFilesEnabled{};

        std::vector<Peer> mPeers{};
        bool mPeersEnabled{};
    signals:
        void updated();
        void changed();

        void filesUpdated(const std::vector<int>& changedIndexes);
        void peersUpdated(
            const std::vector<std::pair<int, int>>& removedIndexRanges,
            const std::vector<std::pair<int, int>>& changedIndexRanges,
            int addedCount
        );
    };
}

namespace fmt {
    template<>
    struct formatter<tremotesf::Torrent> : tremotesf::SimpleFormatter {
        format_context::iterator format(const tremotesf::Torrent& torrent, format_context& ctx) const;
    };
}

#endif // TREMOTESF_RPC_TORRENT_H
