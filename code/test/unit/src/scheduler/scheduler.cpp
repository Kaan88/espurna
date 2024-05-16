#include <unity.h>

#include <Arduino.h>
#include <StreamString.h>
#include <ArduinoJson.h>

#include <espurna/utils.h>
#include <espurna/scheduler_time.re.ipp>
#include <espurna/scheduler_common.ipp>

#include <iostream>
#include <iomanip>

#include <string_view>
using namespace std::string_view_literals;

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
        << ' ' << wday(t.tm_wday) << ' '
        << t.tm_yday << "y "
        << t.tm_hour << ':' << t.tm_min << ':' << t.tm_sec
        << ((t.tm_isdst == 1) ? "+DST" :
            (t.tm_isdst == -1) ? "-DST" : "");

    return out;
};

namespace espurna {
namespace test {
namespace {

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

} // namespace
} // namespace test
} // namespace espurna

int main(int, char**) {
    UNITY_BEGIN();
    using namespace espurna::test;
    RUN_TEST(test_date_day_repeat);
    RUN_TEST(test_date_impl);
    RUN_TEST(test_date_month_glob);
    RUN_TEST(test_date_parsing);
    RUN_TEST(test_date_parsing_invalid);
    RUN_TEST(test_date_week_index);
    RUN_TEST(test_date_year);
    RUN_TEST(test_keyword_impl);
    RUN_TEST(test_keyword_parsing);
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
