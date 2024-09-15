#include <unity.h>

#include <Arduino.h>
#include <StreamString.h>
#include <ArduinoJson.h>

#include <string_view>
using namespace std::string_view_literals;

#include <iomanip>
#include <iostream>

std::string_view wday(int value) {
    switch (value) {
    case 0:
        return "Sun"sv;
    case 1:
        return "Mon"sv;
    case 2:
        return "Tue"sv;
    case 3:
        return "Wed"sv;
    case 4:
        return "Thu"sv;
    case 5:
        return "Fri"sv;
    case 6:
        return "Sat"sv;
    }

    return ""sv;
}

std::ostream& operator<<(std::ostream& out, const tm& t) {
    out.fill('0');
    out << (1900 + t.tm_year)
        << '-' << std::setw(2) << (1 + t.tm_mon)
        << '-' << std::setw(2) << t.tm_mday
        << ' ' << wday(t.tm_wday) << ' ';

    if (t.tm_hour < 10) {
        out << '0';
    }

    out << t.tm_hour << ':';

    if (t.tm_min < 10) {
        out << '0';
    }

    out << t.tm_min << ':';

    if (t.tm_sec < 10) {
        out << '0';
    }

    out << t.tm_sec << ' ';

    if (t.tm_isdst > 0) {
        out << "DST";
    } else if (t.tm_isdst == 0) {
        out << "SDT";
    } else {
        out << "*";
    }

    return out;
}

// on-device, these can only *only* be TZ variables. locally, depends on tzdata.
// currently, only used in DST <-> SDT tests. general schedule tests are in UTC
struct WithTimezone {
    explicit WithTimezone(const char* tz) :
        _tz(getenv("TZ"))
    {
        setenv("TZ", tz, 1);
        tzset();
    }

    ~WithTimezone() {
        if (_tz.length()) {
            setenv("TZ", _tz.c_str(), 1);
        } else {
            unsetenv("TZ");
        }

        tzset();
    }

private:
    String _tz;
};

#include <espurna/utils.h>
#include <espurna/scheduler_common.ipp>
#include <espurna/scheduler_sun.ipp>
#include <espurna/scheduler_time.re.ipp>

#include <ctime>

#include <array>

namespace espurna {
namespace scheduler {
namespace {

constexpr auto OneHour = datetime::Seconds(datetime::Hours(1));

namespace restore {

[[gnu::used]]
void Context::init() {
}

[[gnu::used]]
void Context::init_delta() {
}

[[gnu::used]]
void Context::destroy() {
}

} // namespace restore

namespace test {

// at 2006-01-02T15:04:05-07:00 TZ='US/Pacific'
static constexpr auto ReferenceTimestamp = time_t{ 1136239445 };

#define MAKE_TIMEPOINT(NAME)\
    tm NAME{};\
    gmtime_r(&ReferenceTimestamp, &NAME)

#define TEST_SCHEDULER_MATCH(M) ([&](){\
    MAKE_TIMEPOINT(time_point);\
    TEST_ASSERT(scheduler::match(M, time_point));})()

void test_date_impl() {
    scheduler::DateMatch m;
    m.year = 2006;
    m.day.set(1);
    m.day_index.set(1);
    m.month.set(0);

    TEST_SCHEDULER_MATCH(m);
}

#define TEST_SCHEDULER_INVALID_DATE(FMT) ([](){\
    scheduler::DateMatch m;\
    TEST_ASSERT_FALSE(scheduler::parse_date(m, FMT));\
    })()

void test_date_parsing_invalid() {
    TEST_SCHEDULER_INVALID_DATE("");
    TEST_SCHEDULER_INVALID_DATE("0");
    TEST_SCHEDULER_INVALID_DATE("2");
    TEST_SCHEDULER_INVALID_DATE("foo bar");
    TEST_SCHEDULER_INVALID_DATE("2049-");
    TEST_SCHEDULER_INVALID_DATE("-");
    TEST_SCHEDULER_INVALID_DATE("---");
    TEST_SCHEDULER_INVALID_DATE("*");
    TEST_SCHEDULER_INVALID_DATE("***");
    TEST_SCHEDULER_INVALID_DATE("***");
    TEST_SCHEDULER_INVALID_DATE("2049-06-30 ???");
    TEST_SCHEDULER_INVALID_DATE("   2049-07-01 *Aasdkasd");
    TEST_SCHEDULER_INVALID_DATE("  2049-07-02  ");
    TEST_SCHEDULER_INVALID_DATE(" 2049-07-03");
    TEST_SCHEDULER_INVALID_DATE("  2049-07-0");
    TEST_SCHEDULER_INVALID_DATE(" 2049-5-0 ");
    TEST_SCHEDULER_INVALID_DATE(" 249-5-0");
    TEST_SCHEDULER_INVALID_DATE("  249-5-0 ");
    TEST_SCHEDULER_INVALID_DATE("  49-5-0 ");
    TEST_SCHEDULER_INVALID_DATE("35-5");
    TEST_SCHEDULER_INVALID_DATE("35-");
    TEST_SCHEDULER_INVALID_DATE("3-");
    TEST_SCHEDULER_INVALID_DATE("-70");
    TEST_SCHEDULER_INVALID_DATE("-99-");
    TEST_SCHEDULER_INVALID_DATE("99-");
    TEST_SCHEDULER_INVALID_DATE("1778-*-*");
    TEST_SCHEDULER_INVALID_DATE("11111-*-*");
    TEST_SCHEDULER_INVALID_DATE("1900-11-*");
    TEST_SCHEDULER_INVALID_DATE("-100-04-*");
    TEST_SCHEDULER_INVALID_DATE("*-10 end of input");
    TEST_SCHEDULER_INVALID_DATE("*-end of input");
}

#define TEST_SCHEDULER_VALID_DATE(FMT) ([](){\
    scheduler::DateMatch m;\
    TEST_ASSERT(scheduler::parse_date(m, FMT));\
    TEST_SCHEDULER_MATCH(m);\
    })()

void test_date_parsing() {
    TEST_SCHEDULER_VALID_DATE("01-02");
    TEST_SCHEDULER_VALID_DATE("2006-01-02");
    TEST_SCHEDULER_VALID_DATE("*-01-02");
    TEST_SCHEDULER_VALID_DATE("2006-*-02");
    TEST_SCHEDULER_VALID_DATE("2006-01-*");
    TEST_SCHEDULER_VALID_DATE("01-*");
    TEST_SCHEDULER_VALID_DATE("*-02");
    TEST_SCHEDULER_VALID_DATE("*-*");
    TEST_SCHEDULER_VALID_DATE("01-1..7");
    TEST_SCHEDULER_VALID_DATE("01-1/1");
    TEST_SCHEDULER_VALID_DATE("01-L1..30");
    TEST_SCHEDULER_VALID_DATE("01-W1");
}

#define TEST_SCHEDULER_VALID_WEEK_INDEX(INDEX, FMT) ([](){\
    TEST_ASSERT_GREATER_OR_EQUAL(1, INDEX);\
    TEST_ASSERT_LESS_OR_EQUAL(5, INDEX);\
    MAKE_TIMEPOINT(time_point);\
    scheduler::DateMatch m;\
    time_point.tm_mday += (7 * (INDEX - 1));\
    TEST_ASSERT_GREATER_OR_EQUAL(1, time_point.tm_mday);\
    TEST_ASSERT_LESS_THAN(32, time_point.tm_mday);\
    TEST_ASSERT(scheduler::parse_date(m, FMT));\
    TEST_ASSERT(scheduler::match(m, time_point));\
    })()

void test_date_year() {
    MAKE_TIMEPOINT(time_point);

    scheduler::DateMatch m;
    TEST_ASSERT(scheduler::parse_date(m, "*-*-2"));

    time_point.tm_year = 100;

    for (int year = 0; year < 199; ++year) {
        TEST_ASSERT_EQUAL(100 + year, time_point.tm_year);
        TEST_ASSERT(scheduler::match(m, time_point));
        time_point.tm_year += 1;
    }
}

void test_date_week_index() {
    TEST_SCHEDULER_VALID_WEEK_INDEX(1, "01-W1");
    TEST_SCHEDULER_VALID_WEEK_INDEX(2, "01-W2");
    TEST_SCHEDULER_VALID_WEEK_INDEX(3, "01-W3");
    TEST_SCHEDULER_VALID_WEEK_INDEX(4, "01-W4");
    TEST_SCHEDULER_VALID_WEEK_INDEX(5, "01-W5");
    TEST_SCHEDULER_VALID_WEEK_INDEX(5, "01-WL");
}

void test_date_month_glob() {
    MAKE_TIMEPOINT(time_point);

    scheduler::DateMatch m;
    TEST_ASSERT(scheduler::parse_date(m, "*-2"));
    TEST_ASSERT(scheduler::match(m, time_point));

    for (int month = 0; month < 12; ++month) {
        TEST_ASSERT_EQUAL(month, time_point.tm_mon);
        TEST_ASSERT(scheduler::match(m, time_point));
        time_point.tm_mon += 1;
    }
}

void test_date_day_repeat() {
    MAKE_TIMEPOINT(time_point);

    scheduler::DateMatch m;
    TEST_ASSERT(scheduler::parse_date(m, "*-2/7"));
    TEST_ASSERT(scheduler::match(m, time_point));

    for (int mday = 2; mday < 31; mday += 7) {
        TEST_ASSERT_EQUAL(mday, time_point.tm_mday);
        TEST_ASSERT(scheduler::match(m, time_point));
        time_point.tm_mday += 7;
    }
}

void test_weekday_impl() {
    scheduler::WeekdayMatch m;
    m.day.set(1);

    TEST_SCHEDULER_MATCH(m);
}

#define TEST_SCHEDULER_INVALID_WDS(FMT) ([](){\
    scheduler::WeekdayMatch m;\
    TEST_ASSERT_FALSE(scheduler::parse_weekdays(m, FMT));\
    })()

void test_weekday_invalid_parsing() {
    TEST_SCHEDULER_INVALID_WDS("5,4,Monday wat");
    TEST_SCHEDULER_INVALID_WDS("0,1,2,3,");
    TEST_SCHEDULER_INVALID_WDS("5..1,,,..");
    TEST_SCHEDULER_INVALID_WDS(",9..1,");
    TEST_SCHEDULER_INVALID_WDS("1,,1,");
    TEST_SCHEDULER_INVALID_WDS(",,,");
    TEST_SCHEDULER_INVALID_WDS("1,2,3,4,,,,");
    TEST_SCHEDULER_INVALID_WDS("1111,3,3,2,1,,,");
    TEST_SCHEDULER_INVALID_WDS(",,,,Fridayyy,Sunaady");
    TEST_SCHEDULER_INVALID_WDS(",,,,foo,bar");
    TEST_SCHEDULER_INVALID_WDS("...Weds..Frii");
    TEST_SCHEDULER_INVALID_WDS("..Fri..Mon");
    TEST_SCHEDULER_INVALID_WDS(".Tue..Mon");
    TEST_SCHEDULER_INVALID_WDS("...Mon");
    TEST_SCHEDULER_INVALID_WDS("..Mon");
    TEST_SCHEDULER_INVALID_WDS("..");
    TEST_SCHEDULER_INVALID_WDS(".");
    TEST_SCHEDULER_INVALID_WDS("");
}

#define TEST_SCHEDULER_VALID_WDS(FMT) ([](){\
    scheduler::WeekdayMatch m;\
    TEST_ASSERT(scheduler::parse_weekdays(m, FMT));\
    })()

void test_weekday_parsing() {
    TEST_SCHEDULER_VALID_WDS("1,2,3,4");
    TEST_SCHEDULER_VALID_WDS("5..7");
    TEST_SCHEDULER_VALID_WDS("6..5");
    TEST_SCHEDULER_VALID_WDS("Monday");
    TEST_SCHEDULER_VALID_WDS("Friday,Monday");
    TEST_SCHEDULER_VALID_WDS("1,Saturday");
    TEST_SCHEDULER_VALID_WDS("Sun,5,Friday,1");
}

#define TEST_SCHEDULER_WEEKDAY(FMT) ([](){\
    MAKE_TIMEPOINT(time_point);\
    scheduler::WeekdayMatch m;\
    TEST_ASSERT_EQUAL(datetime::Monday.c_value(), time_point.tm_wday);\
    TEST_ASSERT(scheduler::parse_weekdays(m, FMT));\
    TEST_ASSERT(scheduler::match(m, time_point));})()

void test_weekday() {
    TEST_SCHEDULER_WEEKDAY("Wed..Tue");
    TEST_SCHEDULER_WEEKDAY("Tuesday,Mon,Saturday");
    TEST_SCHEDULER_WEEKDAY("Tue,Fri,Mon");
    TEST_SCHEDULER_WEEKDAY("Monday");
    TEST_SCHEDULER_WEEKDAY("1");
    TEST_SCHEDULER_WEEKDAY("5..3");
    TEST_SCHEDULER_WEEKDAY("1..4");
    TEST_SCHEDULER_WEEKDAY("5,6,7,1");
}

#define TEST_SCHEDULER_WEEKDAY_OFFSET(OFFSET, FMT) ([](){\
    MAKE_TIMEPOINT(time_point);\
    time_point.tm_wday += OFFSET;\
    TEST_ASSERT_GREATER_OR_EQUAL(0, time_point.tm_wday);\
    TEST_ASSERT_LESS_THAN(7, time_point.tm_wday);\
    scheduler::WeekdayMatch m;\
    TEST_ASSERT(scheduler::parse_weekdays(m, FMT));\
    TEST_ASSERT(scheduler::match(m, time_point));})()

void test_weekday_offset() {
    TEST_SCHEDULER_WEEKDAY_OFFSET(-1, "Sunday");
    TEST_SCHEDULER_WEEKDAY_OFFSET(0, "mOnday");
    TEST_SCHEDULER_WEEKDAY_OFFSET(1, "tuEsday");
    TEST_SCHEDULER_WEEKDAY_OFFSET(2, "wedNesday");
    TEST_SCHEDULER_WEEKDAY_OFFSET(3, "thurSday");
    TEST_SCHEDULER_WEEKDAY_OFFSET(4, "fridaY");
    TEST_SCHEDULER_WEEKDAY_OFFSET(5, "saturdAy");
}

void test_time_impl() {
    scheduler::TimeMatch m;
    m.hour.set(22); // -7
    m.minute.set(4);

    TEST_SCHEDULER_MATCH(m);
}

#define TEST_SCHEDULER_INVALID_TIME(FMT) ([](){\
    scheduler::TimeMatch m;\
    TEST_ASSERT_FALSE(scheduler::parse_time(m, FMT));\
    })()

void test_time_invalid_parsing() {
    TEST_SCHEDULER_INVALID_TIME("...");
    TEST_SCHEDULER_INVALID_TIME("");
    TEST_SCHEDULER_INVALID_TIME(":::");
    TEST_SCHEDULER_INVALID_TIME(":");
    TEST_SCHEDULER_INVALID_TIME(":01");
    TEST_SCHEDULER_INVALID_TIME("02:");
    TEST_SCHEDULER_INVALID_TIME(":1:2:3");
    TEST_SCHEDULER_INVALID_TIME(":1:2:3:");
    TEST_SCHEDULER_INVALID_TIME("1:233:");
    TEST_SCHEDULER_INVALID_TIME("1:2:::");
    TEST_SCHEDULER_INVALID_TIME("0/5/2:2");
    TEST_SCHEDULER_INVALID_TIME("///0/5/2:2");
    TEST_SCHEDULER_INVALID_TIME("...///355:2");
    TEST_SCHEDULER_INVALID_TIME("33:55");
    TEST_SCHEDULER_INVALID_TIME("UTC");
    TEST_SCHEDULER_INVALID_TIME("UTC foo");
    TEST_SCHEDULER_INVALID_TIME("UTC 1:2");
    TEST_SCHEDULER_INVALID_TIME("1:2 UTC");
    TEST_SCHEDULER_INVALID_TIME("foo UTC");
    TEST_SCHEDULER_INVALID_TIME("UT 1:5 C");
    TEST_SCHEDULER_INVALID_TIME(" U T 1:5 C");
}

#define TEST_SCHEDULER_VALID_TIME(FMT) ([](){\
    scheduler::TimeMatch m;\
    TEST_ASSERT(scheduler::parse_time(m, FMT));\
    })()

void test_time_parsing() {
    TEST_SCHEDULER_VALID_TIME("1:2");
    TEST_SCHEDULER_VALID_TIME("0:00");
    TEST_SCHEDULER_VALID_TIME("00:0");
    TEST_SCHEDULER_VALID_TIME("11:22");
    TEST_SCHEDULER_VALID_TIME("*:55");
    TEST_SCHEDULER_VALID_TIME("13:*");
    TEST_SCHEDULER_VALID_TIME("*:*");
}

#define TEST_SCHEDULER_VALID_TIME_REPEAT(FMT) ([](){\
    MAKE_TIMEPOINT(time_point);\
    scheduler::TimeMatch m;\
    TEST_ASSERT(scheduler::parse_time(m, FMT));\
    TEST_ASSERT(scheduler::match(m, time_point));\
    })()

void test_time_repeat() {
    TEST_SCHEDULER_VALID_TIME_REPEAT("20..23:*");
    TEST_SCHEDULER_VALID_TIME_REPEAT("*:4");
    TEST_SCHEDULER_VALID_TIME_REPEAT("*:1/1");
    TEST_SCHEDULER_VALID_TIME_REPEAT("21,22,23:2/1");
    TEST_SCHEDULER_VALID_TIME_REPEAT("*:2/2");
    TEST_SCHEDULER_VALID_TIME_REPEAT("22,1:3/1");
    TEST_SCHEDULER_VALID_TIME_REPEAT("*:4/5");
    TEST_SCHEDULER_VALID_TIME_REPEAT("22:55..05");
}

void test_keyword_impl() {
    scheduler::TimeMatch m;
    m.flags = scheduler::FlagUtc;

    TEST_ASSERT(scheduler::want_utc(m));
    TEST_ASSERT_FALSE(scheduler::want_sunrise(m));
    TEST_ASSERT_FALSE(scheduler::want_sunset(m));

    m.flags |= scheduler::FlagSunrise;

    TEST_ASSERT(scheduler::want_utc(m));
    TEST_ASSERT(scheduler::want_sunrise(m));
    TEST_ASSERT_FALSE(scheduler::want_sunset(m));

    m.flags |= scheduler::FlagSunset;
    TEST_ASSERT(scheduler::want_utc(m));
    TEST_ASSERT(scheduler::want_sunrise(m));
    TEST_ASSERT(scheduler::want_sunset(m));
}

#define TEST_SCHEDULER_VALID_KEYWORD(FMT, MASK) ([](){\
    scheduler::TimeMatch m;\
    TEST_ASSERT(scheduler::parse_time_keyword(m, FMT));\
    TEST_ASSERT_BITS(MASK, MASK, m.flags);\
    })()

void test_keyword_parsing() {
    TEST_SCHEDULER_VALID_KEYWORD("UTC", scheduler::FlagUtc);
    TEST_SCHEDULER_VALID_KEYWORD("utc", scheduler::FlagUtc);
    TEST_SCHEDULER_VALID_KEYWORD("Utc", scheduler::FlagUtc);
    TEST_SCHEDULER_VALID_KEYWORD("uTc", scheduler::FlagUtc);
    TEST_SCHEDULER_VALID_KEYWORD("utC", scheduler::FlagUtc);
    TEST_SCHEDULER_VALID_KEYWORD("Sunrise", scheduler::FlagSunrise);
    TEST_SCHEDULER_VALID_KEYWORD("Sunset", scheduler::FlagSunset);
}

#define MAKE_RESTORE_CONTEXT(CTX, SCHEDULE)\
    const auto __reference_ctx = datetime::make_context(ReferenceTimestamp);\
    Schedule SCHEDULE;\
    SCHEDULE.weekdays.day[datetime::Monday.c_value()] = true;\
    SCHEDULE.date.day[2] = true;\
    SCHEDULE.time.hour[15] = true;\
    SCHEDULE.time.minute[4] = true;\
    SCHEDULE.time.flags = FlagUtc;\
    SCHEDULE.ok = true;\
    auto CTX = restore::Context(__reference_ctx)

void test_restore_today() {
    MAKE_RESTORE_CONTEXT(ctx, schedule);

    const auto original_date = schedule.date;

    schedule.date = DateMatch{};
    schedule.date.day[15] = true;
    schedule.date.month[0] = true;
    schedule.date.year = 2006;

    TEST_ASSERT_FALSE(handle_today(ctx, 0, schedule));
    TEST_ASSERT_EQUAL(1, ctx.pending.size());
    TEST_ASSERT_EQUAL(0, ctx.results.size());

    ctx.results.clear();
    ctx.pending.clear();

    schedule.date = original_date;

    TEST_ASSERT_TRUE(handle_today(ctx, 1, schedule));
    TEST_ASSERT_EQUAL(0, ctx.pending.size());
    TEST_ASSERT_EQUAL(1, ctx.results.size());
    TEST_ASSERT_EQUAL(1, ctx.results[0].index);
    TEST_ASSERT_EQUAL(
        datetime::Minutes(datetime::Hours(-7)).count(),
        ctx.results[0].offset.count());

    ctx.results.clear();
    ctx.pending.clear();

    schedule.time.hour.reset();
    schedule.time.hour.set(ctx.current.utc.tm_hour);

    schedule.time.minute.reset();
    schedule.time.minute.set(ctx.current.utc.tm_min - 4);

    TEST_ASSERT_TRUE(handle_today(ctx, 2, schedule));
    TEST_ASSERT_EQUAL(0, ctx.pending.size());
    TEST_ASSERT_EQUAL(1, ctx.results.size());
    TEST_ASSERT_EQUAL(2, ctx.results[0].index);
    TEST_ASSERT_EQUAL(
        datetime::Minutes(-4).count(),
        ctx.results[0].offset.count());

    ctx.results.clear();
    ctx.pending.clear();

    schedule.time.hour.reset();
    schedule.time.hour.set(ctx.current.utc.tm_hour - 1);

    schedule.time.minute.reset();
    schedule.time.minute.set(ctx.current.utc.tm_min + 30);

    TEST_ASSERT_TRUE(handle_today(ctx, 3, schedule));
    TEST_ASSERT_EQUAL(0, ctx.pending.size());
    TEST_ASSERT_EQUAL(1, ctx.results.size());
    TEST_ASSERT_EQUAL(3, ctx.results[0].index);
    TEST_ASSERT_EQUAL(
        datetime::Minutes(-30).count(),
        ctx.results[0].offset.count());
}

void test_restore_delta_future() {
    MAKE_RESTORE_CONTEXT(ctx, schedule);

    const auto pending = Pending{.index = 1, .schedule = schedule};

    ctx.next_delta(datetime::Days{ 25 });
    TEST_ASSERT_FALSE(handle_pending(ctx, pending));
    TEST_ASSERT_EQUAL(0, ctx.results.size());

    ctx.next_delta(datetime::Days{ -15 });
    TEST_ASSERT_FALSE(handle_pending(ctx, pending));
    TEST_ASSERT_EQUAL(0, ctx.results.size());

    ctx.next_delta(datetime::Days{ 0 });
    TEST_ASSERT_FALSE(handle_pending(ctx, pending));
    TEST_ASSERT_EQUAL(0, ctx.results.size());

    ctx.next();
    TEST_ASSERT_FALSE(handle_pending(ctx, pending));
    TEST_ASSERT_EQUAL(0, ctx.results.size());
}

void test_restore_delta_past() {
    struct Expected {
        size_t index;
        datetime::Days delta;
        int day;
        datetime::Weekday weekday;
        datetime::Hours hours;
    };

    constexpr std::array tests{
        Expected{
            .index = 0,
            .delta = datetime::Days{ -1 },
            .day = 1,
            .weekday = datetime::Sunday,
            .hours = datetime::Hours{ -31 }},
        Expected{
            .index = 1,
            .delta = datetime::Days{ -1 },
            .day = 31,
            .weekday = datetime::Saturday,
            .hours = datetime::Hours{ -55 }},
        Expected{
            .index = 2,
            .delta = datetime::Days{ -1 },
            .day = 30,
            .weekday = datetime::Friday,
            .hours = datetime::Hours{ -79 }},
        Expected{
            .index = 3,
            .delta = datetime::Days{ -2 },
            .day = 28,
            .weekday = datetime::Wednesday,
            .hours = datetime::Hours{ -127 }},
    };


    MAKE_RESTORE_CONTEXT(ctx, schedule);

    auto schedule_day_weekday = [&](auto& out, const auto& test) {
        out.date = scheduler::DateMatch{};
        out.date.day[test.day] = true;

        out.weekdays = scheduler::WeekdayMatch{};
        out.weekdays.day[test.weekday.c_value()] = true;
    };

    for (const auto& test : tests) {
        schedule_day_weekday(schedule, test);

        TEST_ASSERT_FALSE(
            handle_today(ctx, test.index, schedule));
    }

    TEST_ASSERT_EQUAL(0, ctx.results.size());
    TEST_ASSERT_EQUAL(tests.size(), ctx.pending.size());

    for (const auto& test : tests) {
        schedule_day_weekday(schedule, test);

        ctx.next_delta(test.delta);

        for (auto& pending : ctx.pending) {
            handle_pending(ctx, pending);
        }
    }

    TEST_ASSERT_EQUAL(tests.size(), ctx.results.size());
    TEST_ASSERT_EQUAL(tests.size(), ctx.pending.size());

    for (auto& result : ctx.results) {
        TEST_ASSERT_EQUAL(tests[result.index].index, result.index);
        TEST_ASSERT_EQUAL(
            datetime::Minutes(tests[result.index].hours).count(),
            result.offset.count());
    }
}

void test_restore_dst_sdt() {
    WithTimezone _(":US/Pacific");

    // at 2006-10-29T02:33:04-08:00 TZ='US/Pacific'
    constexpr auto Timestamp = time_t{ 1162117984 };

    auto ctx = datetime::make_context(Timestamp);

    Schedule sch;
    sch.ok = true;

    // to 2006-10-29T01:32:04-08:00 TZ='US/Pacific'
    sch.time.hour[1] = true;
    sch.time.minute[32] = true;

#define POP_BACK(OUT, RESULTS)\
    TEST_ASSERT_EQUAL(1, (RESULTS).size());\
    OUT = (RESULTS).back();\
    (RESULTS).pop_back()

    Offset result;

    auto restore = std::make_unique<restore::Context>(ctx);
    TEST_ASSERT(handle_today(*restore, 321, sch));

    POP_BACK(result, restore->results);
    TEST_ASSERT_EQUAL(321, result.index);
    TEST_ASSERT_EQUAL(-61, result.offset.count());

    // to 2006-10-29T00:55:04-07:00 TZ='US/Pacific'
    sch.time = TimeMatch{};
    sch.time.hour[0] = true;
    sch.time.minute[55] = true;

    TEST_ASSERT(handle_today(*restore, 654, sch));

    POP_BACK(result, restore->results);
    TEST_ASSERT_EQUAL(654, result.index);
    TEST_ASSERT_EQUAL(-158, result.offset.count());

    // at 2006-10-29T01:33:04-08:00 TZ='US/Pacific'
    ctx = datetime::make_context(Timestamp - OneHour.count());
    restore = std::make_unique<restore::Context>(ctx);

    // to 2006-10-29T00:11:04-07:00 TZ='US/Pacific'
    sch.time = TimeMatch{};
    sch.time.hour[0] = true;
    sch.time.minute[11] = true;

    TEST_ASSERT(handle_today(*restore, 987, sch));

    POP_BACK(result, restore->results);
    TEST_ASSERT_EQUAL(987, result.index);
    TEST_ASSERT_EQUAL(-142, result.offset.count());

    // to 2006-10-29T01:30:04-08:00 TZ='US/Pacific'
    sch.time = TimeMatch{};
    sch.time.hour[1] = true;
    sch.time.minute[30] = true;

    TEST_ASSERT(handle_today(*restore, 567, sch));

    POP_BACK(result, restore->results);
    TEST_ASSERT_EQUAL(567, result.index);
    TEST_ASSERT_EQUAL(-3, result.offset.count());

    // to 2006-10-29T01:44:04-07:00 TZ='US/Pacific'
    sch.time = TimeMatch{};
    sch.time.hour[1] = true;
    sch.time.minute[44] = true;

    TEST_ASSERT(handle_today(*restore, 678, sch));

    POP_BACK(result, restore->results);
    TEST_ASSERT_EQUAL(678, result.index);
    TEST_ASSERT_EQUAL(-49, result.offset.count());

    // to 2006-10-28T21:00:04-07:00 TZ='US/Pacific'
    sch.time = TimeMatch{};
    sch.time.hour[21] = true;
    sch.time.minute[0] = true;

    TEST_ASSERT_FALSE(handle_today(*restore, 912, sch));
    TEST_ASSERT(restore->next());

    TEST_ASSERT(handle_pending(*restore, restore->pending.back()));

    POP_BACK(result, restore->results);
    TEST_ASSERT_EQUAL(912, result.index);
    TEST_ASSERT_EQUAL(-333, result.offset.count());

#undef POP_BACK
}

void test_restore_sdt_dst() {
    WithTimezone _(":US/Pacific");

    // at 2006-04-02T04:33:04-07:00 TZ='US/Pacific'
    constexpr auto Timestamp = time_t{ 1143977584 };

    auto ctx = datetime::make_context(Timestamp);

    Schedule sch;
    sch.ok = true;

    // to invalid 2006-04-02T02:54:04-08:00 TZ='US/Pacific'
    sch.time.hour[2] = true;
    sch.time.minute[54] = true;

    // to 2006-04-02T01:54:04-08:00 TZ='US/Pacific'
    sch.time.hour[1] = true;

#define POP_BACK(OUT, RESULTS)\
    TEST_ASSERT_EQUAL(1, (RESULTS).size());\
    OUT = (RESULTS).back();\
    (RESULTS).pop_back()

    Offset result;

    auto restore = std::make_unique<restore::Context>(ctx);
    TEST_ASSERT(handle_today(*restore, 579, sch));

    POP_BACK(result, restore->results);
    TEST_ASSERT_EQUAL(579, result.index);
    TEST_ASSERT_EQUAL(-99, result.offset.count());

    // to 2006-04-02T00:05:04-08:00 TZ='US/Pacific'
    sch.time = TimeMatch{};
    sch.time.hour[0] = true;
    sch.time.minute[5] = true;

    TEST_ASSERT(handle_today(*restore, 135, sch));

    POP_BACK(result, restore->results);
    TEST_ASSERT_EQUAL(135, result.index);
    TEST_ASSERT_EQUAL(-208, result.offset.count());

    // to 2006-04-01T19:25:04-08:00 TZ='US/Pacific'
    sch.time = TimeMatch{};
    sch.time.hour[19] = true;
    sch.time.minute[25] = true;

    TEST_ASSERT_FALSE(handle_today(*restore, 258, sch));
    TEST_ASSERT_EQUAL(0, restore->results.size());

    TEST_ASSERT_EQUAL(1, restore->pending.size());
    TEST_ASSERT_EQUAL(258, restore->pending.back().index);

    TEST_ASSERT(restore->next());
    TEST_ASSERT(handle_pending(*restore, restore->pending.back()));

    POP_BACK(result, restore->results);
    TEST_ASSERT_EQUAL(258, result.index);
    TEST_ASSERT_EQUAL(-488, result.offset.count());

#undef POP_BACK
}

void test_event_impl() {
    static_assert(std::is_same_v<datetime::Clock::duration, datetime::Seconds>, "");
    static_assert(std::is_same_v<event::time_point, datetime::Clock::time_point>, "");
    const auto now = datetime::Clock::now();
    TEST_ASSERT(event::is_valid(now));

    const auto ctx = datetime::make_context(now);
    const auto time_point =
        event::make_time_point(ctx);
    TEST_ASSERT(event::is_valid(time_point));

    const auto minutes = to_minutes(time_point);
    static_assert(std::is_same_v<decltype(minutes), const datetime::Minutes>, "");

    const auto seconds = (time_point - minutes).time_since_epoch();
    static_assert(std::is_same_v<decltype(seconds), const datetime::Seconds>, "");

    TEST_ASSERT_EQUAL(0, event::difference((time_point - seconds), time_point).count());

    const auto one = time_point + datetime::Minutes(55);
    TEST_ASSERT_EQUAL(55, event::difference(one, time_point).count());

    const auto two = time_point - datetime::Minutes(55);
    TEST_ASSERT_EQUAL(-55, event::difference(two, time_point).count());
}

void test_event_parsing() {
    auto result = parse_relative("5 after \"foobar\"");
    TEST_ASSERT_EQUAL(relative::Order::After, result.order);
    TEST_ASSERT_EQUAL(relative::Type::Named, result.type);
    TEST_ASSERT_EQUAL_STRING("foobar", result.name.c_str());
    TEST_ASSERT_EQUAL(
        datetime::Minutes(5).count(),
        result.offset.count());

    result = parse_relative("33m before \"bar\"");
    TEST_ASSERT_EQUAL(relative::Order::Before, result.order);
    TEST_ASSERT_EQUAL(relative::Type::Named, result.type);
    TEST_ASSERT_EQUAL_STRING("bar", result.name.c_str());
    TEST_ASSERT_EQUAL(
        datetime::Minutes(33).count(),
        result.offset.count());

    result = parse_relative("30m before sunrise");
    TEST_ASSERT_EQUAL(relative::Order::Before, result.order);
    TEST_ASSERT_EQUAL(relative::Type::Sunrise, result.type);
    TEST_ASSERT_EQUAL(
        datetime::Minutes(30).count(),
        result.offset.count());

    result = parse_relative("1h15m after sunset");
    TEST_ASSERT_EQUAL(relative::Order::After, result.order);
    TEST_ASSERT_EQUAL(relative::Type::Sunset, result.type);
    TEST_ASSERT_EQUAL(
        datetime::Minutes(75).count(),
        result.offset.count());

    result = parse_relative("after sunset");
    TEST_ASSERT_EQUAL(relative::Order::After, result.order);
    TEST_ASSERT_EQUAL(relative::Type::Sunset, result.type);
    TEST_ASSERT_EQUAL(1, result.offset.count());

    result = parse_relative("before sunrise");
    TEST_ASSERT_EQUAL(relative::Order::Before, result.order);
    TEST_ASSERT_EQUAL(relative::Type::Sunrise, result.type);
    TEST_ASSERT_EQUAL(1, result.offset.count());

    result = parse_relative("10m before calendar#123");
    TEST_ASSERT_EQUAL(relative::Order::Before, result.order);
    TEST_ASSERT_EQUAL(relative::Type::Calendar, result.type);
    TEST_ASSERT_EQUAL(
        datetime::Minutes(10).count(),
        result.offset.count());
    TEST_ASSERT_EQUAL(123, result.data);

    result = parse_relative("0m after calendar#46");
    TEST_ASSERT_EQUAL(relative::Order::After, result.order);
    TEST_ASSERT_EQUAL(relative::Type::Calendar, result.type);
    TEST_ASSERT_EQUAL(
        datetime::Minutes::zero().count(),
        result.offset.count());
    TEST_ASSERT_EQUAL(46, result.data);

    result = parse_relative("0h before \"after\"");
    TEST_ASSERT_EQUAL(relative::Order::Before, result.order);
    TEST_ASSERT_EQUAL(relative::Type::Named, result.type);
    TEST_ASSERT_EQUAL(
        datetime::Minutes::zero().count(),
        result.offset.count());
    TEST_ASSERT_EQUAL_STRING("after", result.name.c_str());

    result = parse_relative("0 after \"before\"");
    TEST_ASSERT_EQUAL(relative::Order::After, result.order);
    TEST_ASSERT_EQUAL(relative::Type::Named, result.type);
    TEST_ASSERT_EQUAL(
        datetime::Minutes::zero().count(),
        result.offset.count());
    TEST_ASSERT_EQUAL_STRING("before", result.name.c_str());

    result = parse_relative("after calendar#543");
    TEST_ASSERT_EQUAL(relative::Type::None, result.type);

    result = parse_relative("after");
    TEST_ASSERT_EQUAL(relative::Type::None, result.type);

    result = parse_relative("before");
    TEST_ASSERT_EQUAL(relative::Type::None, result.type);

    result = parse_relative("11 befre boot");
    TEST_ASSERT_EQUAL(relative::Type::None, result.type);

    result = parse_relative("55 afer sunrise");
    TEST_ASSERT_EQUAL(relative::Type::None, result.type);
}

#define TEST_TIME_POINT_BOUNDARIES(X)\
    TEST_ASSERT(((X).tm_hour >= 0) && ((X).tm_hour < 24));\
    TEST_ASSERT(((X).tm_min >= 0) && ((X).tm_min < 60))

void test_expect_today() {
    auto ctx = datetime::make_context(ReferenceTimestamp);

    Schedule sch;
    sch.time.hour.set(ctx.utc.tm_hour - 1);
    sch.time.minute.set(ctx.utc.tm_min - 1);
    sch.time.flags = scheduler::FlagUtc;

    auto expect = expect::Context{ ctx };

    TEST_ASSERT_FALSE(handle_today(expect, 123, sch));

    constexpr auto one_h = datetime::Hours(1);
    sch.time = TimeMatch{};
    sch.time.flags = scheduler::FlagUtc;

    sch.time.hour.set(
        ctx.utc.tm_hour + one_h.count());

    constexpr auto twenty_six_m = datetime::Minutes(26);
    sch.time.minute.set(
        ctx.utc.tm_min + twenty_six_m.count());

    TEST_ASSERT(handle_today(expect, 456, sch));
    TEST_ASSERT_EQUAL(1, expect.results.size());
    TEST_ASSERT_EQUAL(
        (twenty_six_m + one_h).count(),
        expect.results.back().offset.count());

    auto result = ReferenceTimestamp
        + datetime::Seconds(expect.results.back().offset).count();

    expect.results.clear();
    expect.pending.clear();

    tm out{};
    gmtime_r(&result, &out);

    TEST_ASSERT_EQUAL(ctx.utc.tm_hour + one_h.count(), out.tm_hour);
    TEST_ASSERT_EQUAL(ctx.utc.tm_min + twenty_six_m.count(), out.tm_min);

    sch.time = TimeMatch{};
    sch.time.flags = scheduler::FlagUtc;

    constexpr auto nine_h = datetime::Hours(9);
    sch.time.hour.set(ctx.utc.tm_hour);

    constexpr auto thirty_m = datetime::Minutes(30);
    sch.time.minute.set(ctx.utc.tm_min);

    const auto next = datetime::make_time_point(
        datetime::Seconds(ReferenceTimestamp) - nine_h + thirty_m);
    ctx = datetime::make_context(next);

    TEST_ASSERT(handle_today(expect, 791, sch));
    TEST_ASSERT_EQUAL(1, expect.results.size());
    TEST_ASSERT_EQUAL(
        (nine_h - thirty_m).count(),
        expect.results.back().offset.count());

    result = (datetime::Seconds(ctx.timestamp)
        + datetime::Seconds(expect.results.back().offset)).count();
    gmtime_r(&result, &out);

    TEST_ASSERT_EQUAL(ctx.utc.tm_hour + nine_h.count(), out.tm_hour);
    TEST_ASSERT_EQUAL(ctx.utc.tm_min - thirty_m.count(), out.tm_min);
}

void test_expect_delta_future() {
    const auto reference = datetime::make_context(ReferenceTimestamp);

    auto ctx = expect::Context{ reference };

    Schedule schedule;
    schedule.time.flags = scheduler::FlagUtc;
    schedule.ok = true;

    constexpr auto one_d = datetime::Days(1);
    schedule.date.day[reference.utc.tm_mday + one_d.count()] = true;

    constexpr auto thirty_one_m = datetime::Minutes(31);
    schedule.time.hour[reference.utc.tm_hour] = true;
    schedule.time.minute[reference.utc.tm_min + thirty_one_m.count()] = true;
    schedule.time.flags = FlagUtc;

    TEST_ASSERT_FALSE(handle_today(ctx, 123, schedule));
    TEST_ASSERT_EQUAL(0, ctx.results.size());
    TEST_ASSERT_EQUAL(1, ctx.pending.size());
    TEST_ASSERT_EQUAL(123, ctx.pending[0].index);

    TEST_ASSERT(ctx.next());

    TEST_ASSERT(handle_pending(ctx, ctx.pending[0]));
    TEST_ASSERT_EQUAL(1, ctx.results.size());
    TEST_ASSERT_EQUAL(123, ctx.results[0].index);

    TEST_ASSERT_EQUAL(
        (one_d + thirty_one_m).count(),
        ctx.results[0].offset.count());
}

void test_expect_sdt_dst() {
    WithTimezone _(":US/Pacific");

    // at 2006-04-02T01:33:04-08:00 TZ='US/Pacific'
    constexpr auto Timestamp = time_t{ 1143970384 };

    auto ctx = datetime::make_context(Timestamp);

    // to invalid 2006-04-02T02:23:04-08:00 TZ='US/Pacific'
    Schedule sch;
    sch.ok = true;
    sch.time.hour[2] = true;
    sch.time.minute[23] = true;

#define POP_BACK(OUT, RESULTS)\
    TEST_ASSERT_EQUAL(1, (RESULTS).size());\
    OUT = (RESULTS).back();\
    (RESULTS).pop_back()

    Offset result;

    auto expect = std::make_unique<expect::Context>(ctx);
    TEST_ASSERT_FALSE(handle_today(*expect, 123, sch));
    TEST_ASSERT_EQUAL(0, expect->results.size());
    TEST_ASSERT_EQUAL(1, expect->pending.size());

    TEST_ASSERT(expect->next_delta(datetime::Days{ 1 }));
    TEST_ASSERT(handle_pending(*expect, expect->pending[0]));

    POP_BACK(result, expect->results);
    TEST_ASSERT_EQUAL(123, result.index);
    TEST_ASSERT_EQUAL(
        (datetime::Minutes(datetime::Days(1)) - datetime::Minutes(10)).count(),
        result.offset.count());

    // back to original state
    expect = std::make_unique<expect::Context>(ctx);

    // to 2006-04-02T04:56:04-07:00 TZ='US/Pacific'
    sch.time = TimeMatch{};
    sch.time.hour[4] = true;
    sch.time.minute[56] = true;

    TEST_ASSERT(handle_today(*expect, 246, sch));

    POP_BACK(result, expect->results);
    TEST_ASSERT_EQUAL(246, result.index);
    TEST_ASSERT_EQUAL(143, result.offset.count());

    sch.time = TimeMatch{};

    // to invalid 2006-04-02T02:59:04-08:00 TZ='US/Pacific'
    sch.time.hour[2] = true;
    sch.time.minute[59] = true;

    // to 2006-04-02T03:33:04-08:00 TZ='US/Pacific'
    // should exclude 02:33 & 02:59
    sch.time.hour[3] = true;
    sch.time.minute[33] = true;

    TEST_ASSERT(handle_today(*expect, 357, sch));

    POP_BACK(result, expect->results);
    TEST_ASSERT_EQUAL(357, result.index);
    TEST_ASSERT_EQUAL(60, result.offset.count());

#undef POP_BACK
}

void test_expect_dst_sdt() {
    WithTimezone _(":US/Pacific");

    // at 2006-10-29T00:33:04-07:00 TZ='US/Pacific'
    constexpr auto Timestamp = time_t{ 1162107184 };

    auto ctx = datetime::make_context(Timestamp);

    // to 2006-10-29T01:22:04-07:00 TZ='US/Pacific'
    Schedule sch;
    sch.time.hour[1] = true;
    sch.time.minute[22] = true;

#define POP_BACK(OUT, RESULTS)\
    TEST_ASSERT_EQUAL(1, (RESULTS).size());\
    OUT = (RESULTS).back();\
    (RESULTS).pop_back()

    Offset result;

    auto expect = std::make_unique<expect::Context>(ctx);
    TEST_ASSERT(handle_today(*expect, 123, sch));

    POP_BACK(result, expect->results);
    TEST_ASSERT_EQUAL(123, result.index);
    TEST_ASSERT_EQUAL(49, result.offset.count());

    // to 2006-10-29T01:00:04-07:00 TZ='US/Pacific'
    sch.time = TimeMatch{};
    sch.time.hour[1] = true;
    sch.time.minute[0] = true;

    TEST_ASSERT(handle_today(*expect, 456, sch));

    POP_BACK(result, expect->results);
    TEST_ASSERT_EQUAL(456, result.index);
    TEST_ASSERT_EQUAL(27, result.offset.count());

    // at 2006-10-29T01:33:04-07:00 TZ='US/Pacific'
    ctx = datetime::make_context(Timestamp + OneHour.count());
    expect = std::make_unique<expect::Context>(ctx);

    // to 2006-10-29T01:49:04-07:00 TZ='US/Pacific'
    sch.time = TimeMatch{};
    sch.time.hour[1] = true;
    sch.time.minute[49] = true;

    TEST_ASSERT(handle_today(*expect, 789, sch));

    POP_BACK(result, expect->results);
    TEST_ASSERT_EQUAL(789, result.index);
    TEST_ASSERT_EQUAL(16, result.offset.count());

    // to 2006-10-29T01:19:04-08:00 TZ='US/Pacific'
    sch.time = TimeMatch{};
    sch.time.hour[1] = true;
    sch.time.minute[19] = true;

    TEST_ASSERT(handle_today(*expect, 111, sch));

    POP_BACK(result, expect->results);
    TEST_ASSERT_EQUAL(111, result.index);
    TEST_ASSERT_EQUAL(46, result.offset.count());

    // at 2006-10-29T01:33:04-08:00 TZ='US/Pacific'
    ctx = datetime::make_context(Timestamp + (OneHour + OneHour).count());
    expect = std::make_unique<expect::Context>(ctx);

    // to 2006-10-29T01:52:04-08:00 TZ='US/Pacific'
    sch.time = TimeMatch{};
    sch.time.hour[1] = true;
    sch.time.minute[52] = true;

    TEST_ASSERT(handle_today(*expect, 222, sch));

    POP_BACK(result, expect->results);
    TEST_ASSERT_EQUAL(222, result.index);
    TEST_ASSERT_EQUAL(19, result.offset.count());

    // to 2006-10-29T02:21:04-08:00 TZ='US/Pacific'
    sch.time = TimeMatch{};
    sch.time.hour[2] = true;
    sch.time.minute[21] = true;

    TEST_ASSERT(handle_today(*expect, 333, sch));

    POP_BACK(result, expect->results);
    TEST_ASSERT_EQUAL(333, result.index);
    TEST_ASSERT_EQUAL(48, result.offset.count());

#undef POP_BACK
}

void test_schedule_invalid_parsing() {
    TEST_ASSERT_FALSE(parse_schedule("UTC 12:00").ok);
    TEST_ASSERT_FALSE(parse_schedule("FOO 12:00").ok);
    TEST_ASSERT_FALSE(parse_schedule("FOO 12:00").ok);
    TEST_ASSERT_FALSE(parse_schedule("13:00 14:00 12:00").ok);
    TEST_ASSERT_FALSE(parse_schedule("Monday Tuesday 14:00 12:00").ok);
    TEST_ASSERT_FALSE(parse_schedule("").ok);
    TEST_ASSERT_FALSE(parse_schedule("SUNRISE UTC").ok);
    TEST_ASSERT_FALSE(parse_schedule("06:05 SUNRISE").ok);
    TEST_ASSERT_FALSE(parse_schedule("SUNSET 13:55").ok);
    TEST_ASSERT_FALSE(parse_schedule("2024-05-23 2024-05-24 2024-05-25").ok);
    TEST_ASSERT_FALSE(parse_schedule("KEYWORD UTC").ok);
    TEST_ASSERT_FALSE(parse_schedule("UTC KEYWORD").ok);
}

#define TEST_SCHEDULE_MATCH_DATE(TIME_POINT, SCHEDULE)\
    TEST_ASSERT(scheduler::match((SCHEDULE).time, (TIME_POINT)))

#define TEST_SCHEDULE_MATCH_WEEKDAYS(TIME_POINT, SCHEDULE)\
    TEST_ASSERT(scheduler::match((SCHEDULE).weekdays, (TIME_POINT)))

#define TEST_SCHEDULE_MATCH_TIME(TIME_POINT, SCHEDULE)\
    TEST_ASSERT(scheduler::match((SCHEDULE).time, (TIME_POINT)))

#define TEST_SCHEDULE_MATCH(TIME_POINT, ELEMS) ([&]() {\
    const auto schedule = parse_schedule(ELEMS);\
    TEST_ASSERT(schedule.ok);\
    TEST_SCHEDULE_MATCH_DATE(TIME_POINT, schedule);\
    TEST_SCHEDULE_MATCH_WEEKDAYS(TIME_POINT, schedule);\
    TEST_SCHEDULE_MATCH_TIME(TIME_POINT, schedule);})()

void test_schedule_parsing_keyword() {
    TEST_ASSERT(want_utc(parse_schedule("UTC").time));
    TEST_ASSERT(want_sunrise(parse_schedule("SUNRISE").time));
    TEST_ASSERT(want_sunset(parse_schedule("SUNSET").time));
}

void test_schedule_parsing_time() {
    MAKE_TIMEPOINT(time_point);

    time_point.tm_hour = 12;
    time_point.tm_min = 0;
    TEST_SCHEDULE_MATCH(time_point, "*:00");
    TEST_SCHEDULE_MATCH(time_point, "12:*");
    TEST_SCHEDULE_MATCH(time_point, "12:00");

    time_point.tm_hour = 13;
    time_point.tm_min = 55;
    TEST_SCHEDULE_MATCH(time_point, "13:*");

    time_point.tm_hour = 14;
    time_point.tm_min = 44;
    TEST_SCHEDULE_MATCH(time_point, "14:*");

    time_point.tm_hour = 15;
    time_point.tm_min = 33;
    TEST_SCHEDULE_MATCH(time_point, "15:*");

    time_point.tm_hour = 22;
    time_point.tm_min = 15;
    TEST_SCHEDULE_MATCH(time_point, "*:*");

    time_point.tm_hour = 0;
    time_point.tm_min = 0;
    TEST_SCHEDULE_MATCH(time_point, "2024-01-01");
    TEST_SCHEDULE_MATCH(time_point, "01-01");
    TEST_SCHEDULE_MATCH(time_point, "Monday");
    TEST_SCHEDULE_MATCH(time_point, "1");
    TEST_SCHEDULE_MATCH(time_point, "00:00");
    TEST_SCHEDULE_MATCH(time_point, "UTC");
}

void test_schedule_parsing_time_range() {
    MAKE_TIMEPOINT(time_point);

    time_point.tm_hour = 13;
    time_point.tm_min = 0;
    TEST_SCHEDULE_MATCH(time_point, "13,15..17:00");

    time_point.tm_hour = 15;
    TEST_SCHEDULE_MATCH(time_point, "13,15..17:00");

    time_point.tm_hour = 16;
    TEST_SCHEDULE_MATCH(time_point, "13,15..17:00");

    time_point.tm_hour = 17;
    TEST_SCHEDULE_MATCH(time_point, "13,15..17:00");

    time_point.tm_hour = 12;
    time_point.tm_min = 5;
    TEST_SCHEDULE_MATCH(time_point, "00..12:5,10,15,20");

    time_point.tm_hour = 11;
    time_point.tm_min = 10;
    TEST_SCHEDULE_MATCH(time_point, "00..12:5,10,15,20");

    time_point.tm_hour = 10;
    time_point.tm_min = 15;
    TEST_SCHEDULE_MATCH(time_point, "00..12:5,10,15,20");

    time_point.tm_hour = 9;
    time_point.tm_min = 20;
    TEST_SCHEDULE_MATCH(time_point, "00..12:5,10,15,20");

    time_point.tm_hour = 18;
    time_point.tm_min = 50;
    TEST_SCHEDULE_MATCH(time_point, "12..23:45..55");

    time_point.tm_hour = 5;
    time_point.tm_min = 44;
    TEST_SCHEDULE_MATCH(time_point, "1/1:*");

    time_point.tm_hour = 11;
    time_point.tm_min = 33;
    TEST_SCHEDULE_MATCH(time_point, "11,12,13:33");

    time_point.tm_hour = 12;
    TEST_SCHEDULE_MATCH(time_point, "11,12,13:33");

    time_point.tm_hour = 13;
    TEST_SCHEDULE_MATCH(time_point, "11,12,13:33");
}

void test_schedule_parsing_date() {
    MAKE_TIMEPOINT(time_point);

    time_point.tm_hour = 6;
    time_point.tm_min = 0;
    time_point.tm_mon = 0;
    time_point.tm_mday = 1;
    TEST_SCHEDULE_MATCH(time_point, "01-01 06:00");

    time_point.tm_hour = 2;
    time_point.tm_min = 0;
    time_point.tm_mon = 4;
    time_point.tm_mday = 10;
    TEST_SCHEDULE_MATCH(time_point, "05-W2 02:00");

    time_point.tm_hour = 18;
    time_point.tm_min = 55;
    time_point.tm_mon = 11;

    time_point.tm_mday = 30;
    TEST_SCHEDULE_MATCH(time_point, "12-L1,2 18:55");

    time_point.tm_mday = 31;
    TEST_SCHEDULE_MATCH(time_point, "12-L1,2 18:55");
}

void test_schedule_parsing_date_range() {
    MAKE_TIMEPOINT(time_point);

    time_point.tm_hour = 5;
    time_point.tm_min = 0;

    time_point.tm_mon = 1;
    time_point.tm_mday = 1;
    TEST_SCHEDULE_MATCH(time_point, "*-1/5 5:00");

    time_point.tm_mon = 2;
    time_point.tm_mday = 6;
    TEST_SCHEDULE_MATCH(time_point, "*-1/5 5:00");

    time_point.tm_mon = 3;
    time_point.tm_mday = 11;
    TEST_SCHEDULE_MATCH(time_point, "*-1/5 5:00");

    time_point.tm_mon = 4;
    time_point.tm_mday = 16;
    TEST_SCHEDULE_MATCH(time_point, "*-1/5 5:00");

    time_point.tm_mon = 5;
    time_point.tm_mday = 21;
    TEST_SCHEDULE_MATCH(time_point, "*-1/5 5:00");

    time_point.tm_mon = 6;
    time_point.tm_mday = 26;
    TEST_SCHEDULE_MATCH(time_point, "*-1/5 5:00");

    time_point.tm_mon = 7;
    time_point.tm_mday = 31;
    TEST_SCHEDULE_MATCH(time_point, "*-1/5 5:00");
}

void test_schedule_parsing_weekdays() {
    MAKE_TIMEPOINT(time_point);

    time_point.tm_hour = 10;
    time_point.tm_min = 0;

    time_point.tm_wday = datetime::Saturday.c_value();
    TEST_SCHEDULE_MATCH(time_point, "Sat,Sun 10:00");

    time_point.tm_wday = datetime::Sunday.c_value();
    TEST_SCHEDULE_MATCH(time_point, "Sat,Sun 10:00");
}

void test_schedule_parsing_weekdays_range() {
    MAKE_TIMEPOINT(time_point);

    time_point.tm_min = 30;
    time_point.tm_wday = 1;

    time_point.tm_hour = 10;
    time_point.tm_wday = 1;
    TEST_SCHEDULE_MATCH(time_point, "Mon,Thu..Sat 10,15,20:30");

    time_point.tm_hour = 15;
    time_point.tm_wday = 4;
    TEST_SCHEDULE_MATCH(time_point, "Mon,Thu..Sat 10,15,20:30");

    time_point.tm_hour = 20;
    time_point.tm_wday = 5;
    TEST_SCHEDULE_MATCH(time_point, "Mon,Thu..Sat 10,15,20:30");

    time_point.tm_hour = 20;
    time_point.tm_wday = 6;
    TEST_SCHEDULE_MATCH(time_point, "Mon,Thu..Sat 10,15,20:30");
}

void test_search_bits() {
    constexpr auto Days = uint32_t{ 0b101010111100100010100110 } ;
    constexpr auto Mask = std::bitset<24>(Days);

    TEST_ASSERT(Mask.test(1));
    TEST_ASSERT(Mask.test(11));
    TEST_ASSERT(Mask.test(14));
    TEST_ASSERT(Mask.test(23));

    const auto past = search::mask_past_hours(Mask, 12);
    TEST_ASSERT_EQUAL(0b000000000000100010100110, past.to_ulong());
    TEST_ASSERT_EQUAL(2, bits::first_set_u32(past.to_ulong()));
    TEST_ASSERT_EQUAL(12, bits::last_set_u32(past.to_ulong()));
    TEST_ASSERT_FALSE(past.test(14));
    TEST_ASSERT_FALSE(past.test(23));

    const auto future = search::mask_future_hours(Mask, 12);
    TEST_ASSERT_EQUAL(0b101010111100000000000000, future.to_ulong());
    TEST_ASSERT_EQUAL(15, bits::first_set_u32(future.to_ulong()));
    TEST_ASSERT_EQUAL(24, bits::last_set_u32(future.to_ulong()));
    TEST_ASSERT_FALSE(future.test(1));
    TEST_ASSERT_FALSE(future.test(11));
}

void test_sun() {
    // 1970-01-01 - prime meridian
    sun::Location location;
    location.latitude = 0.0;
    location.longitude = 0.0;
    location.altitude = 0.0;

    constexpr auto date = datetime::Date{
        .year = 1970, .month = 1, .day = 1,
    };

    const auto ctx = datetime::make_context(0);
    const auto result =
        sun::sunrise_sunset(location, ctx.utc);

    auto expected = datetime::DateHhMmSs{
        .year = date.year, .month = date.month, .day = date.day,
        .hours = 5, .minutes = 59, .seconds = 54};
    TEST_ASSERT(event::is_valid(result.sunrise));
    TEST_ASSERT_EQUAL(
        datetime::to_seconds(expected, true).count(),
        result.sunrise.time_since_epoch().count());

    expected = datetime::DateHhMmSs{
        .year = date.year, .month = date.month, .day = date.day,
        .hours = 18, .minutes = 7, .seconds = 8};
    TEST_ASSERT(event::is_valid(result.sunset));
    TEST_ASSERT_EQUAL(
        datetime::to_seconds(expected, true).count(),
        result.sunset.time_since_epoch().count());
}

void test_datetime_parsing() {
    datetime::DateHhMmSs parsed{};
    bool utc { false };

    TEST_ASSERT(parse_simple_iso8601(parsed, utc, "2006-01-02T22:04:05+00:00"));
    TEST_ASSERT(utc);
    TEST_ASSERT_EQUAL(
        ReferenceTimestamp,
        datetime::to_seconds(parsed, utc).count());

    parsed = datetime::DateHhMmSs{};
    utc = false;

    TEST_ASSERT(parse_simple_iso8601(parsed, utc, "2006-01-02T22:04:05Z"));
    TEST_ASSERT(utc);
    TEST_ASSERT_EQUAL(
        ReferenceTimestamp,
        datetime::to_seconds(parsed, utc).count());

    parsed = datetime::DateHhMmSs{};
    utc = false;

    const auto now = datetime::Clock::now();

    time_t ts;
    ts = now.time_since_epoch().count();

    tm local{};
    localtime_r(&ts, &local);

    char buf[64]{};
    const auto len = strftime(&buf[0], sizeof(buf), "%FT%H:%M:%S", &local);

    TEST_ASSERT_NOT_EQUAL(0, len);
    const auto view = StringView{&buf[0], &buf[0] + len};

    TEST_ASSERT(parse_simple_iso8601(parsed, utc, view));
    TEST_ASSERT_FALSE(utc);

    const auto seconds = datetime::to_seconds(parsed, utc);
    TEST_ASSERT_EQUAL(
        now.time_since_epoch().count(), seconds.count());

    tm c_parsed = parsed.c_value();
    localtime_r(&ts, &c_parsed);

    TEST_ASSERT_EQUAL(local.tm_year, c_parsed.tm_year);
    TEST_ASSERT_EQUAL(local.tm_mon, c_parsed.tm_mon);
    TEST_ASSERT_EQUAL(local.tm_mday, c_parsed.tm_mday);
    TEST_ASSERT_EQUAL(local.tm_hour, c_parsed.tm_hour);
    TEST_ASSERT_EQUAL(local.tm_min, c_parsed.tm_min);
    TEST_ASSERT_EQUAL(local.tm_sec, c_parsed.tm_sec);
}

} // namespace test

} // namespace
} // namespace scheduler
} // namespace espurna

int main(int, char**) {
    UNITY_BEGIN();
    using namespace espurna::scheduler::test;
    RUN_TEST(test_date_day_repeat);
    RUN_TEST(test_date_impl);
    RUN_TEST(test_date_month_glob);
    RUN_TEST(test_date_parsing);
    RUN_TEST(test_date_parsing_invalid);
    RUN_TEST(test_date_week_index);
    RUN_TEST(test_date_year);
    RUN_TEST(test_datetime_parsing);
    RUN_TEST(test_event_impl);
    RUN_TEST(test_event_parsing);
    RUN_TEST(test_expect_delta_future);
    RUN_TEST(test_expect_dst_sdt);
    RUN_TEST(test_expect_sdt_dst);
    RUN_TEST(test_expect_today);
    RUN_TEST(test_keyword_impl);
    RUN_TEST(test_keyword_parsing);
    RUN_TEST(test_restore_delta_future);
    RUN_TEST(test_restore_delta_past);
    RUN_TEST(test_restore_dst_sdt);
    RUN_TEST(test_restore_sdt_dst);
    RUN_TEST(test_restore_today);
    RUN_TEST(test_schedule_invalid_parsing);
    RUN_TEST(test_schedule_parsing_date);
    RUN_TEST(test_schedule_parsing_date_range);
    RUN_TEST(test_schedule_parsing_keyword);
    RUN_TEST(test_schedule_parsing_time);
    RUN_TEST(test_schedule_parsing_time_range);
    RUN_TEST(test_schedule_parsing_weekdays);
    RUN_TEST(test_schedule_parsing_weekdays_range);
    RUN_TEST(test_search_bits);
    RUN_TEST(test_sun);
    RUN_TEST(test_time_impl);
    RUN_TEST(test_time_invalid_parsing);
    RUN_TEST(test_time_parsing);
    RUN_TEST(test_time_repeat);
    RUN_TEST(test_weekday);
    RUN_TEST(test_weekday_impl);
    RUN_TEST(test_weekday_invalid_parsing);
    RUN_TEST(test_weekday_offset);
    RUN_TEST(test_weekday_parsing);
    return UNITY_END();
}
