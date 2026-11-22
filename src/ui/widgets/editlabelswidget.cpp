// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <ranges>
#include <set>

#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLineEdit>

#include "editlabelswidget.h"

#include "rpc/rpc.h"
#include "rpc/torrent.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        const QIcon& tagIcon() {
            static const auto icon = QIcon::fromTheme("tag"_L1);
            return icon;
        }
    }

    EditLabelsWidget::EditLabelsWidget(const std::vector<QString>& enabledLabels, Rpc* rpc, QWidget* parent)
        : QWidget{parent}, mRpc{rpc} {
        auto layout = new QHBoxLayout(this);
        layout->setContentsMargins(QMargins{});

        mComboBox = new QComboBox(this);
        mComboBox->setEditable(true);
        mComboBox->setInsertPolicy(QComboBox::NoInsert);
        mComboBox->lineEdit()->setPlaceholderText(qApp->translate("tremotesf", "Label"));
        if (!enabledLabels.empty()) {
            mComboBox->setCurrentText(enabledLabels.front());
        }
        layout->addWidget(mComboBox);

        updateComboBoxLabels();
        QObject::connect(rpc, &Rpc::connectedChanged, this, &EditLabelsWidget::updateComboBoxLabels);
    }

    void EditLabelsWidget::setFocusOnComboBox() { mComboBox->setFocus(); }

    bool EditLabelsWidget::comboBoxHasFocus() const { return mComboBox->hasFocus(); }

    std::vector<QString> EditLabelsWidget::enabledLabels() const {
        const auto label = mComboBox->currentText().trimmed();
        if (label.isEmpty()) {
            return {};
        }
        return {label};
    }

    void EditLabelsWidget::updateComboBoxLabels() {
        const auto currentText = mComboBox->currentText();
        mComboBox->clear();
        if (mRpc->isConnected()) {
            const auto allLabels = mRpc->torrents()
                                   | std::views::transform([](const auto& t) { return t->data().labels; })
                                   | std::views::join
                                   | std::views::common // For some reason GCC 14 requies views::common here
                                   | std::ranges::to<std::set>();
            for (const auto& label : allLabels) {
                mComboBox->addItem(tagIcon(), label);
            }
        }
        mComboBox->setCurrentText(currentText);
    }

} // namespace tremotesf
