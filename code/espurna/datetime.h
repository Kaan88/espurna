/*

DATETIME MODULE

Copyright (C) 2024 by Maxim Prokhorov <prokhorov dot max at outlook dot com>

*/

#pragma once

#include <Arduino.h>

#include <chrono>
#include <cstdint>
#include <ctime>

#include "types.h"

namespace espurna {
namespace datetime {

using rep_type = time_t;

using Seconds = std::chrono::duration<rep_type>;
using Minutes = std::chrono::duration<rep_type, std::ratio<60> >;
using Hours = std::chrono::duration<rep_type, std::ratio<Minutes::period::num * 60> >;
using Days = std::chrono::duration<rep_type, std::ratio<Hours::period::num * 24> >;
using Weeks = std::chrono::duration<rep_type, std::ratio<Days::period::num * 7> >;

// TODO import std::chrono::{floor,ceil,trunc}

// only -std=c++20 chrono has appropriate class support. until then, use libc tm for full datetime
// helper for to_days / from_days, where only the yyyy-mm-dd is needed
struct Date {
    int year;
    int month;
    int day;
};

// Days since 1970/01/01
Days to_days(const Date&) noexcept;
Days to_days(const tm&) noexcept;

constexpr Date make_date(const tm& t) {
    return Date{
        .year = t.tm_year + 1900,
        .month = t.tm_mon + 1,
        .day = t.tm_mday,
    };
}

struct HhMm {
    int hours;
    int minutes;
};

struct HhMmSs {
    int hours;
    int minutes;
    int seconds;
};

constexpr HhMmSs make_hh_mm_ss(const tm& t) {
    return HhMmSs{
        .hours = t.tm_hour,
        .minutes = t.tm_min,
        .seconds = t.tm_sec,
    };
}

Date from_days(const Days&) noexcept;

// on esp8266 this is a usually an internal timestamp timeshift'ed with `micros64()`
// TODO usec precision only available w/ gettimeofday and would require 64bit time_t
struct Clock {
    using duration = Seconds;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<Clock, duration>;

    static constexpr bool is_steady { false };

    static time_point now() noexcept {
        return time_point(duration(::std::time(nullptr)));
    }
};

// here and later, ref.
// - https://github.com/cassioneri/eaf
// - https://onlinelibrary.wiley.com/doi/full/10.1002/spe.3172
// - (same as above for gcc>13) c++20 chrono algorithms

constexpr bool is_leap_year(int year) {
    // y % 25 == 0 ? y % 16 == 0 : y % 4 == 0;
    // is cited as original implementation
    return (year & (year % 25 == 0 ? 15 : 3)) == 0;
}

constexpr int last_day(int year, int month) {
    // 2nd ternary does not mask 1st bit
    // i.e. (month ^ (month >> 3)) & 1 | 30
    // as noted in cassioneri/eaf, this is unnecessary
	return month != 2
        ? (month ^ (month >> 3)) | 30
	    : is_leap_year(year) ? 29 : 28;
}

constexpr int last_day(const Date& d) {
    return last_day(d.year, d.month);
}

constexpr int last_day(const tm& t) {
    return last_day(t.tm_year + 1900, t.tm_mon + 1);
}

constexpr int day_index(int days) {
    return (days - 1) / 7 + 1;
}

struct Weekday {
    Weekday() = default;

    constexpr explicit Weekday(datetime::Days days) noexcept :
        _value((4 + days.count()) % 7)
    {}

    constexpr explicit Weekday(uint8_t value) :
        _value{ value }
    {}

    static constexpr Weekday min() {
        return Weekday{ _min };
    }

    static constexpr Weekday max() {
        return Weekday{ _max };
    }

    constexpr bool ok() const {
        return (_value <= _max);
    }

    constexpr int value() const {
        return c_value();
    }

    constexpr int c_value() const {
        return _value;
    }

    constexpr int iso_value() const {
        return _value == 0 ? 7 : _value;
    }

    constexpr bool operator==(const Weekday& other) const {
        return _value == other._value;
    }

    constexpr bool operator!=(const Weekday& other) const {
        return _value != other._value;
    }

private:
    static constexpr auto _min = uint8_t{ 0 };
    static constexpr auto _max = uint8_t{ 6 };

    uint8_t _value;
};

constexpr auto Sunday = Weekday{ 0 };
constexpr auto Monday = Weekday{ 1 };
constexpr auto Tuesday = Weekday{ 2 };
constexpr auto Wednesday = Weekday{ 3 };
constexpr auto Thursday = Weekday{ 4 };
constexpr auto Friday = Weekday{ 5 };
constexpr auto Saturday = Weekday{ 6 };

constexpr Weekday next(Weekday day) {
    return (Saturday == day)
        ? Sunday
        : Weekday{ uint8_t(day.value() + 1) };
}

// Seconds since 1970/01/01. Helper function to replace mktime
Seconds to_seconds(const Date&, const HhMmSs&) noexcept;
Seconds to_seconds(const tm&) noexcept;

// main use-case is scheduler, and it needs both tm results
struct Context {
    time_t timestamp;
    tm local;
    tm utc;
};

// generates local and utc tm context for the given timestamp
Context make_context(Seconds);
Context make_context(time_t);

// set target tm to 00:00 and offset N days in the future or past
// input tm *should* be from localtime(_r)
// *can* return negative number on errors, since this uses libc mktime
time_t delta_local(tm&, Days);

// set target tm to 00:00 and offset N days in the future or past
// calculations done through seconds input
// input tm *should* be from gmtime(_r)
time_t delta_utc(tm&, Seconds, Days);

// apply both local and utc operations on the given context
Context delta(const Context&, Days);

// generic string format used for datetime output, without timezone
String format(const tm&);

// prepare 'tm' for the format(const tm&)
String format_local(time_t);
String format_utc(time_t);

// generic iso8601 format w/ timezone offset
String format_local_tz(const Context&);
String format_local_tz(time_t);

String format_utc_tz(const Context&);
String format_utc_tz(const tm&);

} // namespace datetime

constexpr bool operator==(const datetime::Date& lhs, const datetime::Date& rhs) {
    return lhs.year == rhs.year
        && lhs.month == rhs.month
        && lhs.day == rhs.day;
}

constexpr bool operator==(const datetime::Date& lhs, const tm& rhs) {
    return lhs == datetime::make_date(rhs);
}

} // namespace espurna
