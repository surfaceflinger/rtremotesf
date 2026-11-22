// SPDX-FileCopyrightText: 2015-2025 Alexey Rochev
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TREMOTESF_RPC_NUMBERVALUERANGE_H
#define TREMOTESF_RPC_NUMBERVALUERANGE_H

#include <limits>

#include <QSpinBox>
#include <QDoubleSpinBox>

namespace tremotesf {
    template<typename T>
    struct NumberValueRange {
        T min;
        T max;
    };

    inline constexpr auto nonNegativeIntegersRange = NumberValueRange{.min = 0, .max = std::numeric_limits<int>::max()};

    inline constexpr auto nonNegativeDecimalsRange =
        NumberValueRange{.min = 0.0, .max = std::numeric_limits<double>::max()};

    // Port and peer/minute limits are unsigned 16-bit values
    inline constexpr auto minutesRange =
        NumberValueRange{.min = 0, .max = static_cast<int>(std::numeric_limits<unsigned short>::max())};
    inline constexpr auto portsRange = minutesRange;
    inline constexpr auto peersLimitRange = minutesRange;

    // Speed limits are in kB/s
    inline constexpr auto speedLimitKbpsRange =
        NumberValueRange{.min = 0, .max = static_cast<int>(std::numeric_limits<unsigned int>::max() / 1000)};

    inline void setSpinBoxLimits(QSpinBox* spinBox, NumberValueRange<int> range) {
        spinBox->setMinimum(range.min);
        spinBox->setMaximum(range.max);
    }

    inline void setSpinBoxLimits(QDoubleSpinBox* spinBox, NumberValueRange<double> range) {
        spinBox->setMinimum(range.min);
        spinBox->setMaximum(range.max);
    }
}

#endif // TREMOTESF_RPC_NUMBERVALUERANGE_H
