/*

DATETIME MODULE

Copyright (C) 2024 by Maxim Prokhorov <prokhorov dot max at outlook dot com>

*/

#include "datetime.h"

namespace espurna {
namespace datetime {
namespace {

time_t delta_local_impl(tm& out, Days days) {
    out.tm_mday += days.count();
    out.tm_hour = 0;
    out.tm_min = 0;
    out.tm_sec = 0;

    out.tm_isdst = -1;

    return mktime(&out);
}

time_t delta_utc_impl(tm& out, Seconds seconds, Days days) {
    const auto timestamp = start_of_day_offset(seconds, days);

    time_t tmp { timestamp.count() };
    gmtime_r(&tmp, &out);

    return tmp;
}

} // namespace

// Days since 1970/01/01.
// Proposition 6.2 of Neri and Schneider,
// "Euclidean Affine Functions and Applications to Calendar Algorithms".
// https://arxiv.org/abs/2102.06959
Days to_days(const Date& date) noexcept {
    auto constexpr __z2    = static_cast<uint32_t>(-1468000);
    auto constexpr __r2_e3 = static_cast<uint32_t>(536895458);

    const auto __y1 = static_cast<uint32_t>(date.year) - __z2;
    const auto __m1 = static_cast<uint32_t>(
            static_cast<unsigned>(date.month));
    const auto __d1 = static_cast<uint32_t>(
            static_cast<unsigned>(date.day));

    const auto __j  = static_cast<uint32_t>(__m1 < 3);
    const auto __y0 = __y1 - __j;
    const auto __m0 = __j ? __m1 + 12 : __m1;
    const auto __d0 = __d1 - 1;

    const auto __q1 = __y0 / 100;
    const auto __yc = 1461 * __y0 / 4 - __q1 + __q1 / 4;
    const auto __mc = (979 *__m0 - 2919) / 32;
    const auto __dc = __d0;

    return Days{
        static_cast<int32_t>(__yc + __mc + __dc - __r2_e3)};
}

Days to_days(const tm& t) noexcept {
    return to_days(make_date(t));
}

// Construct from days since 1970/01/01.
// Proposition 6.3 of Neri and Schneider,
// "Euclidean Affine Functions and Applications to Calendar Algorithms".
// https://arxiv.org/abs/2102.06959
Date from_days(Days days) noexcept {
    constexpr auto __z2    = static_cast<uint32_t>(-1468000);
    constexpr auto __r2_e3 = static_cast<uint32_t>(536895458);

    const auto __r0 = static_cast<uint32_t>(days.count()) + __r2_e3;

    const auto __n1 = 4 * __r0 + 3;
    const auto __q1 = __n1 / 146097;
    const auto __r1 = __n1 % 146097 / 4;

    constexpr auto __p32 = static_cast<uint64_t>(1) << 32;
    const auto __n2 = 4 * __r1 + 3;
    const auto __u2 = static_cast<uint64_t>(2939745) * __n2;
    const auto __q2 = static_cast<uint32_t>(__u2 / __p32);
    const auto __r2 = static_cast<uint32_t>(__u2 % __p32) / 2939745 / 4;

    constexpr auto __p16 = static_cast<uint32_t>(1) << 16;
    const auto __n3 = 2141 * __r2 + 197913;
    const auto __q3 = __n3 / __p16;
    const auto __r3 = __n3 % __p16 / 2141;

    const auto __y0 = 100 * __q1 + __q2;
    const auto __m0 = __q3;
    const auto __d0 = __r3;

    const auto __j  = __r2 >= 306;
    const auto __y1 = __y0 + __j;
    const auto __m1 = __j ? __m0 - 12 : __m0;
    const auto __d1 = __d0 + 1;

    return Date{
        .year = static_cast<int>(__y1 + __z2),
        .month = static_cast<uint8_t>(__m1),
        .day = static_cast<uint8_t>(__d1)};
}

Context make_context(time_t timestamp) {
    Context out;
    out.timestamp = timestamp;

    localtime_r(&timestamp, &out.local);
    gmtime_r(&timestamp, &out.utc);

    return out;
}

time_t delta_local(tm& out, Days days) {
    return delta_local_impl(out, days);
}

time_t delta_utc(tm& out, Seconds seconds, Days days) {
    return delta_utc_impl(out, seconds, days);
}

Context delta(const datetime::Context& ctx, Days days) {
    Context out = ctx;

    const auto local = delta_local_impl(out.local, days);
    if (local < 0) {
        out.timestamp = -1;
        return out;
    }

    out.timestamp = delta_utc_impl(out.utc,
        Seconds{ out.timestamp }, days);

    return out;
}

String format(const tm& t) {
    char buffer[32];
    snprintf_P(buffer, sizeof(buffer),
        PSTR("%04d-%02d-%02d %02d:%02d:%02d"),
        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
        t.tm_hour, t.tm_min, t.tm_sec);

    return String(buffer);
}

String format_local(time_t timestamp) {
    tm tmp;
    localtime_r(&timestamp, &tmp);
    return format(tmp);
}

String format_utc(time_t timestamp) {
    tm tmp;
    gmtime_r(&timestamp, &tmp);
    return format(tmp);
}

} // namespace datetime
} // namespace espurna
