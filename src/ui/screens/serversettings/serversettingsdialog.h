// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_SERVERSETTINGSDIALOG_H
#define TREMOTESF_SERVERSETTINGSDIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QSpinBox;

class KMessageWidget;

namespace tremotesf {
    class RemoteDirectorySelectionWidget;
    class Rpc;

    class ServerSettingsDialog final : public QDialog {
        Q_OBJECT

    public:
        explicit ServerSettingsDialog(const Rpc* rpc, QWidget* parent = nullptr);
        void accept() override;

    private:
        void setupUi();
        void loadSettings();

        const Rpc* mRpc;

        KMessageWidget* mDisconnectedMessageWidget = nullptr;

        QWidget* mDownloadingPageWidget = nullptr;
        RemoteDirectorySelectionWidget* mDownloadDirectoryWidget = nullptr;

        QWidget* mSpeedPageWidget = nullptr;
        QCheckBox* mDownloadSpeedLimitCheckBox = nullptr;
        QSpinBox* mDownloadSpeedLimitSpinBox = nullptr;
        QCheckBox* mUploadSpeedLimitCheckBox = nullptr;
        QSpinBox* mUploadSpeedLimitSpinBox = nullptr;

        QWidget* mNetworkPageWidget = nullptr;
        QSpinBox* mPeerPortSpinBox = nullptr;
        QCheckBox* mRandomPortCheckBox = nullptr;
        QComboBox* mEncryptionComboBox = nullptr;
        QCheckBox* mPexCheckBox = nullptr;
        QCheckBox* mDhtCheckBox = nullptr;
        QSpinBox* mTorrentPeerLimitSpinBox = nullptr;
    };
}

#endif // TREMOTESF_SERVERSETTINGSDIALOG_H
