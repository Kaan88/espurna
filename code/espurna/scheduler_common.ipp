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

struct Schedule {
    DateMatch date;
    WeekdayMatch weekdays;
    TimeMatch time;

    bool ok { false };
};

// by default, relaxed matching. if specific field is not set, assume it is not required
// parser *will* set appropriate bits, but this allows default struct to always be valid

bool match(const DateMatch& lhs, int year, int month, int day) {
    if ((lhs.year != 0) && (lhs.year != year)) {
        return false;
    }

    if (lhs.month.any() && (!lhs.month[month - 1])) {
        return false;
    }

    if (lhs.day_index[0]) {
        return datetime::day_index(datetime::last_day(year, month))
            == datetime::day_index(day);
    }

    if (lhs.day_index.any()) {
        return lhs.day_index[datetime::day_index(day)];
    }

    if (lhs.day[0]) {
        const auto last_day = datetime::last_day(year, month);
        if (lhs.day.count() > 1) {
            return lhs.day[1 + last_day - day];
        }

        return last_day == day;
    }

    if (lhs.day.any() && (!lhs.day[day])) {
        return false;
    }

    return true;

}

inline bool match(const DateMatch& lhs, const datetime::Date& date) {
    return match(lhs, date.year, date.month, date.day);
}

inline bool match(const DateMatch& lhs, const tm& rhs) {
    return match(lhs, datetime::make_date(rhs));
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

const tm& select_time(const datetime::Context& ctx, const Schedule& schedule) {
    return want_utc(schedule.time)
        ? ctx.utc
        : ctx.local;
}

namespace restore {

struct Result {
    size_t index;
    datetime::Minutes offset;
};

struct Pending {
    size_t index;
    Schedule schedule;
};

struct Context {
    Context() = delete;

    explicit Context(const datetime::Context& ctx) :
        base(ctx),
        current(ctx)
    {
        init();
    }

    ~Context() {
        destroy();
    }

    bool next_delta(const datetime::Days&);
    bool next();

    const datetime::Context& base;

    datetime::Context current{};
    datetime::Days days{};

    std::vector<Pending> pending{};
    std::vector<Result> results{};

private:
    void destroy();
    void init();
    void init_delta();
};

bool Context::next_delta(const datetime::Days& days) {
    if (days.count() == 0) {
        return false;
    }

    this->days += days;
    this->current = datetime::delta(this->current, days);
    if (this->current.timestamp < 0) {
        return false;
    }

    this->init_delta();
    return true;
}

bool Context::next() {
    return next_delta(datetime::Days{ -1 });
}

std::bitset<24> mask_past_hours(const std::bitset<24>& lhs, int rhs) {
    return lhs.to_ulong() & bits::fill_u32(0, rhs);
}

std::bitset<60> mask_past_minutes(const std::bitset<60>& lhs, int rhs) {
    return lhs.to_ullong() & bits::fill_u64(0, rhs);
}

TimeMatch mask_past(const TimeMatch& lhs, const tm& rhs) {
    TimeMatch out;
    out.hour = mask_past_hours(lhs.hour, rhs.tm_hour);
    out.minute = mask_past_minutes(lhs.minute, rhs.tm_hour);
    out.flags = lhs.flags;

    return out;
}

datetime::Minutes to_minutes(int hour, int minute) {
    return datetime::Hours{ hour } + datetime::Minutes{ minute };
}

datetime::Minutes to_minutes(const tm& t) {
    return to_minutes(t.tm_hour, t.tm_min);
}

bool closest_delta(datetime::Minutes& out, const TimeMatch& lhs, const tm& rhs) {
    auto past = mask_past(lhs, rhs);
    if (lhs.hour[rhs.tm_hour]) {
        auto minute = bits::first_set_u64(past.minute.to_ullong());
        if (minute == 0) {
            return false;
        }

        out = datetime::Minutes{ rhs.tm_min - minute };
        return true;
    }

    auto hour = bits::first_set_u32(past.hour.to_ulong());
    if (hour == 0) {
        return false;
    }

    auto minute = bits::first_set_u64(lhs.minute.to_ullong());
    if (minute == 0) {
        return false;
    }

    --hour;
    --minute;

    out -= to_minutes(rhs) - to_minutes(hour, minute);

    return true;
}

bool closest_delta_end_of_day(datetime::Minutes& out, const TimeMatch& lhs, const tm& rhs) {
    tm tmp;
    std::memcpy(&tmp, &rhs, sizeof(tm));

    tmp.tm_hour = 23;
    tmp.tm_min = 59;
    tmp.tm_sec = 00;

    const auto result = closest_delta(out, lhs, tmp);
    if (result) {
        out -= datetime::Minutes{ 1 };
        return true;
    }

    return false;
}

void context_pending(Context& ctx, size_t index, const Schedule& schedule) {
    ctx.pending.push_back({.index = index, .schedule = schedule});
}

void context_result(Context& ctx, size_t index, datetime::Minutes offset) {
    ctx.results.push_back({.index = index, .offset = offset});
}

bool handle_today(Context& ctx, size_t index, const Schedule& schedule) {
    const auto& time_point = select_time(ctx.base, schedule);

    if (match(schedule.date, time_point) && match(schedule.weekdays, time_point)) {
        datetime::Minutes offset{};
        if (closest_delta(offset, schedule.time, time_point)) {
            context_result(ctx, index, offset);
            return true;
        }
    }

    context_pending(ctx, index, schedule);

    return false;
}

bool handle_delta(Context& ctx, const Pending& pending) {
    if (!pending.schedule.ok) {
        return false;
    }

    const auto& time_point = select_time(ctx.current, pending.schedule);

    if (match(pending.schedule.date, time_point) && match(pending.schedule.weekdays, time_point)) {
        datetime::Minutes offset{ ctx.days - datetime::Days{ -1 }};
        if (closest_delta_end_of_day(offset, pending.schedule.time, time_point)) {
            offset -= to_minutes(select_time(ctx.base, pending.schedule));
            context_result(ctx, pending.index, offset);
            return true;
        }
    }

    return false;
}

bool handle_delta(Context& ctx, decltype(Context::pending)::iterator it) {
    if (handle_delta(ctx, *it)) {
        ctx.pending.erase(it);
        return true;
    }

    return false;
}

} // namespace restore

} // namespace
} // namespace scheduler
} // namespace espurna
