// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_SERVEREDITDIALOG_H
#define TREMOTESF_SERVEREDITDIALOG_H

#include <functional>
#include <QDialog>

class QComboBox;
class QCheckBox;
class QDialogButtonBox;
class QLineEdit;
class QSpinBox;

namespace tremotesf {
    class MountedDirectoriesWidget;
    class ServersModel;

    class ServerEditDialog final : public QDialog {
        Q_OBJECT

    public:
        explicit ServerEditDialog(ServersModel* serversModel, int row, QWidget* parent = nullptr);
        void accept() override;

    private:
        std::function<void()> setupUi();
        void setServer();

        ServersModel* mServersModel;
        QString mServerName;

        QLineEdit* mNameLineEdit = nullptr;
        QLineEdit* mAddressLineEdit = nullptr;
        QSpinBox* mPortSpinBox = nullptr;
        QLineEdit* mSocketPathLineEdit = nullptr;

        QComboBox* mProxyTypeComboBox = nullptr;
        QLineEdit* mProxyHostnameLineEdit = nullptr;
        QSpinBox* mProxyPortSpinBox = nullptr;
        QLineEdit* mProxyUserLineEdit = nullptr;
        QLineEdit* mProxyPasswordLineEdit = nullptr;

        QSpinBox* mUpdateIntervalSpinBox = nullptr;
        QSpinBox* mTimeoutSpinBox = nullptr;

        QSpinBox* mAutoReconnectSpinBox = nullptr;
        QCheckBox* mAutoReconnectCheckBox = nullptr;

        MountedDirectoriesWidget* mMountedDirectoriesWidget = nullptr;

        QDialogButtonBox* mDialogButtonBox = nullptr;
    };
}

#endif // TREMOTESF_SERVEREDITDIALOG_H
