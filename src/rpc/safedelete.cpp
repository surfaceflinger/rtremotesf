// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "safedelete.h"

#include <QStringList>

using namespace Qt::StringLiterals;

namespace tremotesf::impl {
    namespace {
        QString normalizeUnixPath(QStringView path) {
            QStringList parts{};
            for (const auto& part : path.split('/')) {
                if (part.isEmpty() || part == "."_L1) {
                    continue;
                }
                if (part == ".."_L1) {
                    return {};
                }
                parts.push_back(part.toString());
            }
            if (parts.isEmpty()) {
                return "/"_L1;
            }
            return "/"_L1 + parts.join('/');
        }

        const QStringList& systemDirectories() {
            static const QStringList directories{"/"_L1,      "/bin"_L1,    "/boot"_L1,  "/dev"_L1,   "/etc"_L1,
                                                 "/home"_L1,  "/lib"_L1,    "/lib64"_L1, "/media"_L1, "/mnt"_L1,
                                                 "/opt"_L1,   "/proc"_L1,   "/root"_L1,  "/run"_L1,   "/sbin"_L1,
                                                 "/srv"_L1,   "/sys"_L1,    "/tmp"_L1,   "/usr"_L1,   "/var"_L1,
                                                 "/Users"_L1, "/Volumes"_L1};
            return directories;
        }
    }

    bool isSafeToDeletePath(const QString& path, const QString& directory) {
        if (path.isEmpty() || directory.isEmpty()) {
            return false;
        }
        const auto normalizedPath = normalizeUnixPath(path);
        const auto normalizedDirectory = normalizeUnixPath(directory);
        if (normalizedPath.isEmpty() || normalizedDirectory.isEmpty()) {
            return false;
        }
        if (!normalizedPath.startsWith('/') || !normalizedDirectory.startsWith('/')) {
            return false;
        }
        if (normalizedPath == "/"_L1 || normalizedDirectory == "/"_L1) {
            return false;
        }
        if (normalizedPath == normalizedDirectory) {
            return false;
        }
        if (!normalizedPath.startsWith(normalizedDirectory + '/')) {
            return false;
        }
        const auto relative = normalizedPath.sliced(normalizedDirectory.size() + 1);
        if (relative.isEmpty() || relative.contains(".."_L1)) {
            return false;
        }
        return !systemDirectories().contains(normalizedDirectory);
    }
}
