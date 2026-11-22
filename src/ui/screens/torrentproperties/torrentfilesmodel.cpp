// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "torrentfilesmodel.h"

#include <QStringBuilder>

#include "coroutines/threadpool.h"
#include "log/log.h"
#include "rpc/mounteddirectoriesutils.h"
#include "rpc/torrent.h"
#include "rpc/serversettings.h"
#include "rpc/rpc.h"
#include "ui/itemmodels/torrentfilestreebuilder.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        std::vector<int> idsFromIndexes(const QList<QModelIndex>& indexes) {
            std::vector<int> ids{};
            // at least indexes.size(), but may be more
            ids.reserve(static_cast<size_t>(indexes.size()));
            for (const QModelIndex& index : indexes) {
                auto entry = static_cast<TorrentFilesModelEntry*>(index.internalPointer());
                entry->getFileIds(ids);
            }
            std::ranges::sort(ids);
            const auto toErase = std::ranges::unique(ids);
            ids.erase(toErase.begin(), toErase.end());
            return ids;
        }

        std::pair<std::unique_ptr<TorrentFilesModelEntry>, std::vector<TorrentFilesModelEntry*>>
        doCreateTree(const std::vector<TorrentFile>& files, bool isMultiFile, QString rootName) {
            TorrentFilesTreeBuilder builder(files.size());
            if (isMultiFile) {
                builder.initializeRootDirectory(std::move(rootName));
            }
            for (const TorrentFile& file : files) {
                builder.addFile(
                    file.pathParts(),
                    !isMultiFile,
                    file.size,
                    file.completedSize,
                    file.wanted,
                    TorrentFilesModelEntry::fromFilePriority(file.priority)
                );
            }
            builder.calculateDirectoriesRecursively();
            return {std::move(builder.rootEntry), std::move(builder.files)};
        }
    }

    TorrentFilesModel::TorrentFilesModel(Rpc* rpc, QObject* parent)
        : BaseTorrentFilesModel(
              {Column::Name, Column::Size, Column::ProgressBar, Column::Priority}, parent
          ) {
        setRpc(rpc);
    }

    TorrentFilesModel::~TorrentFilesModel() {
        if (mTorrent) {
            mTorrent->setFilesEnabled(false);
        }
    }

    Torrent* TorrentFilesModel::torrent() const { return mTorrent; }

    void TorrentFilesModel::setTorrent(Torrent* torrent, bool oldTorrentDestroyed) {
        if (torrent == mTorrent) {
            return;
        }
        if (mTorrent && !oldTorrentDestroyed) {
            QObject::disconnect(mTorrent, nullptr, this, nullptr);
            mTorrent->setFilesEnabled(false);
        }
        mTorrent = torrent;
        mPendingTreeRebuild = false;
        resetTree();
        if (mTorrent) {
            QObject::connect(mTorrent, &Torrent::filesUpdated, this, &TorrentFilesModel::update);
            QObject::connect(mTorrent, &QObject::destroyed, this, [this] { setTorrent(nullptr, true); });
        }
    }

    void TorrentFilesModel::setFilesEnabled(bool enabled) {
        if (mTorrent) {
            mTorrent->setFilesEnabled(enabled);
        }
    }

    Rpc* TorrentFilesModel::rpc() const { return mRpc; }

    void TorrentFilesModel::setRpc(Rpc* rpc) { mRpc = rpc; }

    void TorrentFilesModel::setFilesWanted(const QModelIndexList& indexes, bool wanted) {
        BaseTorrentFilesModel::setFilesWanted(indexes, wanted);
        mTorrent->setFilesWanted(idsFromIndexes(indexes), wanted);
    }

    void
    TorrentFilesModel::setFilesPriority(const QModelIndexList& indexes, TorrentFilesModelEntry::Priority priority) {
        BaseTorrentFilesModel::setFilesPriority(indexes, priority);
        mTorrent->setFilesPriority(idsFromIndexes(indexes), TorrentFilesModelEntry::toFilePriority(priority));
    }

    QString TorrentFilesModel::localFilePath(const QModelIndex& index) const {
        if (!index.isValid()) {
            return localTorrentRootDirectoryPath(mRpc, mTorrent);
        }
        const auto* entry = static_cast<const TorrentFilesModelEntry*>(index.internalPointer());
        QString path(entry->path());
        return localTorrentDownloadDirectoryPath(mRpc, mTorrent) % '/' % path;
    }

    bool TorrentFilesModel::isWanted(const QModelIndex& index) const {
        if (!index.isValid()) {
            return true;
        }
        return static_cast<const TorrentFilesModelEntry*>(index.internalPointer())->wantedState()
               != TorrentFilesModelEntry::WantedState::Unwanted;
    }

    void TorrentFilesModel::update(std::span<const int> changed) {
        if (!mTorrent) {
            return;
        }
        if (mFiles.size() != mTorrent->files().size()) {
            if (mCreatingTree) {
                mPendingTreeRebuild = true;
            } else {
                mCoroutineScope.launch(createTree());
            }
            return;
        }
        if (mLoaded) {
            updateTree(changed);
        }
    }

    Coroutine<> TorrentFilesModel::createTree() {
        if (mCreatingTree) {
            co_return;
        }

        mCreatingTree = true;
        resetTree();

        const auto* const torrent = mTorrent;
        if (!torrent) {
            mCreatingTree = false;
            co_return;
        }

        // Snapshot the files: rtorrent updates can replace the torrent's
        // file list while the tree is being built on the thread pool.
        auto files = torrent->files();
        auto [rootEntry, treeFiles] = co_await runOnThreadPool(
            [](const std::vector<TorrentFile>& files, bool isMultiFile, QString rootName) {
                return doCreateTree(files, isMultiFile, std::move(rootName));
            },
            std::move(files),
            torrent->data().isMultiFile,
            torrent->data().name
        );

        if (mTorrent != torrent) {
            mCreatingTree = false;
            if (mPendingTreeRebuild) {
                mPendingTreeRebuild = false;
                mCoroutineScope.launch(createTree());
            }
            co_return;
        }

        beginResetModel();
        mRootEntry = std::move(rootEntry);
        mFiles = std::move(treeFiles);
        endResetModel();

        setLoaded(true);
        mCreatingTree = false;

        if (mPendingTreeRebuild) {
            mPendingTreeRebuild = false;
            mCoroutineScope.launch(createTree());
        } else if (mFiles.size() != mTorrent->files().size()) {
            mCoroutineScope.launch(createTree());
        }
    }

    void TorrentFilesModel::resetTree() {
        if (mLoaded) {
            beginResetModel();
            mRootEntry.reset();
            mFiles.clear();
            endResetModel();
            setLoaded(false);
        }
    }

    void TorrentFilesModel::updateTree(std::span<const int> changed) {
        const auto& jsons = mTorrent->files();
        updateFiles(changed, [&](size_t i, TorrentFilesModelEntry* file) {
            const auto& json = jsons.at(i);
            file->update(json.wanted, TorrentFilesModelEntry::fromFilePriority(json.priority), json.completedSize);
        });
    }

    void TorrentFilesModel::setLoaded(bool loaded) { mLoaded = loaded; }
}
