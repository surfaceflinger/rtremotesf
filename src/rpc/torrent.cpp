// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <climits>
#include <ranges>
#include <stdexcept>
#include <utility>

#include <QDateTime>
#include <QVariantList>

#include <fmt/ranges.h>

#include "rpc.h"
#include "serversettings.h"
#include "torrent.h"

#include "itemlistupdater.h"
#include "jsonutils.h"
#include "log/log.h"
#include "pathutils.h"
#include "stdutils.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    using namespace impl;

    enum class TorrentData::UpdateKey {
        HashString,
        Name,
        AddedDate,
        ActivityDate,
        TotalSize,
        CompletedSize,
        LeftUntilDone,
        DownloadSpeed,
        UploadSpeed,
        TotalDownloaded,
        TotalUploaded,
        IsMultiFile,
        DownloadDirectory,
        BasePath,
        Labels,
        ErrorString,
        PeersLimit,
        BandwidthPriority,
        State,
        Hashing,
        HashChecking,
        Complete,
        HashingFailed,
        IsMeta,
        PeersConnected,
        DoneDate,
        CreationDate,
        ChunkSize,
        ChunksHashed,
        ChunksTotal,
        WantedChunks,
        TrackerStats,

        Count
    };

    namespace {
        constexpr auto allUpdateKeys =
            std::views::iota(0, static_cast<int>(TorrentData::UpdateKey::Count))
            | std::views::transform([](int key) { return static_cast<TorrentData::UpdateKey>(key); });

        QString updateKeyCommand(TorrentData::UpdateKey key) {
            switch (key) {
            case TorrentData::UpdateKey::HashString:
                return "d.hash="_L1;
            case TorrentData::UpdateKey::Name:
                return "d.name="_L1;
            case TorrentData::UpdateKey::AddedDate:
                return "d.load_date="_L1;
            case TorrentData::UpdateKey::ActivityDate:
                return "d.timestamp.started="_L1;
            case TorrentData::UpdateKey::TotalSize:
                return "d.size_bytes="_L1;
            case TorrentData::UpdateKey::CompletedSize:
                return "d.completed_bytes="_L1;
            case TorrentData::UpdateKey::LeftUntilDone:
                return "d.left_bytes="_L1;
            case TorrentData::UpdateKey::DownloadSpeed:
                return "d.down.rate="_L1;
            case TorrentData::UpdateKey::UploadSpeed:
                return "d.up.rate="_L1;
            case TorrentData::UpdateKey::TotalDownloaded:
                return "d.down.total="_L1;
            case TorrentData::UpdateKey::TotalUploaded:
                return "d.up.total="_L1;
            case TorrentData::UpdateKey::IsMultiFile:
                return "d.is_multi_file="_L1;
            case TorrentData::UpdateKey::DownloadDirectory:
                return "d.directory="_L1;
            case TorrentData::UpdateKey::BasePath:
                return "d.base_path="_L1;
            case TorrentData::UpdateKey::Labels:
                return "d.custom1="_L1;
            case TorrentData::UpdateKey::ErrorString:
                return "d.message="_L1;
            case TorrentData::UpdateKey::PeersLimit:
                return "d.peers_max="_L1;
            case TorrentData::UpdateKey::BandwidthPriority:
                return "d.priority="_L1;
            case TorrentData::UpdateKey::State:
                return "d.state="_L1;
            case TorrentData::UpdateKey::Hashing:
                return "d.hashing="_L1;
            case TorrentData::UpdateKey::HashChecking:
                return "d.is_hash_checking="_L1;
            case TorrentData::UpdateKey::Complete:
                return "d.complete="_L1;
            case TorrentData::UpdateKey::HashingFailed:
                return "d.hashing_failed="_L1;
            case TorrentData::UpdateKey::IsMeta:
                return "d.is_meta="_L1;
            case TorrentData::UpdateKey::PeersConnected:
                return "d.peers_connected="_L1;
            case TorrentData::UpdateKey::DoneDate:
                return "d.timestamp.finished="_L1;
            case TorrentData::UpdateKey::CreationDate:
                return "d.creation_date="_L1;
            case TorrentData::UpdateKey::ChunkSize:
                return "d.chunk_size="_L1;
            case TorrentData::UpdateKey::ChunksHashed:
                return "d.chunks_hashed="_L1;
            case TorrentData::UpdateKey::ChunksTotal:
                return "d.size_chunks="_L1;
            case TorrentData::UpdateKey::WantedChunks:
                return "d.wanted_chunks="_L1;
            case TorrentData::UpdateKey::TrackerStats:
                return "t.multicall=,t.url=,t.group=,t.is_enabled=,t.is_usable=,t.is_busy=,t.scrape_complete=,t.scrape_incomplete=,t.latest_sum_peers=,t.success_time_next=,t.success_counter=,t.failed_counter="_L1;
            case TorrentData::UpdateKey::Count:
                return {};
            }
            return {};
        }

        constexpr auto priorityMapper = EnumMapper(
            std::array{
                EnumMapping(TorrentData::Priority::Low, 1),
                EnumMapping(TorrentData::Priority::Normal, 2),
                EnumMapping(TorrentData::Priority::High, 3),
                EnumMapping(TorrentData::Priority::Off, 0)
            }
        );

        void updateDateTimeFromSeconds(QDateTime& dateTime, const QVariant& value, bool& changed) {
            const auto seconds = value.toLongLong();
            const QDateTime newDateTime = seconds > 0 ? QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::utc())
                                                      : QDateTime{{}, {}, QTimeZone::utc()};
            setChanged(dateTime, newDateTime, changed);
        }
    }

    int TorrentData::priorityToInt(Priority value) { return priorityMapper.toJsonConstant(value); }

    bool TorrentData::update(const QVariantList& values, bool firstTime, const Rpc* rpc) {
        bool changed = false;
        if (static_cast<qsizetype>(values.size()) < static_cast<qsizetype>(UpdateKey::Count)) {
            warning().log(
                "Torrent update has fewer values than expected: {} vs {}",
                values.size(),
                std::to_underlying(UpdateKey::Count)
            );
        }
        qsizetype index = 0;
        for (auto key : allUpdateKeys) {
            if (index >= values.size()) {
                break;
            }
            updateProperty(key, values.at(index), changed, firstTime, rpc);
            ++index;
        }
        recomputeDerived(changed);
        return changed;
    }

    void TorrentData::updateProperty(
        TorrentData::UpdateKey key, const QVariant& value, bool& changed, bool firstTime, const Rpc* rpc
    ) {
        switch (key) {
        case TorrentData::UpdateKey::HashString:
            if (firstTime) {
                hashString = value.toString().toLower();
            }
            return;
        case TorrentData::UpdateKey::Name:
            setChanged(name, value.toString(), changed);
            return;
        case TorrentData::UpdateKey::AddedDate:
            updateDateTimeFromSeconds(addedDate, value, changed);
            return;
        case TorrentData::UpdateKey::ActivityDate:
            updateDateTimeFromSeconds(activityDate, value, changed);
            return;
        case TorrentData::UpdateKey::TotalSize:
            setChanged(totalSize, value.toLongLong(), changed);
            return;
        case TorrentData::UpdateKey::CompletedSize:
            setChanged(completedSize, value.toLongLong(), changed);
            return;
        case TorrentData::UpdateKey::LeftUntilDone:
            setChanged(leftUntilDone, value.toLongLong(), changed);
            return;
        case TorrentData::UpdateKey::DownloadSpeed:
            setChanged(downloadSpeed, value.toLongLong(), changed);
            return;
        case TorrentData::UpdateKey::UploadSpeed:
            setChanged(uploadSpeed, value.toLongLong(), changed);
            return;
        case TorrentData::UpdateKey::TotalDownloaded:
            setChanged(totalDownloaded, value.toLongLong(), changed);
            return;
        case TorrentData::UpdateKey::TotalUploaded:
            setChanged(totalUploaded, value.toLongLong(), changed);
            return;
        case TorrentData::UpdateKey::IsMultiFile:
            setChanged(isMultiFile, value.toBool() || value.toInt() != 0, changed);
            return;
        case TorrentData::UpdateKey::DownloadDirectory:
            setChanged(rootDirectory, normalizePath(value.toString(), rpc->serverSettings()->data().pathOs), changed);
            return;
        case TorrentData::UpdateKey::BasePath:
            setChanged(basePath, normalizePath(value.toString(), rpc->serverSettings()->data().pathOs), changed);
            return;
        case TorrentData::UpdateKey::Labels: {
            std::vector<QString> newLabels{};
            for (const auto& label : value.toString().split(QLatin1Char('\n'))) {
                if (!label.isEmpty()) {
                    newLabels.push_back(label);
                }
            }
            setChanged(labels, std::move(newLabels), changed);
            return;
        }
        case TorrentData::UpdateKey::ErrorString:
            setChanged(errorString, value.toString(), changed);
            return;
        case TorrentData::UpdateKey::PeersLimit:
            setChanged(peersLimit, value.toInt(), changed);
            return;
        case TorrentData::UpdateKey::BandwidthPriority: {
            Priority priority{};
            switch (value.toInt()) {
            case 0:
                priority = Priority::Off;
                break;
            case 1:
                priority = Priority::Low;
                break;
            case 3:
                priority = Priority::High;
                break;
            default:
                priority = Priority::Normal;
                break;
            }
            setChanged(bandwidthPriority, priority, changed);
            return;
        }
        case TorrentData::UpdateKey::State:
            setChanged(state, value.toInt(), changed);
            return;
        case TorrentData::UpdateKey::Hashing:
            setChanged(hashing, value.toBool() || value.toInt() != 0, changed);
            return;
        case TorrentData::UpdateKey::HashChecking:
            setChanged(hashChecking, value.toBool() || value.toInt() != 0, changed);
            return;
        case TorrentData::UpdateKey::Complete:
            setChanged(complete, value.toBool() || value.toInt() != 0, changed);
            return;
        case TorrentData::UpdateKey::HashingFailed:
            setChanged(hashingFailed, value.toBool() || value.toInt() != 0, changed);
            return;
        case TorrentData::UpdateKey::IsMeta:
            setChanged(isMeta, value.toBool() || value.toInt() != 0, changed);
            return;
        case TorrentData::UpdateKey::PeersConnected: {
            const auto peers = value.toInt();
            setChanged(peersConnected, peers, changed);
            return;
        }
        case TorrentData::UpdateKey::DoneDate:
            updateDateTimeFromSeconds(doneDate, value, changed);
            return;
        case TorrentData::UpdateKey::CreationDate:
            updateDateTimeFromSeconds(creationDate, value, changed);
            return;
        case TorrentData::UpdateKey::ChunkSize:
            setChanged(chunkSize, value.toLongLong(), changed);
            return;
        case TorrentData::UpdateKey::ChunksHashed:
            setChanged(chunksHashed, value.toInt(), changed);
            return;
        case TorrentData::UpdateKey::ChunksTotal:
            setChanged(chunksTotal, value.toInt(), changed);
            return;
        case TorrentData::UpdateKey::WantedChunks:
            setChanged(wantedChunks, value.toInt(), changed);
            return;
        case TorrentData::UpdateKey::TrackerStats: {
            std::vector<Tracker> newTrackers{};
            int seeders{};
            int leechers{};
            const auto rows = value.toList();
            newTrackers.reserve(static_cast<size_t>(rows.size()));
            int trackerId = 0;
            for (const auto& row : rows) {
                const auto fields = row.toList();
                if (fields.isEmpty()) {
                    continue;
                }
                newTrackers.emplace_back(trackerId, fields);
                seeders += newTrackers.back().seeders();
                leechers += newTrackers.back().leechers();
                ++trackerId;
            }
            setChanged(trackers, std::move(newTrackers), changed);
            setChanged(totalSeedersFromTrackersCount, seeders, changed);
            setChanged(totalLeechersFromTrackersCount, leechers, changed);
            return;
        }
        case TorrentData::UpdateKey::Count:
            throw std::logic_error("UpdateKey::Count should not be mapped");
        }
    }

    void TorrentData::recomputeDerived(bool& changed) {
        QString newDownloadDirectory = rootDirectory;
        if (isMultiFile && !newDownloadDirectory.isEmpty()) {
            const auto separatorIndex = newDownloadDirectory.lastIndexOf('/');
            if (separatorIndex > 0) {
                newDownloadDirectory = newDownloadDirectory.left(separatorIndex);
            } else if (separatorIndex == 0) {
                newDownloadDirectory = "/"_L1;
            }
        }
        setChanged(downloadDirectory, newDownloadDirectory, changed);
        setChanged(
            completedSize,
            std::clamp<qint64>(completedSize, 0, totalSize > 0 ? totalSize : completedSize),
            changed
        );
        // d.left_bytes covers the whole torrent, including files turned off.
        // Clamp it to the chunks rtorrent still wants, so partially selected
        // torrents report their real remaining size.
        const auto wantedLeft = chunkSize > 0 ? static_cast<qint64>(wantedChunks) * chunkSize : leftUntilDone;
        setChanged(leftUntilDone, std::min(leftUntilDone, wantedLeft), changed);
        const auto newSizeWhenDone = completedSize + leftUntilDone;
        setChanged(sizeWhenDone, newSizeWhenDone, changed);
        setChanged(
            percentDone,
            newSizeWhenDone > 0 ? static_cast<double>(completedSize) / newSizeWhenDone : 0.0,
            changed
        );
        setChanged(ratio, totalDownloaded > 0 ? static_cast<double>(totalUploaded) / totalDownloaded : 0.0, changed);
        setChanged(magnetLink, "magnet:?xt=urn:btih:"_L1 + hashString, changed);

        Status newStatus{};
        if (hashing || hashChecking) {
            newStatus = Status::Checking;
        } else if (state == 0) {
            newStatus = Status::Paused;
        } else if (complete || (wantedChunks == 0 && chunkSize > 0 && sizeWhenDone > 0)) {
            newStatus = Status::Seeding;
        } else {
            newStatus = Status::Downloading;
        }
        setChanged(status, newStatus, changed);

        Error newError{};
        if (hashingFailed) {
            newError = Error::LocalError;
        } else if (!errorString.isEmpty() && !errorString.startsWith("Tracker:"_L1)) {
            newError = Error::LocalError;
        }
        setChanged(error, newError, changed);

        int newEta{};
        if (leftUntilDone == 0) {
            newEta = 0;
        } else if (downloadSpeed > 0) {
            newEta = static_cast<int>(std::min<qint64>(leftUntilDone / downloadSpeed, INT_MAX));
        } else {
            newEta = -1;
        }
        setChanged(eta, newEta, changed);

        const double newRecheckProgress =
            (chunksTotal > 0 && (hashing || hashChecking)) ? static_cast<double>(chunksHashed) / chunksTotal : 1.0;
        setChanged(recheckProgress, newRecheckProgress, changed);
    }

    Torrent::Torrent(int id, const QString& hash, const QVariantList& values, Rpc* rpc, QObject* parent)
        : QObject(parent), mRpc(rpc) {
        mData.id = id;
        mData.hashString = hash;
        [[maybe_unused]] const bool changed = mData.update(values, true, rpc);
    }

    QStringList Torrent::updateFields() {
        QStringList fields{};
        for (auto key : allUpdateKeys) {
            fields.push_back(updateKeyCommand(key));
        }
        return fields;
    }

    void Torrent::setPeersLimit(int limit) {
        mData.peersLimit = limit;
        mRpc->setTorrentPeersLimit(mData.id, limit);
    }

    void Torrent::setBandwidthPriority(TorrentData::Priority priority) {
        mData.bandwidthPriority = priority;
        mRpc->setTorrentPriority(mData.id, priority);
    }

    void Torrent::addTrackers(std::span<const std::set<QString>> announceUrls) {
        int group = 0;
        for (const auto& tier : announceUrls) {
            for (const auto& announce : tier) {
                mRpc->addTorrentTracker(mData.id, announce, std::min(group, 32));
            }
            ++group;
        }
    }

    void Torrent::setFilesEnabled(bool enabled) {
        if (enabled != mFilesEnabled) {
            mFilesEnabled = enabled;
            if (mFilesEnabled) {
                mRpc->getTorrentFiles(mData.id);
            } else {
                mFiles.clear();
            }
        }
    }

    void Torrent::setFilesWanted(std::span<const int> fileIds, bool wanted) {
        mRpc->setTorrentFilesWanted(mData.id, fileIds, wanted);
    }

    void Torrent::setFilesPriority(std::span<const int> fileIds, TorrentFile::Priority priority) {
        mRpc->setTorrentFilesPriority(mData.id, fileIds, priority);
    }

    void Torrent::setPeersEnabled(bool enabled) {
        if (enabled != mPeersEnabled) {
            mPeersEnabled = enabled;
            if (mPeersEnabled) {
                mRpc->getTorrentPeers(mData.id);
            } else {
                mPeers.clear();
            }
        }
    }

    bool Torrent::update(const QVariantList& values) {
        const bool c = mData.update(values, false, mRpc);
        emit updated();
        if (c) {
            emit changed();
        }
        return c;
    }

    void Torrent::updateFiles(const QVariantList& rows) {
        std::vector<int> changed{};
        if (static_cast<size_t>(rows.size()) != mFiles.size()) {
            mFiles.clear();
            mFiles.reserve(static_cast<size_t>(rows.size()));
            changed.reserve(static_cast<size_t>(rows.size()));
            for (qsizetype i = 0; i < rows.size(); ++i) {
                const auto row = rows.at(i).toList();
                mFiles.emplace_back(row, mData.chunkSize);
                changed.push_back(static_cast<int>(i));
            }
        } else {
            for (qsizetype i = 0; i < rows.size(); ++i) {
                const auto row = rows.at(i).toList();
                if (row.isEmpty()) {
                    continue;
                }
                if (mFiles[static_cast<size_t>(i)].update(row, mData.chunkSize)) {
                    changed.push_back(static_cast<int>(i));
                }
            }
        }
        emit filesUpdated(changed);
    }

    namespace {
        struct NewPeer {
            QVariantList row{};
            QString address{};
        };

        class PeersListUpdater final : public ItemListUpdater<Peer, std::vector<NewPeer>> {
        public:
            PeersListUpdater() = default;

            std::vector<std::pair<int, int>> removedIndexRanges{};
            std::vector<std::pair<int, int>> changedIndexRanges{};
            int addedCount{};

        protected:
            std::vector<NewPeer>::iterator
            findNewItemForItem(std::vector<NewPeer>& newPeers, const Peer& peer) override {
                return std::ranges::find(newPeers, peer.address, &NewPeer::address);
            }

            void onAboutToRemoveItems(size_t, size_t) override {};

            void onRemovedItems(size_t first, size_t last) override {
                removedIndexRanges.emplace_back(static_cast<int>(first), static_cast<int>(last));
            }

            // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
            bool updateItem(Peer& peer, NewPeer&& newPeer) override { return peer.update(newPeer.row); }

            void onChangedItems(size_t first, size_t last) override {
                changedIndexRanges.emplace_back(static_cast<int>(first), static_cast<int>(last));
            }

            // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
            Peer createItemFromNewItem(NewPeer&& newPeer) override { return Peer(newPeer.address, newPeer.row); }

            void onAboutToAddItems(size_t) override {}

            void onAddedItems(size_t count) override { addedCount = static_cast<int>(count); };
        };
    }

    void Torrent::updatePeers(const QVariantList& rows) {
        std::vector<NewPeer> newPeers{};
        newPeers.reserve(static_cast<size_t>(rows.size()));
        for (const auto& rowValue : rows) {
            const auto row = rowValue.toList();
            if (row.isEmpty()) {
                continue;
            }
            const QString address = row.constFirst().toString();
            if (address.isEmpty()) {
                continue;
            }
            newPeers.push_back({.row = row, .address = address});
        }

        PeersListUpdater updater{};
        updater.update(mPeers, std::move(newPeers));

        emit peersUpdated(updater.removedIndexRanges, updater.changedIndexRanges, updater.addedCount);
    }
}

namespace fmt {
    format_context::iterator
    formatter<tremotesf::Torrent>::format(const tremotesf::Torrent& torrent, format_context& ctx) const {
        return fmt::format_to(ctx.out(), "Torrent(id={}, name={})", torrent.data().id, torrent.data().name);
    }
}
