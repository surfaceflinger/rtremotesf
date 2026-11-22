// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "rpc.h"
#include "safedelete.h"

#include <ranges>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QHostInfo>
#include <QNetworkProxy>
#include <QStringList>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "addressutils.h"
#include "coroutines/hostinfo.h"
#include "coroutines/threadpool.h"
#include "coroutines/timer.h"
#include "coroutines/waitall.h"
#include "fileutils.h"
#include "itemlistupdater.h"
#include "log/log.h"
#include "serversettings.h"
#include "serverstats.h"
#include "torrentfileparser.h"

using namespace Qt::StringLiterals;

SPECIALIZE_FORMATTER_FOR_QDEBUG(QHostAddress)

namespace tremotesf {
    using namespace impl;

    namespace {
        constexpr auto torrentsView = "main"_L1;

        QString quotedRtorrentValue(QString value) {
            value.replace(QLatin1Char('\\'), "\\\\"_L1);
            value.replace(QLatin1Char('"'), "\\\""_L1);
            return '"' + value + '"';
        }

        Coroutine<> deleteTorrentFile(QString filePath, bool moveToTrash) {
            co_await runOnThreadPool([moveToTrash, filePath = std::move(filePath)] {
                try {
                    if (moveToTrash) {
                        moveFileToTrashOrDelete(filePath);
                    } else {
                        deleteFile(filePath);
                    }
                } catch (const QFileError& e) {
                    warning().logWithException(e, "Failed to delete torrent file");
                }
            });
        }
    }

    QString Rpc::Status::toString() const {
        switch (connectionState) {
        case ConnectionState::Disconnected:
            switch (error) {
            case Error::NoError:
                //: Server connection status
                return qApp->translate("tremotesf", "Disconnected");
            case Error::TimedOut:
                //: Server connection status
                return qApp->translate("tremotesf", "Timed out");
            case Error::ConnectionError:
                //: Server connection status
                return qApp->translate("tremotesf", "Connection error");
            case Error::ParseError:
                //: Server connection status
                return qApp->translate("tremotesf", "Parse error");
            }
            break;
        case ConnectionState::Connecting:
            //: Server connection status
            return qApp->translate("tremotesf", "Connecting...");
        case ConnectionState::Connected:
            //: Server connection status
            return qApp->translate("tremotesf", "Connected");
        }

        return {};
    }

    Rpc::Rpc(QObject* parent)
        : QObject(parent),
          mRequestRouter(new RequestRouter(this)),
          mServerSettings(new ServerSettings(this, this)),
          mServerStats(new ServerStats(this)) {
        QObject::connect(mRequestRouter, &RequestRouter::requestFailed, this, &Rpc::onRequestFailed);
    }

    Rpc::~Rpc() = default;

    ServerSettings* Rpc::serverSettings() const { return mServerSettings; }

    ServerStats* Rpc::serverStats() const { return mServerStats; }

    const std::vector<std::unique_ptr<Torrent>>& Rpc::torrents() const { return mTorrents; }

    Torrent* Rpc::torrentByHash(const QString& hash) const {
        for (const std::unique_ptr<Torrent>& torrent : mTorrents) {
            if (torrent->data().hashString == hash) {
                return torrent.get();
            }
        }
        return nullptr;
    }

    Torrent* Rpc::torrentById(int id) const {
        const auto found = std::ranges::find(mTorrents, id, [](const auto& torrent) { return torrent->data().id; });
        return (found != mTorrents.end()) ? found->get() : nullptr;
    }

    bool Rpc::isConnected() const { return (mStatus.connectionState == ConnectionState::Connected); }

    const Rpc::Status& Rpc::status() const { return mStatus; }

    Rpc::ConnectionState Rpc::connectionState() const { return mStatus.connectionState; }

    Rpc::Error Rpc::error() const { return mStatus.error; }

    const QString& Rpc::errorMessage() const { return mStatus.errorMessage; }

    const QString& Rpc::detailedErrorMessage() const { return mStatus.detailedErrorMessage; }

    bool Rpc::isLocal() const { return mServerIsLocal.value_or(false); }

    int Rpc::torrentsCount() const { return static_cast<int>(mTorrents.size()); }

    void Rpc::setConnectionConfiguration(const ConnectionConfiguration& configuration) {
        disconnect();

        RequestRouter::RequestsConfiguration requestsConfig{};
        requestsConfig.address = configuration.address;
        requestsConfig.port = static_cast<quint16>(configuration.port);
        requestsConfig.socketPath = configuration.socketPath;

        switch (configuration.proxyType) {
        case ConnectionConfiguration::ProxyType::Default:
            break;
        case ConnectionConfiguration::ProxyType::Http:
            requestsConfig.proxy = QNetworkProxy(
                QNetworkProxy::HttpProxy,
                configuration.proxyHostname,
                static_cast<quint16>(configuration.proxyPort),
                configuration.proxyUser,
                configuration.proxyPassword
            );
            break;
        case ConnectionConfiguration::ProxyType::Socks5:
            requestsConfig.proxy = QNetworkProxy(
                QNetworkProxy::Socks5Proxy,
                configuration.proxyHostname,
                static_cast<quint16>(configuration.proxyPort),
                configuration.proxyUser,
                configuration.proxyPassword
            );
            break;
        case ConnectionConfiguration::ProxyType::None:
            requestsConfig.proxy = QNetworkProxy(QNetworkProxy::NoProxy);
            break;
        }

        requestsConfig.timeout = std::chrono::seconds(configuration.timeout);
        mRequestRouter->setConfiguration(std::move(requestsConfig));

        mUpdateInterval = std::chrono::seconds(configuration.updateInterval);

        mAutoReconnectEnabled = configuration.autoReconnectEnabled;
        mAutoReconnectInterval = std::chrono::seconds(configuration.autoReconnectInterval);
    }

    void Rpc::resetConnectionConfiguration() {
        disconnect();
        mRequestRouter->resetConfiguration();
        mAutoReconnectEnabled = false;
        mAutoReconnectCoroutineScope.cancelAll();
    }

    void Rpc::connect() {
        if (connectionState() == ConnectionState::Disconnected && mRequestRouter->configuration().has_value()) {
            setStatus(Status{.connectionState = ConnectionState::Connecting});
            mBackgroundRequestsCoroutineScope.launch(connectAndPerformDataUpdates());
        }
    }

    void Rpc::disconnect() { setStatus(Status{.connectionState = ConnectionState::Disconnected}); }

    QString Rpc::targetForTorrent(int id, const QString& suffix) const {
        const auto torrent = torrentById(id);
        if (!torrent) {
            return {};
        }
        return torrent->data().hashString + suffix;
    }

    std::vector<Rpc::Command> Rpc::commandsForTorrents(std::span<const int> ids, QString method) const {
        std::vector<Command> commands{};
        commands.reserve(ids.size());
        for (const int id : ids) {
            if (auto target = targetForTorrent(id); !target.isEmpty()) {
                commands.push_back({std::move(method), {std::move(target)}});
            }
        }
        return commands;
    }

    Coroutine<RequestRouter::Response> Rpc::call(Command command, bool updateIfSuccessful) {
        auto response = co_await mRequestRouter->call(command.method, std::move(command.parameters));
        if (updateIfSuccessful && response.success && isConnected()) {
            mBackgroundRequestsCoroutineScope.launch(updateData());
        }
        co_return response;
    }

    Coroutine<RequestRouter::Response> Rpc::callBatch(std::vector<Command> commands, bool updateIfSuccessful) {
        if (commands.empty()) {
            co_return RequestRouter::Response{.value = QVariantList{}, .success = true};
        }

        QVariantList calls{};
        calls.reserve(static_cast<qsizetype>(commands.size()));
        for (auto& command : commands) {
            calls.append(QVariantMap{{u"methodName"_s, command.method}, {u"params"_s, std::move(command.parameters)}});
        }

        auto response = co_await mRequestRouter->call("system.multicall"_L1, {std::move(calls)});
        if (!response.success) {
            co_return response;
        }

        QVariantList values{};
        bool success = true;
        QString errorMessage{};
        for (const auto& result : response.value.toList()) {
            if (result.typeId() == QMetaType::QVariantMap) {
                if (success) {
                    const auto fault = result.toMap();
                    errorMessage = fault.value("faultString"_L1).toString();
                }
                success = false;
                values.append(QVariant{});
            } else {
                const auto list = result.toList();
                values.append(list.isEmpty() ? QVariant{} : list.constFirst());
            }
        }

        if (updateIfSuccessful && success && isConnected()) {
            mBackgroundRequestsCoroutineScope.launch(updateData());
        }
        co_return RequestRouter::Response{
            .value = std::move(values),
            .success = success,
            .errorMessage = std::move(errorMessage)
        };
    }

    void Rpc::addTorrentFile(
        QString filePath,
        QString downloadDirectory,
        std::vector<int> unwantedFiles,
        std::vector<int> highPriorityFiles,
        std::vector<int> lowPriorityFiles,
        TorrentData::Priority bandwidthPriority,
        bool start,
        DeleteFileMode deleteFileMode,
        std::vector<QString> labels
    ) {
        if (isConnected()) {
            mBackgroundRequestsCoroutineScope.launch(addTorrentFileImpl(
                std::move(filePath),
                std::move(downloadDirectory),
                std::move(unwantedFiles),
                std::move(highPriorityFiles),
                std::move(lowPriorityFiles),
                bandwidthPriority,
                start,
                deleteFileMode,
                std::move(labels)
            ));
        }
    }

    Coroutine<> Rpc::addTorrentFileImpl(
        QString filePath,
        QString downloadDirectory,
        std::vector<int> unwantedFiles,
        std::vector<int> highPriorityFiles,
        std::vector<int> lowPriorityFiles,
        TorrentData::Priority bandwidthPriority,
        bool start,
        DeleteFileMode deleteFileMode,
        std::vector<QString> labels
    ) {
        struct LoadedTorrent {
            QByteArray data{};
            QString hash{};
        };

        auto loaded = co_await runOnThreadPool([filePath]() -> std::optional<LoadedTorrent> {
            try {
                auto data = readFile(filePath);
                auto metainfo = parseTorrentFile(filePath);
                return LoadedTorrent{.data = std::move(data), .hash = std::move(metainfo.infoHashV1)};
            } catch (const std::exception& e) {
                warning().logWithException(e, "addTorrentFile: failed to read or parse torrent file");
                return std::nullopt;
            }
        });
        if (!loaded.has_value()) {
            emit torrentAddError(filePath);
            co_return;
        }
        if (torrentByHash(loaded->hash)) {
            emit torrentAddDuplicate();
            co_return;
        }

        auto response = co_await call({"load.raw"_L1, {QString{}, loaded->data}});
        if (!response.success) {
            emit torrentAddError(filePath);
            co_return;
        }

        auto check = co_await call({"d.hash"_L1, {loaded->hash}});
        if (!check.success) {
            emit torrentAddError(filePath);
            co_return;
        }

        co_await applyAddedTorrentSettings(
            loaded->hash,
            std::move(downloadDirectory),
            unwantedFiles,
            highPriorityFiles,
            lowPriorityFiles,
            bandwidthPriority,
            labels
        );

        if (start) {
            co_await call({"d.start"_L1, {loaded->hash}});
        }

        if (deleteFileMode != DeleteFileMode::No) {
            mDeletingFilesCoroutineScope.launch(
                deleteTorrentFile(filePath, deleteFileMode == DeleteFileMode::MoveToTrash)
            );
        }
        mBackgroundRequestsCoroutineScope.launch(updateData());
    }

    void Rpc::addTorrentLinks(
        QStringList links,
        QString downloadDirectory,
        TorrentData::Priority bandwidthPriority,
        bool start,
        std::vector<QString> labels
    ) {
        if (isConnected()) {
            mBackgroundRequestsCoroutineScope.launch(addTorrentLinksImpl(
                std::move(links),
                std::move(downloadDirectory),
                bandwidthPriority,
                start,
                std::move(labels)
            ));
        }
    }

    Coroutine<> Rpc::addTorrentLinksImpl(
        QStringList links,
        QString downloadDirectory,
        TorrentData::Priority bandwidthPriority,
        bool start,
        std::vector<QString> labels
    ) {
        const auto method = QString(start ? "load.start"_L1 : "load.normal"_L1);
        const auto priority = TorrentData::priorityToInt(bandwidthPriority);

        std::vector<Coroutine<>> requests{};
        requests.reserve(static_cast<size_t>(links.size()));
        for (const auto& link : links) {
            QVariantList parameters{QString{}, link};
            if (!downloadDirectory.isEmpty()) {
                parameters.append("d.directory.set="_L1 + quotedRtorrentValue(downloadDirectory));
            }
            if (!labels.empty()) {
                parameters.append("d.custom1.set="_L1 + quotedRtorrentValue(labels.front()));
            }
            parameters.append("d.priority.set="_L1 + QString::number(priority));
            requests.push_back([this, method, parameters = std::move(parameters), link]() -> Coroutine<> {
                auto response = co_await call({method, parameters});
                if (!response.success) {
                    emit torrentAddError(link);
                }
            }());
        }
        co_await waitAll(std::move(requests));
        mBackgroundRequestsCoroutineScope.launch(updateData());
    }

    Coroutine<> Rpc::applyAddedTorrentSettings(
        const QString& hash,
        QString downloadDirectory,
        std::span<const int> unwantedFiles,
        std::span<const int> highPriorityFiles,
        std::span<const int> lowPriorityFiles,
        TorrentData::Priority bandwidthPriority,
        std::span<const QString> labels
    ) {
        std::vector<Command> commands{};
        if (!downloadDirectory.isEmpty()) {
            commands.push_back({"d.directory.set"_L1, {hash, downloadDirectory}});
        }
        if (!labels.empty()) {
            commands.push_back({"d.custom1.set"_L1, {hash, labels.front()}});
        }
        commands.push_back({"d.priority.set"_L1, {hash, TorrentData::priorityToInt(bandwidthPriority)}});

        const auto appendFileCommands = [&](std::span<const int> fileIds, int priority) {
            for (const int fileId : fileIds) {
                commands.push_back({"f.priority.set"_L1, {hash + ":f"_L1 + QString::number(fileId), priority}});
            }
        };
        appendFileCommands(unwantedFiles, 0);
        appendFileCommands(highPriorityFiles, 2);
        appendFileCommands(lowPriorityFiles, 1);

        const auto response = co_await callBatch(std::move(commands));
        if (!response.success) {
            warning().log("Failed to apply settings to added torrent {}: {}", hash, response.errorMessage);
        }
    }

    void Rpc::startTorrents(std::span<const int> ids) {
        mBackgroundRequestsCoroutineScope.launch(callBatch(commandsForTorrents(ids, "d.start"_L1), true));
    }

    void Rpc::startTorrentsNow(std::span<const int> ids) {
        mBackgroundRequestsCoroutineScope.launch(callBatch(commandsForTorrents(ids, "d.start"_L1), true));
    }

    void Rpc::pauseTorrents(std::span<const int> ids) {
        mBackgroundRequestsCoroutineScope.launch(callBatch(commandsForTorrents(ids, "d.stop"_L1), true));
    }

    void Rpc::removeTorrents(std::span<const int> ids, bool deleteFiles) {
        std::vector<QString> hashes{};
        hashes.reserve(ids.size());
        for (const int id : ids) {
            if (const auto torrent = torrentById(id)) {
                hashes.push_back(torrent->data().hashString);
            }
        }
        if (deleteFiles) {
            mBackgroundRequestsCoroutineScope.launch(removeTorrentsImpl(std::move(hashes), true));
        } else {
            std::vector<Command> commands{};
            commands.reserve(hashes.size());
            for (auto& hash : hashes) {
                commands.push_back({"d.erase"_L1, {std::move(hash)}});
            }
            mBackgroundRequestsCoroutineScope.launch(callBatch(std::move(commands), true));
        }
    }

    Coroutine<> Rpc::removeTorrentsImpl(std::vector<QString> hashes, bool deleteFiles) {
        std::vector<QString> pathsToDelete{};
        if (deleteFiles) {
            for (const auto& hash : hashes) {
                auto basePath = co_await call({"d.base_path.realpath.or_empty"_L1, {hash}});
                auto directory = co_await call({"d.directory.realpath.or_empty"_L1, {hash}});
                if (!basePath.success || !directory.success) {
                    continue;
                }
                const auto path = basePath.value.toString();
                const auto directoryPath = directory.value.toString();
                if (isSafeToDeletePath(path, directoryPath)) {
                    pathsToDelete.push_back(path);
                } else {
                    warning().log(
                        "Refusing to delete '{}': path is not a safe child of torrent directory '{}'",
                        path,
                        directoryPath
                    );
                }
            }
        }

        std::vector<Command> eraseCommands{};
        eraseCommands.reserve(hashes.size());
        for (auto& hash : hashes) {
            eraseCommands.push_back({"d.erase"_L1, {std::move(hash)}});
        }
        co_await callBatch(std::move(eraseCommands));

        for (const auto& path : pathsToDelete) {
            const auto response = co_await call({"execute.throw"_L1, {QString{}, "rm"_L1, "-rf"_L1, "--"_L1, path}});
            if (!response.success) {
                warning().log("Failed to delete '{}': {}", path, response.errorMessage);
            }
        }

        mBackgroundRequestsCoroutineScope.launch(updateData());
    }

    void Rpc::checkTorrents(std::span<const int> ids) {
        mBackgroundRequestsCoroutineScope.launch(callBatch(commandsForTorrents(ids, "d.check_hash"_L1), true));
    }

    void Rpc::reannounceTorrents(std::span<const int> ids) {
        mBackgroundRequestsCoroutineScope.launch(
            callBatch(commandsForTorrents(ids, "d.tracker_announce.force"_L1), true)
        );
    }

    void Rpc::setSessionProperty(QString command, QVariant value) {
        setSessionProperties({{std::move(command), std::move(value)}});
    }

    void Rpc::setSessionProperties(QVariantMap properties) {
        std::vector<Command> commands{};
        commands.reserve(static_cast<size_t>(properties.size()));
        for (auto [command, value] : properties.asKeyValueRange()) {
            if (value.typeId() == QMetaType::QVariantList) {
                QVariantList parameters{QString{}};
                for (const auto& item : value.toList()) {
                    parameters.append(item);
                }
                commands.push_back({command, std::move(parameters)});
            } else {
                commands.push_back({command, {QString{}, std::move(value)}});
            }
        }
        mBackgroundRequestsCoroutineScope.launch(callBatch(std::move(commands)));
    }

    void Rpc::setTorrentsLabels(std::span<const int> ids, std::span<const QString> labels) {
        const auto label = labels.empty() ? QString{} : labels.front();
        std::vector<Command> commands{};
        commands.reserve(ids.size());
        for (const int id : ids) {
            if (auto target = targetForTorrent(id); !target.isEmpty()) {
                commands.push_back({"d.custom1.set"_L1, {std::move(target), label}});
            }
        }
        mBackgroundRequestsCoroutineScope.launch(callBatch(std::move(commands), true));
    }

    void Rpc::addTorrentTracker(int id, const QString& announce, int group) {
        if (auto target = targetForTorrent(id); !target.isEmpty()) {
            mBackgroundRequestsCoroutineScope.launch(
                call({"d.tracker.insert"_L1, {std::move(target), group, announce}}, true)
            );
        }
    }

    void Rpc::setTorrentPriority(int id, TorrentData::Priority priority) {
        if (auto target = targetForTorrent(id); !target.isEmpty()) {
            mBackgroundRequestsCoroutineScope.launch(
                call({"d.priority.set"_L1, {std::move(target), TorrentData::priorityToInt(priority)}}, true)
            );
        }
    }

    void Rpc::setTorrentPeersLimit(int id, int limit) {
        if (auto target = targetForTorrent(id); !target.isEmpty()) {
            mBackgroundRequestsCoroutineScope.launch(call({"d.peers_max.set"_L1, {std::move(target), limit}}, true));
        }
    }

    void Rpc::setTorrentFilesWanted(int id, std::span<const int> fileIds, bool wanted) {
        const auto hash = targetForTorrent(id);
        if (hash.isEmpty()) {
            return;
        }
        std::vector<Command> commands{};
        commands.reserve(fileIds.size());
        for (const int fileId : fileIds) {
            commands.push_back({"f.priority.set"_L1, {hash + ":f"_L1 + QString::number(fileId), wanted ? 1 : 0}});
        }
        mBackgroundRequestsCoroutineScope.launch(callBatch(std::move(commands), true));
    }

    void Rpc::setTorrentFilesPriority(int id, std::span<const int> fileIds, TorrentFile::Priority priority) {
        const auto hash = targetForTorrent(id);
        if (hash.isEmpty()) {
            return;
        }
        int value = 1;
        switch (priority) {
        case TorrentFile::Priority::Low:
        case TorrentFile::Priority::Normal:
            value = 1;
            break;
        case TorrentFile::Priority::High:
            value = 2;
            break;
        }

        std::vector<Command> commands{};
        commands.reserve(fileIds.size());
        for (const int fileId : fileIds) {
            commands.push_back({"f.priority.set"_L1, {hash + ":f"_L1 + QString::number(fileId), value}});
        }
        mBackgroundRequestsCoroutineScope.launch(callBatch(std::move(commands), true));
    }

    void Rpc::getTorrentFiles(int torrentId) {
        const auto torrent = torrentById(torrentId);
        if (isConnected() && torrent) {
            mBackgroundRequestsCoroutineScope.launch(getTorrentsFiles({torrent->data().hashString}));
        }
    }

    Coroutine<> Rpc::getTorrentsFiles(std::vector<QString> hashes) {
        const QStringList fields{
            "f.path="_L1,
            "f.size_bytes="_L1,
            "f.completed_chunks="_L1,
            "f.size_chunks="_L1,
            "f.priority="_L1,
            "f.is_created="_L1
        };
        std::vector<Coroutine<>> requests{};
        requests.reserve(hashes.size());
        for (const auto& hash : hashes) {
            requests.push_back([this, hash, fields]() -> Coroutine<> {
                QVariantList parameters{hash, QString{}};
                for (const auto& field : fields) {
                    parameters.append(field);
                }
                const auto response = co_await call({"f.multicall"_L1, std::move(parameters)});
                if (!response.success) {
                    co_return;
                }
                if (auto torrent = torrentByHash(hash); torrent && torrent->isFilesEnabled()) {
                    torrent->updateFiles(response.value.toList());
                }
            }());
        }
        co_await waitAll(std::move(requests));
    }

    void Rpc::getTorrentPeers(int torrentId) {
        const auto torrent = torrentById(torrentId);
        if (isConnected() && torrent) {
            mBackgroundRequestsCoroutineScope.launch(getTorrentsPeers({torrent->data().hashString}));
        }
    }

    Coroutine<> Rpc::getTorrentsPeers(std::vector<QString> hashes) {
        const QStringList fields{
            "p.address="_L1,
            "p.client_version="_L1,
            "p.down_rate="_L1,
            "p.up_rate="_L1,
            "p.completed_percent="_L1,
            "p.is_encrypted="_L1,
            "p.is_incoming="_L1,
            "p.is_snubbed="_L1,
            "p.is_preferred="_L1,
            "p.is_unwanted="_L1
        };
        std::vector<Coroutine<>> requests{};
        requests.reserve(hashes.size());
        for (const auto& hash : hashes) {
            requests.push_back([this, hash, fields]() -> Coroutine<> {
                QVariantList parameters{hash, QString{}};
                for (const auto& field : fields) {
                    parameters.append(field);
                }
                const auto response = co_await call({"p.multicall"_L1, std::move(parameters)});
                if (!response.success) {
                    co_return;
                }
                if (auto torrent = torrentByHash(hash); torrent && torrent->isPeersEnabled()) {
                    torrent->updatePeers(response.value.toList());
                }
            }());
        }
        co_await waitAll(std::move(requests));
    }

    void Rpc::shutdownServer() {
        if (isConnected()) {
            mBackgroundRequestsCoroutineScope.launch(shutdownServerImpl());
        }
    }

    Coroutine<> Rpc::shutdownServerImpl() {
        const auto response = co_await call({"system.shutdown"_L1, {QString{}}});
        if (response.success) {
            info().log("Successfully sent shutdown request, disconnecting");
            disconnect();
        }
    }

    void Rpc::setStatus(Status&& status) {
        if (status == mStatus) {
            return;
        }

        const Status oldStatus = mStatus;

        const bool connectionStateChanged = status.connectionState != oldStatus.connectionState;
        if (connectionStateChanged && oldStatus.connectionState == ConnectionState::Connected) {
            emit aboutToDisconnect();
        }

        mStatus = std::move(status);

        if (connectionStateChanged) {
            resetStateOnConnectionStateChanged(oldStatus.connectionState);
        }

        emit statusChanged();

        if (connectionStateChanged) {
            emitSignalsOnConnectionStateChanged(oldStatus.connectionState);
        }
    }

    void Rpc::resetStateOnConnectionStateChanged(ConnectionState oldConnectionState) {
        switch (mStatus.connectionState) {
        case ConnectionState::Disconnected: {
            info().log("Disconnected");

            mBackgroundRequestsCoroutineScope.cancelAll();
            mServerIsLocal = std::nullopt;

            if (!mTorrents.empty() && oldConnectionState == ConnectionState::Connected) {
                const auto removedTorrentsCount = mTorrents.size();
                emit onAboutToRemoveTorrents(0, removedTorrentsCount);
                mTorrents.clear();
                emit onRemovedTorrents(0, removedTorrentsCount);
            }

            if (error() != RpcError::NoError && mAutoReconnectEnabled) {
                mAutoReconnectCoroutineScope.launch(autoReconnect());
            }

            break;
        }
        case ConnectionState::Connecting:
            info().log("Connecting");
            mAutoReconnectCoroutineScope.cancelAll();
            break;
        case ConnectionState::Connected: {
            info().log("Connected");
            break;
        }
        }
    }

    void Rpc::emitSignalsOnConnectionStateChanged(Rpc::ConnectionState oldConnectionState) {
        switch (mStatus.connectionState) {
        case ConnectionState::Disconnected: {
            emit connectionStateChanged();
            if (oldConnectionState == ConnectionState::Connected) {
                emit connectedChanged();
                emit torrentsUpdated();
            }
            break;
        }
        case ConnectionState::Connecting:
            emit connectionStateChanged();
            break;
        case ConnectionState::Connected: {
            emit torrentsUpdated();
            emit connectionStateChanged();
            emit connectedChanged();
            break;
        }
        }
    }

    Coroutine<> Rpc::getServerSettings() {
        std::vector<Command> commands{};
        for (const auto& command : ServerSettings::requestCommands()) {
            commands.push_back({command, {QString{}}});
        }
        const auto response = co_await callBatch(std::move(commands));
        if (response.success) {
            mServerSettings->update(response.value.toList());
        }
    }

    struct NewTorrent {
        int id{};
        QString hash{};
        QVariantList values{};
    };

    class TorrentsListUpdater final : public ItemListUpdater<std::unique_ptr<Torrent>, std::vector<NewTorrent>> {
    public:
        inline explicit TorrentsListUpdater(Rpc& rpc) : mRpc(rpc) {}

        std::vector<std::pair<int, int>> removedIndexRanges{};
        std::vector<std::pair<int, int>> changedIndexRanges{};
        int addedCount{};

    protected:
        std::vector<NewTorrent>::iterator
        findNewItemForItem(std::vector<NewTorrent>& newTorrents, const std::unique_ptr<Torrent>& torrent) override {
            return std::ranges::find(newTorrents, torrent->data().hashString, &NewTorrent::hash);
        }

        void onAboutToRemoveItems(size_t first, size_t last) override {
            emit mRpc.onAboutToRemoveTorrents(first, last);
        };

        void onRemovedItems(size_t first, size_t last) override {
            removedIndexRanges.emplace_back(static_cast<int>(first), static_cast<int>(last));
            emit mRpc.onRemovedTorrents(first, last);
        }

        // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
        bool updateItem(std::unique_ptr<Torrent>& torrent, NewTorrent&& newTorrent) override {
            const bool wasFinished = torrent->data().isFinished();
            const bool wasPaused = (torrent->data().status == TorrentData::Status::Paused);
            const auto oldSizeWhenDone = torrent->data().sizeWhenDone;

            const bool changed = torrent->update(newTorrent.values);
            if (changed
                && !wasFinished
                && torrent->data().isFinished()
                && !wasPaused
                && torrent->data().sizeWhenDone >= oldSizeWhenDone) {
                emit mRpc.torrentFinished(torrent.get());
            }

            return changed;
        }

        void onChangedItems(size_t first, size_t last) override {
            changedIndexRanges.emplace_back(static_cast<int>(first), static_cast<int>(last));
            emit mRpc.onChangedTorrents(first, last);
        }

        // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
        std::unique_ptr<Torrent> createItemFromNewItem(NewTorrent&& newTorrent) override {
            auto torrent = std::make_unique<Torrent>(newTorrent.id, newTorrent.hash, newTorrent.values, &mRpc);
            if (mRpc.isConnected()) {
                emit mRpc.torrentAdded(torrent.get());
            }
            return torrent;
        }

        void onAboutToAddItems(size_t count) override { emit mRpc.onAboutToAddTorrents(count); }

        void onAddedItems(size_t count) override {
            addedCount = static_cast<int>(count);
            emit mRpc.onAddedTorrents(count);
        };

    private:
        Rpc& mRpc;
    };

    Coroutine<> Rpc::getTorrents() {
        QVariantList parameters{QString{}, torrentsView};
        for (const auto& field : Torrent::updateFields()) {
            parameters.append(field);
        }
        const auto response = co_await call({"d.multicall2"_L1, std::move(parameters)});
        if (!response.success) {
            co_return;
        }

        TorrentsListUpdater updater(*this);
        {
            const auto rows = response.value.toList();
            std::vector<NewTorrent> newTorrents{};
            newTorrents.reserve(static_cast<size_t>(rows.size()));
            for (const auto& rowValue : rows) {
                const auto row = rowValue.toList();
                if (row.isEmpty()) {
                    continue;
                }
                const auto hash = row.constFirst().toString().toLower();
                if (hash.isEmpty()) {
                    continue;
                }
                int id{};
                if (const auto existing = torrentByHash(hash)) {
                    id = existing->data().id;
                } else {
                    id = mNextTorrentId++;
                }
                newTorrents.push_back({.id = id, .hash = hash, .values = row});
            }
            updater.update(mTorrents, std::move(newTorrents));
        }

        std::vector<QString> filesHashes{};
        std::vector<QString> peersHashes{};
        for (const auto& torrent : mTorrents) {
            if (torrent->isFilesEnabled()) {
                filesHashes.push_back(torrent->data().hashString);
            }
            if (torrent->isPeersEnabled()) {
                peersHashes.push_back(torrent->data().hashString);
            }
        }
        std::vector<Coroutine<>> additionalRequests{};
        if (!filesHashes.empty()) {
            additionalRequests.push_back(getTorrentsFiles(std::move(filesHashes)));
        }
        if (!peersHashes.empty()) {
            additionalRequests.push_back(getTorrentsPeers(std::move(peersHashes)));
        }
        if (!additionalRequests.empty()) {
            co_await waitAll(std::move(additionalRequests));
        }

        const bool wasConnecting = connectionState() == ConnectionState::Connecting;
        if (!wasConnecting) {
            emit torrentsUpdated();
        }
    }

    Coroutine<> Rpc::getServerStats() {
        std::vector<Command> commands{};
        for (const auto& command : ServerStats::requestCommands()) {
            commands.push_back({command, {QString{}}});
        }
        const auto response = co_await callBatch(std::move(commands));
        if (response.success) {
            mServerStats->update(response.value.toList());
        }
    }

    Coroutine<> Rpc::connectAndPerformDataUpdates() {
        co_await getServerSettings();
        if (connectionState() == ConnectionState::Disconnected && error() != Error::NoError) {
            co_return;
        }
        if (mServerSettings->data().apiVersion == 0) {
            setStatus(
                Status{
                    .connectionState = ConnectionState::Disconnected,
                    .error = Error::ConnectionError,
                    .errorMessage = qApp->translate("tremotesf", "Failed to read rtorrent settings")
                }
            );
            co_return;
        }
        co_await waitAll(checkIfServerIsLocal(), getTorrents(), getServerStats());
        if (connectionState() == ConnectionState::Disconnected && error() != Error::NoError) {
            co_return;
        }
        setStatus(Status{.connectionState = ConnectionState::Connected});
        while (true) {
            co_await waitFor(mUpdateInterval);
            co_await updateData();
        }
    }

    Coroutine<> Rpc::updateData() {
        debug().log("Updating data");
        co_await waitAll(getServerSettings(), getTorrents(), getServerStats());
        debug().log("Finished updating data");
    }

    Coroutine<> Rpc::checkIfServerIsLocal() {
        info().log("checkIfServerIsLocal() called");
        const auto configuration = mRequestRouter->configuration();
        if (!configuration.has_value()) {
            co_return;
        }
        if (!configuration->socketPath.isEmpty()) {
            mServerIsLocal = true;
            info().log("checkIfServerIsLocal: server is running locally: true");
            co_return;
        }
        const auto host = configuration->address;
        if (auto localIp = isLocalIpAddress(host); localIp.has_value()) {
            mServerIsLocal = localIp;
            info().log("checkIfServerIsLocal: server is running locally: {}", *mServerIsLocal);
            co_return;
        }
        info().log("checkIfServerIsLocal: resolving IP address for host name {}", host);
        const QHostInfo hostInfo = co_await lookupHost(host);
        info().log("checkIfServerIsLocal: resolved IP address for host name {}", host);
        const auto addresses = hostInfo.addresses();
        if (!addresses.isEmpty()) {
            info().log("checkIfServerIsLocal: IP addresses:");
            for (const auto& address : addresses) {
                info().log("checkIfServerIsLocal: - {}", address);
            }
            info().log("checkIfServerIsLocal: checking first address");
            mServerIsLocal = isLocalIpAddress(addresses.first());
        } else {
            mServerIsLocal = false;
        }
        info().log("checkIfServerIsLocal: server is running locally: {}", *mServerIsLocal);
    }

    void Rpc::onRequestFailed(RpcError error, const QString& errorMessage, const QString& detailedErrorMessage) {
        setStatus(
            {.connectionState = RpcConnectionState::Disconnected,
             .error = error,
             .errorMessage = errorMessage,
             .detailedErrorMessage = detailedErrorMessage}
        );
    }

    Coroutine<> Rpc::autoReconnect() {
        info().log("Auto reconnecting in {} seconds", mAutoReconnectInterval.count());
        co_await waitFor(mAutoReconnectInterval);
        info().log("Auto reconnection");
        connect();
    }
}
