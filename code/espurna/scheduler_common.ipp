/*

Part of SCHEDULER MODULE

Copyright (C) 2017 by faina09
Adapted by Xose Pérez <xose dot perez at gmail dot com>

Copyright (C) 2019-2024 by Maxim Prokhorov <prokhorov dot max at outlook dot com>

*/

#pragma once

#include "datetime.h"

#include <bitset>

namespace espurna {
namespace scheduler {
namespace {

struct DateMatch {
    // simply store the year as-is
    uint16_t year { 0 };

    // [0..11] - Nth month
    std::bitset<12> month;

    // [0]     - Nth day, starting from the end-of-month
    // [1..31] - Nth day in the current month
    std::bitset<32> day;

    // [0]    - last weekday
    // [1..5] - Nth weekday
    std::bitset<6> day_index;
};

struct WeekdayMatch {
    // [0..6] - Nth day in the week
    // [7]    - reserved
    std::bitset<8> day;
};

struct TimeMatch {
    // [0..23]
    std::bitset<24> hour;

    // [0..59] (note that we don't handle leap seconds)
    std::bitset<60> minute;

    // extra matching conditions, defined by the implementation
    uint8_t flags { 0 };
};

constexpr uint8_t FlagUtc = 1;
constexpr uint8_t FlagSunrise = 1 << 1;
constexpr uint8_t FlagSunset = 1 << 2;

// by default, relaxed matching. if specific field is not set, assume it is not required
// parser *will* set appropriate bits, but this allows default struct to always be valid

bool match(const DateMatch& lhs, const tm& rhs) {
    if ((lhs.year != 0) && ((lhs.year - 1900) != rhs.tm_year)) {
        return false;
    }

    if (lhs.month.any() && (!lhs.month[rhs.tm_mon])) {
        return false;
    }

    if (lhs.day_index[0]) {
        return datetime::day_index(datetime::last_day(rhs))
            == datetime::day_index(rhs.tm_mday);
    }

    if (lhs.day_index.any()) {
        return lhs.day_index[datetime::day_index(rhs.tm_mday)];
    }

    if (lhs.day[0]) { 
        const auto day = datetime::last_day(rhs);
        if (lhs.day.count() > 1) {
            return lhs.day[1 + day - rhs.tm_mday];
        }

        return day == rhs.tm_mday;
    }

    if (lhs.day.any() && (!lhs.day[rhs.tm_mday])) {
        return false;
    }

    return true;
}

bool match(const WeekdayMatch& lhs, const tm& rhs) {
    if (lhs.day[7]) {
        return false;
    }

    if (lhs.day.none()) {
        return true;
    }

    return lhs.day[rhs.tm_wday];
}

bool match(const TimeMatch& lhs, const tm& rhs) {
    if (lhs.hour.any() && (!lhs.hour[rhs.tm_hour])) {
        return false;
    }

    if (lhs.minute.any() && (!lhs.minute[rhs.tm_min])) {
        return false;
    }

    return true;
}

constexpr bool want_utc(const TimeMatch& m) {
    return (m.flags & FlagUtc) > 0;
}

constexpr bool want_sunrise(const TimeMatch& m) {
    return (m.flags & FlagSunrise) > 0;
}

constexpr bool want_sunset(const TimeMatch& m) {
    return (m.flags & FlagSunset) > 0;
}

constexpr bool want_sunrise_sunset(const TimeMatch& m) {
    return want_sunrise(m) || want_sunset(m);
}

namespace bits {

#if __cplusplus > 201411L
#define CONSTEXPR17 constexpr
#else
#define CONSTEXPR17
#endif

// fill u32 or 64 [begin, end] with ones, keep the rest as zeroes

template <typename T>
CONSTEXPR17 T fill_generic(uint8_t begin, uint8_t end) {
    T mask = std::numeric_limits<T>::max();
    mask >>= (sizeof(T) * 8) - T(end - begin);
    mask <<= T(begin);

    return mask;
}

template <typename T>
CONSTEXPR17 T fill_generic_inverse(uint8_t begin, uint8_t end) {
    T high = std::numeric_limits<T>::max();
    high <<= T(begin);

    T low = std::numeric_limits<T>::max();
    low >>= (sizeof(T) * 8) - T(end);

    return high | low;
}

CONSTEXPR17 uint32_t fill_u32(uint8_t begin, uint8_t end) {
    return fill_generic<uint32_t>(begin, end);
}

CONSTEXPR17 uint32_t fill_u32_inverse(uint8_t begin, uint8_t end) {
    return fill_generic_inverse<uint32_t>(begin, end);
}

CONSTEXPR17 uint64_t fill_u64(uint8_t begin, uint8_t end) {
    return fill_generic<uint64_t>(begin, end);
}

CONSTEXPR17 uint32_t fill_u64_inverse(uint8_t begin, uint8_t end) {
    return fill_generic_inverse<uint64_t>(begin, end);
}

#undef CONSTEXPR17

// helper class for [begin, end] bit range

struct Range {
    Range(uint8_t begin, uint8_t end) :
        _begin(begin),
        _end(end)
    {}

    bool valid(uint8_t value) {
        return (value >= _begin) && (value <= _end);
    }

    void fill(uint8_t begin, uint8_t end, uint8_t repeat) {
        if (begin > end) {
            _fill_inverse(begin, end, repeat);
        } else {
            _fill(begin, end, repeat);
        }
    }

    void fill(uint8_t begin, uint8_t end) {
        _fill(begin, end, 1);
    }

    void set() {
        _mask.set();
    }

    void set(uint8_t index) {
        _mask[index] = true;
    }

    void reset() {
        _mask.reset();
    }

    void reset(uint8_t index) {
        _mask[index] = false;
    }

    int begin() const {
        return _begin;
    }

    int end() const {
        return _end;
    }

    static constexpr int min() {
        return 0;
    }

    static constexpr int max() {
        return SizeMax;
    }

    uint32_t to_u32() const {
        return _mask.to_ulong();
    }

    uint64_t to_u64() const {
        return _mask.to_ullong();
    }

    std::string to_string() const {
        return _mask.to_string();
    }

private:
    void _fill_inverse(uint8_t begin, uint8_t end, uint8_t repeat) {
        if (repeat == 1) {
            _mask |= fill_u64_inverse(begin, end + 1);
            return;
        }

        for (int n = begin; n <= _end; n += repeat) {
            _mask[n] = true;
        }

        for (int n = _begin; n <= end; n += repeat) {
            _mask[n] = true;
        }
    }

    void _fill(uint8_t begin, uint8_t end, uint8_t repeat) {
        if (repeat == 1) {
            _mask |= fill_u64(begin, end + 1);
            return;
        }

        for (int n = begin; n <= end; n += repeat) {
            _mask[n] = true;
        }
    }

    static constexpr auto SizeMax = size_t{ 64 };
    using Mask = std::bitset<SizeMax>;

    uint8_t _begin;
    uint8_t _end;

    Mask _mask{};
};

// check whether first Nth bit is set. handles value == 0 by returning 0

constexpr int first_set_u32(uint32_t value) {
    return __builtin_ffs(value);
}

constexpr int first_set_u64(uint64_t value) {
    return __builtin_ffsll(value);
}

} // namespace bits

WeekdayMatch& operator|=(WeekdayMatch& lhs, const datetime::Weekday& rhs) {
    lhs.day.set(rhs.c_value());
    return lhs;
}

WeekdayMatch& operator|=(WeekdayMatch& lhs, const WeekdayMatch& rhs) {
    lhs.day |= rhs.day;
    return lhs;
}

WeekdayMatch fill_match(datetime::Weekday lhs, datetime::Weekday rhs) {
    WeekdayMatch out;

    if (!lhs.ok() || !rhs.ok()) {
        out.day[7] = true;
        return out;
    }

    for (auto day = lhs; day != next(rhs); day = next(day)) {
        out.day[day.c_value()] = true;
    }

    return out;
}

} // namespace
} // namespace scheduler
} // namespace espurna
