// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "torrentpropertieswidget.h"

#include <array>

#include <fmt/format.h>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QGroupBox>
#include <QLabel>
#include <QListView>
#include <QLocale>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <KMessageWidget>
#include <KColumnResizer>

#include "desktoputils.h"
#include "formatutils.h"
#include "peersmodel.h"
#include "settings.h"
#include "stdutils.h"
#include "torrentfilesmodel.h"
#include "trackersviewwidget.h"
#include "rpc/numbervaluerange.h"
#include "rpc/pathutils.h"
#include "rpc/rpc.h"
#include "rpc/serversettings.h"
#include "rpc/torrent.h"
#include "ui/itemmodels/baseproxymodel.h"
#include "ui/itemmodels/stringlistmodel.h"
#include "ui/stylehelpers.h"
#include "ui/widgets/basetreeview.h"
#include "ui/widgets/progressbardelegate.h"
#include "ui/widgets/tooltipwhenelideddelegate.h"
#include "ui/widgets/torrentfilesview.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        constexpr std::array priorityComboBoxItems{
            TorrentData::Priority::High, TorrentData::Priority::Normal, TorrentData::Priority::Low
        };
    }

    TorrentPropertiesWidget::TorrentPropertiesWidget(Rpc* rpc, bool horizontalDetails, QWidget* parent)
        : QTabWidget(parent),
          mRpc(rpc),
          mFilesModel(new TorrentFilesModel(mRpc, this)),
          mFilesView(new TorrentFilesView(mFilesModel, mRpc, this)),
          mTrackersViewWidget(new TrackersViewWidget(mRpc, this)) {
        setEnabled(false);

        setupDetailsTab(horizontalDetails);

        auto filesTab = new QWidget(this);
        auto filesTabLayout = new QVBoxLayout(filesTab);
        filesTabLayout->addWidget(mFilesView);
        overrideBreezeFramelessScrollAreaHeuristic(mFilesView, true);
        //: Torrent properties dialog tab
        addTab(filesTab, qApp->translate("tremotesf", "Files"));

        //: Torrent properties dialog tab
        addTab(mTrackersViewWidget, qApp->translate("tremotesf", "Trackers"));

        setupPeersTab();
        setupLimitsTab();
    }

    void TorrentPropertiesWidget::saveState() {
        Settings::instance()->set_peersViewHeaderState(mPeersView->header()->saveState());
        mFilesView->saveState();
        mTrackersViewWidget->saveState();
    }

    void TorrentPropertiesWidget::setupDetailsTab(bool horizontal) {
        auto detailsTab = new QScrollArea(this);
        detailsTab->setWidgetResizable(true);
        makeScrollAreaTransparent(detailsTab);

        if (!horizontal) {
            detailsTab->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }
        //: Torrent's properties dialog tab
        addTab(detailsTab, qApp->translate("tremotesf", "Details"));

        auto detailsTabScrollContent = new QWidget(detailsTab);
        detailsTab->setWidget(detailsTabScrollContent);

        QBoxLayout* detailsTabLayout{};
        if (horizontal) {
            detailsTabLayout = new QHBoxLayout(detailsTabScrollContent);
        } else {
            detailsTabLayout = new QVBoxLayout(detailsTabScrollContent);
        }

        //: Torrent's details tab section
        auto activityGroupBox = new QGroupBox(qApp->translate("tremotesf", "Activity"), this);
        auto activityGroupBoxLayout = new QFormLayout(activityGroupBox);
        activityGroupBoxLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        auto completedLabel = new QLabel(this);
        //: Torrent's completed size
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Completed:"), completedLabel);
        auto downloadedLabel = new QLabel(this);
        //: Torrent's downloaded size
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Downloaded:"), downloadedLabel);
        auto uploadedLabel = new QLabel(this);
        //: Torrent's uploaded size
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Uploaded:"), uploadedLabel);
        auto ratioLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Ratio:"), ratioLabel);
        auto downloadSpeedLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Download speed:"), downloadSpeedLabel);
        auto uploadSpeedLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Upload speed:"), uploadSpeedLabel);
        auto etaLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "ETA:"), etaLabel);
        auto seedersLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Seeders:"), seedersLabel);
        auto leechersLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Leechers:"), leechersLabel);
        auto peersSendingToUsLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(
            qApp->translate("tremotesf", "Peers we are downloading from:"),
            peersSendingToUsLabel
        );
        auto peersGettingFromUsLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(
            qApp->translate("tremotesf", "Peers we are uploading to:"),
            peersGettingFromUsLabel
        );
        auto lastActivityLabel = new QLabel(this);
        activityGroupBoxLayout->addRow(qApp->translate("tremotesf", "Last activity:"), lastActivityLabel);
        detailsTabLayout->addWidget(activityGroupBox);

        //: Torrent's details tab section
        auto infoGroupBox = new QGroupBox(qApp->translate("tremotesf", "Information"), this);
        auto infoGroupBoxLayout = new QFormLayout(infoGroupBox);
        infoGroupBoxLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        auto totalSizeLabel = new QLabel(this);
        infoGroupBoxLayout->addRow(qApp->translate("tremotesf", "Total size:"), totalSizeLabel);
        auto locationLabel = new QLabel(this);
        locationLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        //: Torrent's download directory
        infoGroupBoxLayout->addRow(qApp->translate("tremotesf", "Location:"), locationLabel);
        auto hashLabel = new QLabel(this);
        hashLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        //: Torrent's hash string
        infoGroupBoxLayout->addRow(qApp->translate("tremotesf", "Hash:"), hashLabel);
        auto creationDateLabel = new QLabel(this);
        //: Date/time when torrent was created
        infoGroupBoxLayout->addRow(qApp->translate("tremotesf", "Created on:"), creationDateLabel);

        auto labelsModel = new StringListModel({}, QIcon::fromTheme("tag"_L1), this);
        auto labelsProxyModel = new BaseProxyModel(labelsModel, Qt::DisplayRole, std::nullopt, this);
        auto labelsView = new QListView(this);
        labelsView->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
        labelsView->setFlow(QListView::LeftToRight);
        labelsView->setWrapping(true);
        labelsView->setResizeMode(QListView::Adjust);
        labelsView->setIconSize(QSize(16, 16));
        labelsView->setModel(labelsProxyModel);
        //: Torrent's labels
        infoGroupBoxLayout->addRow(qApp->translate("tremotesf", "Labels:"), labelsView);
        auto labelsLabel = infoGroupBoxLayout->labelForField(labelsView);

        detailsTabLayout->addWidget(infoGroupBox);

        if (!horizontal) {
            auto resizer = new KColumnResizer(this);
            resizer->addWidgetsFromLayout(activityGroupBoxLayout);
            resizer->addWidgetsFromLayout(infoGroupBoxLayout);
        }

        mUpdateDetailsTab = [=, this] {
            if (mTorrent) {
                //: Torrent's completion size, e.g. 100 MiB of 200 MiB (50%). %1 is completed size, %2 is size, %3 is progress in percents
                completedLabel->setText(qApp->translate("tremotesf", "%1 of %2 (%3)")
                                            .arg(
                                                formatutils::formatByteSize(mTorrent->data().completedSize),
                                                formatutils::formatByteSize(mTorrent->data().sizeWhenDone),
                                                formatutils::formatProgress(mTorrent->data().percentDone)
                                            ));
                downloadedLabel->setText(formatutils::formatByteSize(mTorrent->data().totalDownloaded));
                uploadedLabel->setText(formatutils::formatByteSize(mTorrent->data().totalUploaded));
                ratioLabel->setText(formatutils::formatRatio(mTorrent->data().ratio));
                downloadSpeedLabel->setText(formatutils::formatByteSpeed(mTorrent->data().downloadSpeed));
                uploadSpeedLabel->setText(formatutils::formatByteSpeed(mTorrent->data().uploadSpeed));
                etaLabel->setText(formatutils::formatEta(mTorrent->data().eta));

                const QLocale locale{};
                seedersLabel->setText(locale.toString(mTorrent->data().totalSeedersFromTrackersCount));
                leechersLabel->setText(locale.toString(mTorrent->data().totalLeechersFromTrackersCount));
                peersSendingToUsLabel->setText(locale.toString(mTorrent->data().peersSendingToUsCount));
                peersGettingFromUsLabel->setText(locale.toString(mTorrent->data().peersGettingFromUsCount));

                lastActivityLabel->setText(
                    formatutils::formatDateTime(mTorrent->data().activityDate.toLocalTime(), QLocale::LongFormat)
                );

                totalSizeLabel->setText(formatutils::formatByteSize(mTorrent->data().totalSize));
                locationLabel->setText(
                    toNativeSeparators(mTorrent->data().downloadDirectory, mRpc->serverSettings()->data().pathOs)
                );
                hashLabel->setText(mTorrent->data().hashString);
                creationDateLabel->setText(
                    formatutils::formatDateTime(mTorrent->data().creationDate.toLocalTime(), QLocale::LongFormat)
                );
                labelsModel->setStringList(mTorrent->data().labels);
            } else {
                completedLabel->clear();
                downloadedLabel->clear();
                uploadedLabel->clear();
                ratioLabel->clear();
                downloadSpeedLabel->clear();
                uploadSpeedLabel->clear();
                etaLabel->clear();
                seedersLabel->clear();
                leechersLabel->clear();
                peersSendingToUsLabel->clear();
                peersGettingFromUsLabel->clear();
                lastActivityLabel->clear();
                totalSizeLabel->clear();
                locationLabel->clear();
                hashLabel->clear();
                creationDateLabel->clear();
                labelsModel->setStringList({});
            }
            const bool labelsViewVisible = labelsModel->rowCount() > 0;
            labelsView->setVisible(labelsViewVisible);
            labelsLabel->setVisible(labelsViewVisible);
        };

        QObject::connect(Settings::instance(), &Settings::displayRelativeTimeChanged, this, mUpdateDetailsTab);
    }

    void TorrentPropertiesWidget::setupPeersTab() {
        mPeersModel = new PeersModel(this);
        auto peersProxyModel =
            new BaseProxyModel(mPeersModel, PeersModel::SortRole, static_cast<int>(PeersModel::Column::Address), this);

        auto peersTab = new QWidget(this);
        auto peersTabLayout = new QVBoxLayout(peersTab);

        mPeersView = new QTreeView(this);
        setCommonTreeViewProperties(mPeersView, true);
        mPeersView->setItemDelegate(new TooltipWhenElidedDelegate(this));
        mPeersView->setItemDelegateForColumn(
            static_cast<int>(PeersModel::Column::ProgressBar),
            new ProgressBarDelegate(PeersModel::SortRole, this)
        );
        mPeersView->setModel(peersProxyModel);
        mPeersView->header()->restoreState(Settings::instance()->get_peersViewHeaderState());
        overrideBreezeFramelessScrollAreaHeuristic(mPeersView, true);

        peersTabLayout->addWidget(mPeersView);
        //: Torrent's properties dialog tab
        addTab(peersTab, qApp->translate("tremotesf", "Peers"));
    }

    void TorrentPropertiesWidget::setupLimitsTab() {
        auto limitsTab = new QScrollArea(this);
        limitsTab->setWidgetResizable(true);
        limitsTab->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        makeScrollAreaTransparent(limitsTab);
        //: Torrent's properties dialog tab
        addTab(limitsTab, qApp->translate("tremotesf", "Limits"));

        auto limitsTabScrollContent = new QWidget(limitsTab);
        limitsTab->setWidget(limitsTabScrollContent);

        auto limitsTabLayout = new QVBoxLayout(limitsTabScrollContent);

        //: Torrent's limits tab section
        auto peersGroupBox = new QGroupBox(qApp->translate("tremotesf", "Peers"), this);
        auto peersGroupBoxLayout = new QFormLayout(peersGroupBox);
        peersGroupBoxLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

        auto priorityComboBox = new QComboBox(this);
        for (const TorrentData::Priority priority : priorityComboBoxItems) {
            switch (priority) {
            case TorrentData::Priority::High:
                //: Torrent's loading priority
                priorityComboBox->addItem(qApp->translate("tremotesf", "High"));
                break;
            case TorrentData::Priority::Normal:
                //: Torrent's loading priority
                priorityComboBox->addItem(qApp->translate("tremotesf", "Normal"));
                break;
            case TorrentData::Priority::Low:
                //: Torrent's loading priority
                priorityComboBox->addItem(qApp->translate("tremotesf", "Low"));
                break;
            }
        }
        peersGroupBoxLayout->addRow(qApp->translate("tremotesf", "Torrent priority:"), priorityComboBox);

        auto peersLimitsSpinBox = new QSpinBox(this);
        setSpinBoxLimits(peersLimitsSpinBox, peersLimitRange);
        peersGroupBoxLayout->addRow(qApp->translate("tremotesf", "Maximum peers:"), peersLimitsSpinBox);

        limitsTabLayout->addWidget(peersGroupBox);
        limitsTabLayout->addStretch();

        QObject::connect(
            priorityComboBox,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                if (!mUpdatingLimits && mTorrent) {
                    mTorrent->setBandwidthPriority(priorityComboBoxItems.at(static_cast<size_t>(index)));
                }
            }
        );
        QObject::connect(
            peersLimitsSpinBox,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this,
            [this](int limit) {
                if (!mUpdatingLimits && mTorrent) {
                    mTorrent->setPeersLimit(limit);
                }
            }
        );

        mUpdateLimitsTab = [=, this] {
            mUpdatingLimits = true;

            if (mTorrent) {
                priorityComboBox->setCurrentIndex(
                    indexOfCasted<int>(priorityComboBoxItems, mTorrent->data().bandwidthPriority).value()
                );
                peersLimitsSpinBox->setValue(mTorrent->data().peersLimit);
            } else {
                priorityComboBox->setCurrentIndex(0);
                peersLimitsSpinBox->clear();
            }

            mUpdatingLimits = false;
        };
    }

    void TorrentPropertiesWidget::setTorrent(Torrent* torrent) { setTorrent(torrent, false); }

    void TorrentPropertiesWidget::setTorrent(Torrent* torrent, bool oldTorrentDestroyed) {
        if (torrent == mTorrent) {
            return;
        }
        if (mTorrent && !oldTorrentDestroyed) {
            QObject::disconnect(mTorrent, nullptr, this, nullptr);
        }

        const bool hadTorrent = mTorrent != nullptr;
        mTorrent = torrent;

        const auto update = [this] {
            mUpdateDetailsTab();
            mUpdateLimitsTab();
        };
        update();
        if (mTorrent) {
            QObject::connect(mTorrent, &Torrent::changed, this, update);
            QObject::connect(mTorrent, &QObject::destroyed, this, [this] { setTorrent(nullptr, true); });
        }
        mFilesModel->setTorrent(mTorrent, oldTorrentDestroyed);
        mTrackersViewWidget->setTorrent(mTorrent, oldTorrentDestroyed);
        mPeersModel->setTorrent(mTorrent, oldTorrentDestroyed);

        const auto onHasTorrentChanged = [this](bool hasTorrent) {
            setEnabled(hasTorrent);
            emit hasTorrentChanged(hasTorrent);
        };
        if (hadTorrent && !mTorrent) {
            onHasTorrentChanged(false);
        } else if (!hadTorrent && mTorrent) {
            onHasTorrentChanged(true);
        }
    }
}
