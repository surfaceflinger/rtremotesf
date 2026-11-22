// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "serversettingsdialog.h"

#include <array>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QIcon>
#include <QSpinBox>
#include <QVBoxLayout>

#include <KColumnResizer>
#include <KMessageWidget>
#include <KPageWidget>

#include "rpc/numbervaluerange.h"
#include "rpc/rpc.h"
#include "rpc/serversettings.h"
#include "stdutils.h"
#include "ui/widgets/remotedirectoryselectionwidget.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        constexpr std::array encryptionModeComboBoxItems{
            ServerSettingsData::EncryptionMode::Allowed,
            ServerSettingsData::EncryptionMode::Preferred,
            ServerSettingsData::EncryptionMode::Required
        };

        ServerSettingsData::EncryptionMode encryptionModeFromComboBoxItem(int index) {
            if (index == -1) {
                return {};
            }
            return encryptionModeComboBoxItems.at(static_cast<size_t>(index));
        }
    }

    ServerSettingsDialog::ServerSettingsDialog(const Rpc* rpc, QWidget* parent) : QDialog(parent), mRpc(rpc) {
        //: Dialog title
        setWindowTitle(qApp->translate("tremotesf", "Server Options"));

        setupUi();
        resize(sizeHint().expandedTo(QSize(700, 450)));

        const auto onConnectedChanged = [this] {
            if (mRpc->isConnected()) {
                mDisconnectedMessageWidget->animatedHide();
                mDownloadingPageWidget->setEnabled(true);
                mSpeedPageWidget->setEnabled(true);
                mNetworkPageWidget->setEnabled(true);
                loadSettings();
            } else {
                mDisconnectedMessageWidget->hide();
                mDisconnectedMessageWidget->animatedShow();
                mDownloadingPageWidget->setEnabled(false);
                mSpeedPageWidget->setEnabled(false);
                mNetworkPageWidget->setEnabled(false);
            }
        };
        QObject::connect(mRpc, &Rpc::connectedChanged, this, onConnectedChanged);
        onConnectedChanged();

        loadSettings();
    }

    void ServerSettingsDialog::accept() {
        ServerSettings* settings = mRpc->serverSettings();

        settings->setSaveOnSet(false);

        settings->setDownloadDirectory(mDownloadDirectoryWidget->path());
        settings->setDownloadSpeedLimited(mDownloadSpeedLimitCheckBox->isChecked());
        settings->setDownloadSpeedLimit(mDownloadSpeedLimitSpinBox->value());
        settings->setUploadSpeedLimited(mUploadSpeedLimitCheckBox->isChecked());
        settings->setUploadSpeedLimit(mUploadSpeedLimitSpinBox->value());
        settings->setPeerPort(mPeerPortSpinBox->value());
        settings->setRandomPortEnabled(mRandomPortCheckBox->isChecked());
        settings->setEncryptionMode(encryptionModeFromComboBoxItem(mEncryptionComboBox->currentIndex()));
        settings->setPexEnabled(mPexCheckBox->isChecked());
        settings->setDhtEnabled(mDhtCheckBox->isChecked());
        settings->setMaximumPeersPerTorrent(mTorrentPeerLimitSpinBox->value());

        settings->save();
        settings->setSaveOnSet(true);

        QDialog::accept();
    }

    void ServerSettingsDialog::setupUi() {
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        //: Message that appears when disconnected from server
        mDisconnectedMessageWidget = new KMessageWidget(qApp->translate("tremotesf", "Disconnected"), this);
        mDisconnectedMessageWidget->setCloseButtonVisible(false);
        mDisconnectedMessageWidget->setMessageType(KMessageWidget::Warning);
        mDisconnectedMessageWidget->setVisible(false);
        layout->addWidget(mDisconnectedMessageWidget);

        auto pageWidget = new KPageWidget(this);

        mDownloadingPageWidget = new QWidget(this);
        KPageWidgetItem* downloadingPageItem = pageWidget->addPage(
            mDownloadingPageWidget,
            //: "Downloading" server setting page
            qApp->translate("tremotesf", "Downloading", "Noun")
        );
        downloadingPageItem->setIcon(QIcon::fromTheme("folder-download"_L1));

        auto downloadingPageLayout = new QFormLayout(mDownloadingPageWidget);
        downloadingPageLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        mDownloadDirectoryWidget = new RemoteDirectorySelectionWidget(this);
        mDownloadDirectoryWidget->setup({}, mRpc);
        downloadingPageLayout->addRow(qApp->translate("tremotesf", "Download directory:"), mDownloadDirectoryWidget);

        mSpeedPageWidget = new QWidget(this);
        KPageWidgetItem* speedPageItem = pageWidget->addPage(
            mSpeedPageWidget,
            //: "Speed" server settings page
            qApp->translate("tremotesf", "Speed")
        );
        speedPageItem->setIcon(QIcon::fromTheme("preferences-system-time"_L1));

        auto speedPageLayout = new QVBoxLayout(mSpeedPageWidget);

        //: Speed limits section
        auto speedLimitsGroupBox = new QGroupBox(qApp->translate("tremotesf", "Limits"), this);
        auto speedLimitsGroupBoxLayout = new QGridLayout(speedLimitsGroupBox);

        //: Suffix that is added to input field with download/upload speed limit, e.g. "5000 kB/s". 'k' prefix means SI prefix, i.e kB = 1000 bytes
        const QString suffix(qApp->translate("tremotesf", " kB/s"));

        //: Download speed limit input field label
        mDownloadSpeedLimitCheckBox = new QCheckBox(qApp->translate("tremotesf", "Download:"), this);
        speedLimitsGroupBoxLayout->addWidget(mDownloadSpeedLimitCheckBox, 0, 0, 1, 2);

        mDownloadSpeedLimitSpinBox = new QSpinBox(this);
        mDownloadSpeedLimitSpinBox->setEnabled(false);
        setSpinBoxLimits(mDownloadSpeedLimitSpinBox, speedLimitKbpsRange);
        mDownloadSpeedLimitSpinBox->setSuffix(suffix);
        QObject::connect(
            mDownloadSpeedLimitCheckBox,
            &QCheckBox::toggled,
            mDownloadSpeedLimitSpinBox,
            &QSpinBox::setEnabled
        );
        speedLimitsGroupBoxLayout->addWidget(mDownloadSpeedLimitSpinBox, 1, 1);

        //: Upload speed limit input field label
        mUploadSpeedLimitCheckBox = new QCheckBox(qApp->translate("tremotesf", "Upload:"), this);
        speedLimitsGroupBoxLayout->addWidget(mUploadSpeedLimitCheckBox, 2, 0, 1, 2);

        mUploadSpeedLimitSpinBox = new QSpinBox(this);
        mUploadSpeedLimitSpinBox->setEnabled(false);
        setSpinBoxLimits(mUploadSpeedLimitSpinBox, speedLimitKbpsRange);
        mUploadSpeedLimitSpinBox->setSuffix(suffix);
        QObject::connect(
            mUploadSpeedLimitCheckBox,
            &QCheckBox::toggled,
            mUploadSpeedLimitSpinBox,
            &QSpinBox::setEnabled
        );
        speedLimitsGroupBoxLayout->addWidget(mUploadSpeedLimitSpinBox, 3, 1);

        speedLimitsGroupBoxLayout->setColumnMinimumWidth(0, 28 - speedLimitsGroupBoxLayout->spacing());
        speedPageLayout->addWidget(speedLimitsGroupBox);
        speedPageLayout->addStretch();

        mNetworkPageWidget = new QWidget(this);
        KPageWidgetItem* networkPageItem = pageWidget->addPage(
            mNetworkPageWidget,
            //: "Network" server settings page
            qApp->translate("tremotesf", "Network")
        );
        networkPageItem->setIcon(QIcon::fromTheme("preferences-system-network"_L1));

        auto networkPageLayout = new QVBoxLayout(mNetworkPageWidget);

        //: Title of settings section related to peer connections
        auto connectionGroupBox = new QGroupBox(qApp->translate("tremotesf", "Connection"), this);
        auto connectionGroupBoxLayout = new QFormLayout(connectionGroupBox);
        connectionGroupBoxLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        mPeerPortSpinBox = new QSpinBox(this);
        setSpinBoxLimits(mPeerPortSpinBox, portsRange);
        connectionGroupBoxLayout->addRow(qApp->translate("tremotesf", "Peer port:"), mPeerPortSpinBox);

        //: Check box label
        mRandomPortCheckBox = new QCheckBox(qApp->translate("tremotesf", "Random port on rtorrent start"), this);
        connectionGroupBoxLayout->addRow(mRandomPortCheckBox);

        mEncryptionComboBox = new QComboBox();
        for (const auto mode : encryptionModeComboBoxItems) {
            switch (mode) {
            case ServerSettingsData::EncryptionMode::Allowed:
                //: Encryption mode (allow/prefer/require)
                mEncryptionComboBox->addItem(qApp->translate("tremotesf", "Allow"));
                break;
            case ServerSettingsData::EncryptionMode::Preferred:
                //: Encryption mode (allow/prefer/require)
                mEncryptionComboBox->addItem(qApp->translate("tremotesf", "Prefer"));
                break;
            case ServerSettingsData::EncryptionMode::Required:
                //: Encryption mode (allow/prefer/require)
                mEncryptionComboBox->addItem(qApp->translate("tremotesf", "Require"));
                break;
            }
        }
        connectionGroupBoxLayout->addRow(qApp->translate("tremotesf", "Encryption:"), mEncryptionComboBox);

        //: Check box label
        mPexCheckBox = new QCheckBox(qApp->translate("tremotesf", "Enable PEX (Peer exchange)"), this);
        connectionGroupBoxLayout->addRow(mPexCheckBox);

        //: Check box label
        mDhtCheckBox = new QCheckBox(qApp->translate("tremotesf", "Enable DHT"), this);
        connectionGroupBoxLayout->addRow(mDhtCheckBox);

        networkPageLayout->addWidget(connectionGroupBox);

        auto peerLimitsGroupBox = new QGroupBox(qApp->translate("tremotesf", "Peer Limits"), this);
        auto peerLimitsGroupBoxLayout = new QFormLayout(peerLimitsGroupBox);
        peerLimitsGroupBoxLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        mTorrentPeerLimitSpinBox = new QSpinBox(this);
        setSpinBoxLimits(mTorrentPeerLimitSpinBox, peersLimitRange);
        peerLimitsGroupBoxLayout->addRow(
            qApp->translate("tremotesf", "Maximum peers per torrent:"),
            mTorrentPeerLimitSpinBox
        );

        networkPageLayout->addWidget(peerLimitsGroupBox);
        networkPageLayout->addStretch();

        auto networkPageResizer = new KColumnResizer(this);
        networkPageResizer->addWidgetsFromLayout(connectionGroupBoxLayout);
        networkPageResizer->addWidgetsFromLayout(peerLimitsGroupBoxLayout);

        layout->addWidget(pageWidget);

        auto dialogButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        QObject::connect(dialogButtonBox, &QDialogButtonBox::accepted, this, &ServerSettingsDialog::accept);
        QObject::connect(dialogButtonBox, &QDialogButtonBox::rejected, this, &ServerSettingsDialog::reject);
        pageWidget->setPageFooter(dialogButtonBox);
    }

    void ServerSettingsDialog::loadSettings() {
        const ServerSettings* settings = mRpc->serverSettings();

        mDownloadDirectoryWidget->resetPath(settings->data().downloadDirectory);

        mDownloadSpeedLimitCheckBox->setChecked(settings->data().downloadSpeedLimited);
        mDownloadSpeedLimitSpinBox->setValue(settings->data().downloadSpeedLimit);
        mUploadSpeedLimitCheckBox->setChecked(settings->data().uploadSpeedLimited);
        mUploadSpeedLimitSpinBox->setValue(settings->data().uploadSpeedLimit);

        mPeerPortSpinBox->setValue(settings->data().peerPort);
        mRandomPortCheckBox->setChecked(settings->data().randomPortEnabled);
        mEncryptionComboBox->setCurrentIndex(
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
            indexOfCasted<int>(encryptionModeComboBoxItems, settings->data().encryptionMode).value()
        );
        mPexCheckBox->setChecked(settings->data().pexEnabled);
        mDhtCheckBox->setChecked(settings->data().dhtEnabled);
        mTorrentPeerLimitSpinBox->setValue(settings->data().maximumPeersPerTorrent);
    }
}
