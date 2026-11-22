// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tracker.h"

#include <QHostAddress>
#include <QUrl>

#ifndef TREMOTESF_REGISTRABLE_DOMAIN_QT
#    include <libpsl.h>
#endif

#include "stdutils.h"

using namespace Qt::StringLiterals;

namespace tremotesf {
    namespace {
        QDateTime dateTimeFromSeconds(const QVariant& value) {
            const auto seconds = value.toLongLong();
            if (seconds <= 0) {
                return QDateTime{{}, {}, QTimeZone::utc()};
            }
            return QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::utc());
        }
    }

    Tracker::Tracker(int id, const QVariantList& row) : mId(id) { update(row); }

    bool Tracker::update(const QVariantList& row) {
        bool changed = false;

        if (row.size() >= 1) {
            QString announce(row.at(0).toString());
            if (announce != mAnnounce) {
                changed = true;
                mAnnounce = std::move(announce);
                mSite = impl::registrableDomainFromUrl(QUrl(mAnnounce));
            }
        }
        if (row.size() >= 2) {
            setChanged(mTier, row.at(1).toInt(), changed);
        }

        const bool enabled = row.size() >= 3 && (row.at(2).toBool() || row.at(2).toInt() != 0);
        const bool usable = row.size() < 4 || row.at(3).toBool() || row.at(3).toInt() != 0;
        const bool busy = row.size() >= 5 && (row.at(4).toBool() || row.at(4).toInt() != 0);

        Status status{};
        if (!enabled) {
            status = Status::Inactive;
        } else if (busy) {
            status = Status::Updating;
        } else {
            status = Status::WaitingForUpdate;
        }
        setChanged(mStatus, status, changed);

        const int successCounter = row.size() >= 10 ? row.at(9).toInt() : 0;
        const int failedCounter = row.size() >= 11 ? row.at(10).toInt() : 0;
        setChanged(
            mErrorMessage,
            (!usable && failedCounter > successCounter) ? QString("Tracker error"_L1) : QString{},
            changed
        );

        if (row.size() >= 6) {
            setChanged(mSeeders, row.at(5).toInt(), changed);
        }
        if (row.size() >= 7) {
            setChanged(mLeechers, row.at(6).toInt(), changed);
        }
        if (row.size() >= 8) {
            setChanged(mPeers, row.at(7).toInt(), changed);
        }
        if (row.size() >= 9) {
            setChanged(mNextUpdateTime, dateTimeFromSeconds(row.at(8)), changed);
        }

        return changed;
    }
}

#ifdef TREMOTESF_REGISTRABLE_DOMAIN_QT
// Private Qt API
bool qIsEffectiveTLD(QStringView domain);

namespace {
    QString registrableDomainFromDomain(const QString& fullDomain, [[maybe_unused]] const QUrl& url) {
        QStringView domain = fullDomain;
        QStringView previousDomain = fullDomain;
        while (!domain.isEmpty()) {
            if (qIsEffectiveTLD(domain)) {
                return previousDomain.toString();
            }
            const auto dotIndex = domain.indexOf('.');
            if (dotIndex == -1) {
                break;
            }
            previousDomain = domain;
            domain = domain.sliced(dotIndex + 1);
        }
        return fullDomain;
    }
}
#else
namespace {
    QString registrableDomainFromDomain(const QString& fullDomain, [[maybe_unused]] const QUrl& url) {
        const auto fullDomainUtf8 = fullDomain.toUtf8();
        const auto psl = psl_builtin();
        if (!psl) {
            return fullDomain;
        }
        const auto registrable = psl_registrable_domain(psl, fullDomainUtf8);
        return registrable ? QString(registrable) : fullDomain;
    }
}
#endif

QString tremotesf::impl::registrableDomainFromUrl(const QUrl& url) {
    auto host = url.host().toLower().normalized(QString::NormalizationForm_KC);
    if (host.isEmpty()) {
        return {};
    }
    if (const bool isIpAddress = !QHostAddress(host).isNull(); isIpAddress) {
        return host;
    }
    return registrableDomainFromDomain(host, url);
}
