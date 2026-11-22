// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_REQUESTROUTER_H
#define TREMOTESF_RPC_REQUESTROUTER_H

#include <chrono>
#include <optional>

#include <QByteArray>
#include <QNetworkProxy>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>

#include "coroutines/coroutinefwd.h"

namespace tremotesf {
    enum class RpcError;
}

namespace tremotesf::impl {
    class RequestRouter final : public QObject {
        Q_OBJECT

    public:
        explicit RequestRouter(QObject* parent = nullptr);

        struct RequestsConfiguration {
            QString address{};
            quint16 port{};
            QString socketPath{};
            QNetworkProxy proxy{QNetworkProxy::applicationProxy()};
            std::chrono::milliseconds timeout{};
            int retryAttempts{2};
        };

        const std::optional<RequestsConfiguration>& configuration() const { return mConfiguration; }
        void setConfiguration(RequestsConfiguration configuration);
        void resetConfiguration();

        struct Response {
            QVariant value{};
            bool success{};
            QString errorMessage{};
        };

        Coroutine<Response> call(const QString& method, QVariantList parameters = {});

    private:
        Coroutine<Response> performRequest(const QByteArray& data, const QString& method);

        std::optional<RequestsConfiguration> mConfiguration{};

    signals:
        /**
         * Emitted if request has failed with network error
         * @brief requestFailed
         * @param error Error type
         * @param errorMessage Short error message
         * @param detailedErrorMessage Detailed error message
         */
        void requestFailed(tremotesf::RpcError error, const QString& errorMessage, const QString& detailedErrorMessage);
    };
}

#endif // TREMOTESF_RPC_REQUESTROUTER_H
