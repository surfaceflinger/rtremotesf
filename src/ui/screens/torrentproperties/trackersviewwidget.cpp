// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "trackersviewwidget.h"

#include <array>
#include <ranges>

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>

#include "rpc/torrent.h"
#include "rpc/tracker.h"
#include "ui/itemmodels/baseproxymodel.h"
#include "ui/widgets/basetreeview.h"
#include "ui/widgets/textinputdialog.h"
#include "rpc/rpc.h"
#include "settings.h"
#include "trackersmodel.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        class EnterEatingTreeView final : public QTreeView {
            Q_OBJECT

        public:
            explicit EnterEatingTreeView(QWidget* parent = nullptr) : QTreeView(parent) {}

        protected:
            void keyPressEvent(QKeyEvent* event) override {
                QTreeView::keyPressEvent(event);
                switch (event->key()) {
                case Qt::Key_Enter:
                case Qt::Key_Return:
                    event->accept();
                default:
                    break;
                }
            }
        };
    }

    TrackersViewWidget::TrackersViewWidget(Rpc* rpc, QWidget* parent)
        : QWidget(parent),
          mRpc(rpc),
          mModel(new TrackersModel(this)),
          mProxyModel(new BaseProxyModel(
              mModel, TrackersModel::SortRole, static_cast<int>(TrackersModel::Column::Announce), this
          )),
          mTrackersView(new EnterEatingTreeView(this)) {
        auto layout = new QHBoxLayout(this);

        setCommonTreeViewProperties(mTrackersView, true);
        mTrackersView->setContextMenuPolicy(Qt::CustomContextMenu);
        mTrackersView->setModel(mProxyModel);
        mTrackersView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        mTrackersView->header()->restoreState(Settings::instance()->get_trackersViewHeaderState());
        layout->addWidget(mTrackersView);

        auto buttonsLayout = new QVBoxLayout();
        layout->addLayout(buttonsLayout);
        auto addTrackersButton = new QPushButton(
            QIcon::fromTheme("list-add"_L1),
            //: Button
            qApp->translate("tremotesf", "Add..."),
            this
        );
        QObject::connect(addTrackersButton, &QPushButton::clicked, this, &TrackersViewWidget::addTrackers);
        buttonsLayout->addWidget(addTrackersButton);
        auto reannounceButton = new QPushButton(
            QIcon::fromTheme("view-refresh"_L1),
            //: Button
            qApp->translate("tremotesf", "Reanno&unce"),
            this
        );
        QObject::connect(reannounceButton, &QPushButton::clicked, this, [=, this] {
            mRpc->reannounceTorrents(std::array{mTorrent->data().id});
        });
        buttonsLayout->addWidget(reannounceButton);
        buttonsLayout->addStretch();
    }

    void TrackersViewWidget::setTorrent(Torrent* torrent, bool oldTorrentDestroyed) {
        mTorrent = torrent;
        mModel->setTorrent(torrent, oldTorrentDestroyed);
    }

    void TrackersViewWidget::saveState() {
        Settings::instance()->set_trackersViewHeaderState(mTrackersView->header()->saveState());
    }

    void TrackersViewWidget::addTrackers() {
        auto dialog = new TextInputDialog(
            //: Dialog title
            qApp->translate("tremotesf", "Add Trackers"),
            qApp->translate("tremotesf", "Trackers announce URLs:"),
            QString(),
            //: Dialog confirmation button
            qApp->translate("tremotesf", "Add"),
            true,
            this
        );
        QObject::connect(dialog, &TextInputDialog::accepted, this, [=, this] {
            auto lines = dialog->text().split('\n', Qt::SkipEmptyParts);
            mTorrent->addTrackers(
                lines
                | std::views::transform([](QString& announceUrl) { return std::set{std::move(announceUrl)}; })
                | std::ranges::to<std::vector>()
            );
        });
        dialog->show();
    }

}

#include "trackersviewwidget.moc"
