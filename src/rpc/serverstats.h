// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_SERVERSTATS_H
#define TREMOTESF_RPC_SERVERSTATS_H

#include <QObject>
#include <QStringList>
#include <QVariantList>

namespace tremotesf {
    class ServerStats final : public QObject {
        Q_OBJECT

    public:
        using QObject::QObject;

        static QStringList requestCommands();

        [[nodiscard]] qint64 downloadSpeed() const { return mDownloadSpeed; };
        [[nodiscard]] qint64 uploadSpeed() const { return mUploadSpeed; };
        [[nodiscard]] qint64 downloaded() const { return mDownloaded; };
        [[nodiscard]] qint64 uploaded() const { return mUploaded; };

        void update(const QVariantList& values);

    private:
        qint64 mDownloadSpeed{};
        qint64 mUploadSpeed{};
        qint64 mDownloaded{};
        qint64 mUploaded{};
    signals:
        void updated();
    };
}

#endif // TREMOTESF_RPC_SERVERSTATS_H
