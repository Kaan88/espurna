/*

SCHEDULER MODULE

Copyright (C) 2017 by faina09
Adapted by Xose Pérez <xose dot perez at gmail dot com>

Copyright (C) 2019-2024 by Maxim Prokhorov <prokhorov dot max at outlook dot com>

*/

#include "espurna.h"

#if SCHEDULER_SUPPORT

#include "api.h"
#include "curtain_kingart.h"
#include "datetime.h"
#include "mqtt.h"
#include "ntp.h"
#include "ntp_timelib.h"
#include "scheduler.h"
#include "types.h"
#include "ws.h"

#if TERMINAL_SUPPORT == 0
#if LIGHT_PROVIDER != LIGHT_PROVIDER_NONE
#include "light.h"
#endif
#if RELAY_SUPPORT
#include "relay.h"
#endif
#endif

#include "libs/EphemeralPrint.h"
#include "libs/PrintString.h"

// -----------------------------------------------------------------------------

#include "scheduler_common.ipp"
#include "scheduler_time.re.ipp"

#if SCHEDULER_SUN_SUPPORT
#include "scheduler_sun.ipp"
#endif

namespace espurna {
namespace scheduler {
namespace error {

STRING_VIEW_INLINE(InvalidTime, "Invalid time string");

} // namespace error

enum class Type : int {
    Unknown = 0,
    Disabled,
    Calendar,
    Relative,
};

namespace v1 {

enum class Type : int {
    None = 0,
    Relay,
    Channel,
    Curtain,
};

} // namespace v1

namespace {

bool initial { true };

constexpr auto EventTtl = datetime::Days{ 1 };
constexpr auto EventsMax = size_t{ 4 };

struct NamedEvent {
    String name;
    event::time_point time_point;
};

std::forward_list<NamedEvent> named_events;

NamedEvent* find_named(StringView name) {
    const auto it = std::find_if(
        named_events.begin(),
        named_events.end(),
        [&](const NamedEvent& entry) {
            return entry.name == name;
        });

    if (it != named_events.end()) {
        return &(*it);
    }

    return nullptr;
}

bool named_event(String name, datetime::Clock::time_point time_point) {
    auto it = find_named(name);
    if (it) {
        it->time_point = time_point;
        return true;
    }

    size_t size { 0 };

    auto last = named_events.begin();
    while (last != named_events.end()) {
        ++size;
        ++last;
    }

    if (size < EventsMax) {
        named_events.push_front(
            NamedEvent{
                .name = std::move(name),
                .time_point = time_point,
            });

        return true;
    }

    return false;
}

bool named_event(String name, StringView value) {
    datetime::DateHhMmSs datetime;
    bool utc { false };

    const auto result = parse_simple_iso8601(datetime, utc, value);
    if (!result) {
        return false;
    }

    return named_event(
        std::move(name),
        datetime::make_time_point(datetime, utc));
}

String format_time_point(datetime::Clock::time_point time_point) {
    return datetime::format_local_tz(time_point);
}

String format_named_event(const NamedEvent& event) {
    return format_time_point(event.time_point);
}

template <typename Container>
void cleanup_entries_impl(const datetime::Context& ctx, Container& container, datetime::Minutes ttl) {
    const auto minutes = to_minutes(ctx);
    container.remove_if(
        [&](const typename Container::value_type& entry) {
            return !event::is_valid(entry.time_point)
                || event::difference(minutes, entry.time_point) > ttl;
        });
}

void cleanup_named_events(const datetime::Context& ctx) {
    cleanup_entries_impl(ctx, named_events, EventTtl);
}

constexpr auto LastTtl = datetime::Days{ 1 };

struct Last {
    size_t index;
    datetime::Clock::time_point time_point;
};

String format_last_action(const Last& last) {
    return format_time_point(last.time_point);
}

std::forward_list<Last> last_actions;

Last* find_last(size_t index) {
    auto it = std::find_if(
        last_actions.begin(),
        last_actions.end(),
        [&](const Last& entry) {
            return entry.index == index;
        });

    if (it != last_actions.end()) {
        return &(*it);
    }

    return nullptr;
}

void last_action(size_t index, datetime::Clock::time_point time_point) {
    auto* it = find_last(index);
    if (it) {
        it->time_point = time_point;
        return;
    }

    last_actions.push_front(
        Last{
            .index = index,
            .time_point = time_point,
        });
}

void last_action(const datetime::Context& ctx, size_t index) {
    last_action(
        index, datetime::make_time_point(ctx));
}

datetime::Clock::time_point last_action(size_t index) {
    auto it = find_last(index);
    if (it) {
        return it->time_point;
    }

    return datetime::Clock::time_point(datetime::Seconds{ -1 });
}

void cleanup_last_actions(const datetime::Context& ctx) {
    cleanup_entries_impl(ctx, last_actions, LastTtl);
}

[[gnu::unused]]
bool check_parsed(const Relative& relative) {
    return relative.type != relative::Type::None;
}

[[gnu::unused]]
bool check_parsed(const Schedule& schedule) {
    return schedule.ok;
}

#if SCHEDULER_SUN_SUPPORT
namespace sun {

struct EventMatch : public Event {
    datetime::Date date;
    TimeMatch time;
};

struct Match {
    EventMatch rising;
    EventMatch setting;
};

Location location;
Match match;

auto next_update = event::DefaultTimePoint;

void setup();

void reset() {
    match.rising = EventMatch{};
    match.setting = EventMatch{};
}

} // namespace sun
#endif

namespace build {

constexpr size_t max() {
    return SCHEDULER_MAX_SCHEDULES;
}

constexpr Type type() {
    return Type::Unknown;
}

constexpr bool restore() {
    return 1 == SCHEDULER_RESTORE;
}

constexpr int restoreDays() {
    return SCHEDULER_RESTORE_DAYS;
}

#if SCHEDULER_SUN_SUPPORT
constexpr double latitude() {
    return SCHEDULER_LATITUDE;
}

constexpr double longitude() {
    return SCHEDULER_LONGITUDE;
}

constexpr double altitude() {
    return SCHEDULER_ALTITUDE;
}
#endif

} // namespace build

namespace settings {
namespace internal {

using espurna::settings::options::Enumeration;

STRING_VIEW_INLINE(Unknown, "unknown");
STRING_VIEW_INLINE(Disabled, "disabled");
STRING_VIEW_INLINE(Calendar, "calendar");
STRING_VIEW_INLINE(Relative, "relative");

static constexpr std::array<Enumeration<Type>, 4> Types PROGMEM {
   {{Type::Unknown, Unknown},
    {Type::Disabled, Disabled},
    {Type::Calendar, Calendar},
    {Type::Relative, Relative}}
};

namespace v1 {

STRING_VIEW_INLINE(None, "none");
STRING_VIEW_INLINE(Relay, "relay");
STRING_VIEW_INLINE(Channel, "channel");
STRING_VIEW_INLINE(Curtain, "curtain");

static constexpr std::array<Enumeration<scheduler::v1::Type>, 4> Types PROGMEM {
    {{scheduler::v1::Type::None, None},
     {scheduler::v1::Type::Relay, Relay},
     {scheduler::v1::Type::Channel, Channel},
     {scheduler::v1::Type::Curtain, Curtain}}
};

} // namespace v1
} // namespace internal
} // namespace settings

} // namespace
} // namespace scheduler

namespace settings {
namespace internal {

template <>
scheduler::Type convert(const String& value) {
    return convert(scheduler::settings::internal::Types, value, scheduler::Type::Unknown);
}

String serialize(scheduler::Type type) {
    return serialize(scheduler::settings::internal::Types, type);
}

template <>
scheduler::v1::Type convert(const String& value) {
    return convert(scheduler::settings::internal::v1::Types, value, scheduler::v1::Type::None);
}

String serialize(scheduler::v1::Type type) {
    return serialize(scheduler::settings::internal::v1::Types, type);
}

} // namespace internal
} // namespace settings
} // namespace espurna

namespace espurna {
namespace scheduler {
namespace {

bool tryParseId(StringView value, size_t& out) {
    return ::tryParseId(value, build::max(), out);
}

namespace settings {

STRING_VIEW_INLINE(Prefix, "sch");

namespace keys {

#if SCHEDULER_SUN_SUPPORT
STRING_VIEW_INLINE(Latitude, "schLat");
STRING_VIEW_INLINE(Longitude, "schLong");
STRING_VIEW_INLINE(Altitude, "schAlt");
#endif

STRING_VIEW_INLINE(Days, "schRstrDays");

STRING_VIEW_INLINE(Type, "schType");
STRING_VIEW_INLINE(Restore, "schRestore");
STRING_VIEW_INLINE(Time, "schTime");
STRING_VIEW_INLINE(Action, "schAction");

} // namespace keys

#if SCHEDULER_SUN_SUPPORT
double latitude() {
    return getSetting(keys::Latitude, build::latitude());
}

double longitude() {
    return getSetting(keys::Longitude, build::longitude());
}

double altitude() {
    return getSetting(keys::Altitude, build::altitude());
}
#endif

int restoreDays() {
    return getSetting(keys::Days, build::restoreDays());
}

Type type(size_t index) {
    return getSetting({keys::Type, index}, build::type());
}

bool restore(size_t index) {
    return getSetting({keys::Restore, index}, build::restore());
}

String time(size_t index) {
    return getSetting({keys::Time, index});
}

String action(size_t index) {
    return getSetting({keys::Action, index});
}

namespace internal {

#define ID_VALUE(NAME, FUNC)\
String NAME (size_t id) {\
    return espurna::settings::internal::serialize(FUNC(id));\
}

ID_VALUE(type, settings::type)
ID_VALUE(restore, settings::restore)

#undef ID_VALUE

#define EXACT_VALUE(NAME, FUNC)\
String NAME () {\
    return espurna::settings::internal::serialize(FUNC());\
}

EXACT_VALUE(restoreDays, settings::restoreDays);

#if SCHEDULER_SUN_SUPPORT
EXACT_VALUE(latitude, settings::latitude);
EXACT_VALUE(longitude, settings::longitude);
EXACT_VALUE(altitude, settings::altitude);
#endif

#undef EXACT_VALUE

} // namespace internal

static constexpr espurna::settings::query::Setting Settings[] PROGMEM {
    {keys::Days, internal::restoreDays},
#if SCHEDULER_SUN_SUPPORT
    {keys::Latitude, internal::latitude},
    {keys::Longitude, internal::longitude},
    {keys::Altitude, internal::altitude},
#endif
};

static constexpr espurna::settings::query::IndexedSetting IndexedSettings[] PROGMEM {
    {keys::Type, internal::type},
    {keys::Restore, internal::restore},
    {keys::Action, settings::action},
    {keys::Time, settings::time},
};

struct Parsed {
    bool date { false };
    bool weekdays { false };
    bool time { false };
};

Schedule schedule(size_t index) {
    return parse_schedule(settings::time(index));
}

Relative relative(size_t index) {
    return parse_relative(settings::time(index));
}

template <typename T>
void foreach_type(T&& callback) {
    for (size_t index = 0; index < build::max(); ++index) {
        const auto type = settings::type(index);
        if (type == Type::Unknown) {
            break;
        }

        callback(type);
    }
}

using Types = std::vector<Type>;

Types types() {
    Types out;

    foreach_type([&](Type type) {
        out.push_back(type);
    });

    return out;
}

size_t count() {
    size_t out { 0 };

    foreach_type([&](Type type) {
        ++out;
    });

    return out;
}

void gc(size_t total) {
    DEBUG_MSG_P(PSTR("[SCH] Registered %zu schedule(s)\n"), total);
    for (size_t index = total; index < build::max(); ++index) {
        for (auto setting : IndexedSettings) {
            delSetting({setting.prefix(), index});
        }
    }
}

bool checkSamePrefix(StringView key) {
    return key.startsWith(settings::Prefix);
}

espurna::settings::query::Result findFrom(StringView key) {
    return espurna::settings::query::findFrom(Settings, key);
}

void setup() {
    ::settingsRegisterQueryHandler({
        .check = checkSamePrefix,
        .get = findFrom,
    });
}

bool validate(Type type, StringView spec) {
    switch (type) {
    case Type::Unknown:
        break;

    case Type::Disabled:
        return true;

    case Type::Calendar:
        return check_parsed(parse_schedule(spec));

    case Type::Relative:
        return check_parsed(parse_relative(spec));
    }

    return false;
}

bool validate(StringView spec) {
    return validate(Type::Calendar, spec)
        || validate(Type::Relative, spec);
}

#if DEBUG_SUPPORT || WEB_SUPPORT
void report_validate(size_t index) {
    DEBUG_MSG_P(PSTR("[SCH] ERROR: #%zu -> %.*s\n"),
            index,
            error::InvalidTime.length(),
            error::InvalidTime.data());
#if WEB_SUPPORT
    wsPost([index](JsonObject& root) {
        auto& info = root.createNestedArray(STRING_VIEW("schValidate"));
        info.add(index);
        info.add(keys::Time);
        info.add(error::InvalidTime);
    });
#endif
}
#endif

void validate() {
#if DEBUG_SUPPORT || WEB_SUPPORT
    for (size_t index = 0; index < build::max(); ++index) {
        const auto type = settings::type(index);

        switch (type) {
        case Type::Unknown:
            return;

        case Type::Disabled:
            break;

        case Type::Calendar:
        case Type::Relative:
            if (!validate(type, time(index))) {
                report_validate(index);
            }

            break;
        }
    }
#endif
}

} // namespace settings

namespace v1 {

using scheduler::v1::Type;

namespace settings {
namespace keys {

STRING_VIEW_INLINE(Enabled, "schEnabled");

STRING_VIEW_INLINE(Switch, "schSwitch");
STRING_VIEW_INLINE(Target, "schTarget");

STRING_VIEW_INLINE(Hour, "schHour");
STRING_VIEW_INLINE(Minute, "schMinute");
STRING_VIEW_INLINE(Weekdays, "schWDs");
STRING_VIEW_INLINE(UTC, "schUTC");

static constexpr std::array<StringView, 5> List {
    Enabled,
    Target,
    Hour,
    Minute,
    Weekdays,
};

} // namespace keys

STRING_VIEW_INLINE(DefaultWeekdays, "1,2,3,4,5,6,7");

bool enabled(size_t index) {
    return getSetting({keys::Enabled, index}, false);
}

Type type(size_t index) {
    return getSetting({espurna::scheduler::settings::keys::Type, index}, Type::None);
}

int target(size_t index) {
    return getSetting({keys::Target, index}, 0);
}

int action(size_t index) {
    using namespace espurna::scheduler::settings::keys;
    return getSetting({Action, index}, 0);
}

int hour(size_t index) {
    return getSetting({keys::Hour, index}, 0);
}

int minute(size_t index) {
    return getSetting({keys::Minute, index}, 0);
}

String weekdays(size_t index) {
    return getSetting({keys::Weekdays, index}, DefaultWeekdays);
}

bool utc(size_t index) {
    return getSetting({keys::UTC, index}, false);
}

} // namespace settings

String convert_time(const String& weekdays, int hour, int minute, bool utc) {
    String out;

    // implicit mon..sun already by default
    if (weekdays != settings::DefaultWeekdays) {
        out += weekdays;
        out += ' ';
    }

    if (hour < 10) {
        out += '0';
    }

    out += String(hour, 10);
    out += ':';

    if (minute < 10) {
        out += '0';
    }

    out += String(minute, 10);

    if (utc) {
        out += STRING_VIEW(" UTC");
    }

    return out;
}

String convert_action(Type type, int target, int action) {
    String out;

    StringView prefix;

    switch (type) {
    case Type::None:
        break;

    case Type::Relay:
    {
        STRING_VIEW_INLINE(Relay, "relay");
        prefix = Relay;
        break;
    }

    case Type::Channel:
    {
        STRING_VIEW_INLINE(Channel, "channel");
        prefix = Channel;
        break;
    }

    case Type::Curtain:
    {
        STRING_VIEW_INLINE(Curtain, "curtain");
        prefix = Curtain;
        break;
    }

    }

    if (prefix.length()) {
        out += prefix.toString()
            + ' ';
        out += String(target, 10)
            + ' '
            + String(action, 10);
    }

        return out;
    }

String convert_type(bool enabled, Type type) {
    auto out = scheduler::Type::Unknown;

    switch (type) {
    case Type::None:
        break;

    case Type::Relay:
    case Type::Channel:
    case Type::Curtain:
        out = scheduler::Type::Calendar;
        break;
    }

    if (!enabled && (out != scheduler::Type::Unknown)) {
        out = scheduler::Type::Disabled;
    }

    return ::espurna::settings::internal::serialize(out);
}

void migrate() {
    for (size_t index = 0; index < build::max(); ++index) {
        const auto type = settings::type(index);

        setSetting({scheduler::settings::keys::Type, index},
            convert_type(settings::enabled(index), type));

        setSetting({scheduler::settings::keys::Time, index},
            convert_time(settings::weekdays(index),
                settings::hour(index),
                settings::minute(index),
                settings::utc(index)));

        setSetting({scheduler::settings::keys::Action, index},
            convert_action(type,
                settings::target(index),
                settings::action(index)));

        for (auto& key : settings::keys::List) {
            delSetting({key, index});
        }
    }
}

} // namespace v1

namespace settings {

void migrate(int version) {
    if (version < 6) {
        moveSettings(
            v1::settings::keys::Switch.toString(),
            v1::settings::keys::Target.toString());
    }

    if (version < 15) {
        v1::migrate();
    }
}

} // namespace settings

#if SCHEDULER_SUN_SUPPORT
namespace sun {

STRING_VIEW_INLINE(Module, "sun");

void setup() {
    location.latitude = settings::latitude();
    location.longitude = settings::longitude();
    location.altitude = settings::altitude();
}

EventMatch* find_event_match(const TimeMatch& m) {
    if (want_sunrise(m)) {
        return &match.rising;
    } else if (want_sunset(m)) {
        return &match.setting;
    }

    return nullptr;
}

EventMatch* find_event_match(const Schedule& schedule) {
    return find_event_match(schedule.time);
}

tm make_utc_date_time(datetime::Seconds seconds) {
    tm out{};

    time_t timestamp{ seconds.count() };
    gmtime_r(&timestamp, &out);

    return out;
}

datetime::Date make_date(const tm& date_time) {
    datetime::Date out;

    out.year = date_time.tm_year + 1900;
    out.month = date_time.tm_mon + 1;
    out.day = date_time.tm_mday;

    return out;
}

TimeMatch make_time_match(const tm& date_time) {
    TimeMatch out;

    out.hour[date_time.tm_hour] = true;
    out.minute[date_time.tm_min] = true;
    out.flags = FlagUtc;

    return out;
}

void update_event_match(EventMatch& match, datetime::Clock::time_point time_point) {
    const auto next_valid = event::is_valid(match.next);
    if (!event::is_valid(time_point)) {
        if (next_valid) {
            match.last = match.next;
        }

        match.next = event::DefaultTimePoint;
        return;
    }

    const auto duration = time_point.time_since_epoch();

    const auto date_time = make_utc_date_time(duration);
    match.date = make_date(date_time);
    match.time = make_time_match(date_time);

    if (next_valid) {
        match.last = match.next;
    }

    match.next = time_point;
}

void update_schedule_from(Schedule& schedule, const EventMatch& match) {
    schedule.date.day[match.date.day] = true;
    schedule.date.month[match.date.month] = true;
    schedule.date.year = match.date.year;
    schedule.time = match.time;
}

bool update_schedule(Schedule& schedule) {
    // if not sun{rise,set} schedule, keep it as-is
    const auto* selected = sun::find_event_match(schedule);
    if (nullptr == selected) {
        return true;
    }

    // in case calculation failed, no use here
    if (!event::is_valid((*selected).next)) {
        return false;
    }

    // make sure event can actually trigger with this date spec
    if (::espurna::scheduler::match(schedule.date, (*selected).date)) {
        update_schedule_from(schedule, *selected);
        return true;
    }

    return false;
}

bool needs_update(datetime::Clock::time_point time_point) {
    const auto next = event::is_valid(next_update);
    if (next) {
        return event::less(next_update, time_point);
    }

    return event::less(match.rising.next, time_point)
        || event::less(match.setting.next, time_point);
}

template <typename T>
datetime::Clock::time_point delta_compare(tm& out, datetime::Clock::time_point, T);

void update(datetime::Clock::time_point, const tm& today) {
    const auto result = sun::sunrise_sunset(location, today);
    update_event_match(match.rising, result.sunrise);
    update_event_match(match.setting, result.sunset);
}

template <typename T>
datetime::Clock::time_point update(datetime::Clock::time_point time_point, const tm& today, T compare) {
    auto result = sun::sunrise_sunset(location, today);

    const auto reset_sunrise =
        !event::is_valid(result.sunrise) || compare(time_point, result.sunrise);

    const auto reset_sunset =
        !event::is_valid(result.sunset) || compare(time_point, result.sunset);

    auto out = event::DefaultTimePoint;

    tm tmp;
    if (reset_sunrise || reset_sunset) {
        std::memcpy(&tmp, &today, sizeof(tmp));

        out = delta_compare(tmp, time_point, compare);

        const auto other = sun::sunrise_sunset(location, tmp);
        if (reset_sunrise && event::is_valid(other.sunrise)) {
            result.sunrise = other.sunrise;
        }

        if (reset_sunset && event::is_valid(other.sunset)) {
            result.sunset = other.sunset;
        }
    }

    update_event_match(match.rising, result.sunrise);
    update_event_match(match.setting, result.sunset);

    return out;
}

template <typename T>
void update(time_t timestamp, const tm& today, T&& compare) {
    update(datetime::make_time_point(timestamp), today, std::forward<T>(compare));
}

String format_time_point(const event::time_point& time_point) {
    return (time_point.time_since_epoch() > datetime::Clock::duration::zero())
        ? datetime::format_local_tz(time_point)
        : STRING_VIEW("value not set").toString();
}

String format_next(const EventMatch& match) {
    return format_time_point(match.next);
}

String format_last(const EventMatch& match) {
    return format_time_point(match.last);
}

// check() needs current or future events, discard timestamps in the past
// round to minutes when doing so as well, since std::greater<> would compare seconds
struct CompareAfter {
    bool operator()(const event::time_point& lhs, const event::time_point& rhs) {
        return event::greater(lhs, rhs);
    }
};

datetime::Clock::time_point delta_compare_days(tm& out, datetime::Clock::time_point time_point, datetime::Days days) {
    return datetime::make_time_point(
        datetime::delta_utc(out, time_point.time_since_epoch(), days));
}

template <>
datetime::Clock::time_point delta_compare(tm& out, datetime::Clock::time_point time_point, CompareAfter) {
    return delta_compare_days(out, time_point, datetime::Days{ 1 });
}

void update_after(const datetime::Context& ctx) {
    const auto time_point = event::make_time_point(ctx);
    if (!needs_update(time_point)) {
        return;
    }

    auto next = update(time_point, ctx.utc, CompareAfter{});

    if (event::is_valid(match.rising.next)) {
        DEBUG_MSG_P(PSTR("[SCH] Sunrise at %s\n"),
            datetime::format_local_tz(match.rising.next).c_str());
    }

    if (event::is_valid(match.setting.next)) {
        DEBUG_MSG_P(PSTR("[SCH] Sunset at %s\n"),
            datetime::format_local_tz(match.setting.next).c_str());
    }

    const event::time_point unordered[] {
        next,
        match.rising.next,
        match.setting.next,
    };

    next_update = event::DefaultTimePoint;

    for (const auto& value : unordered) {
        if (!event::is_valid(value)) {
            continue;
        }

        next_update = event::is_valid(next_update)
            ? std::min(next_update, value)
            : value;
    }
}

// relative events need current or past time point
struct CompareBefore {
    bool operator()(const event::time_point& lhs, const event::time_point& rhs) {
        return event::less(lhs, rhs);
    }
};

template <>
datetime::Clock::time_point delta_compare(tm& out, datetime::Clock::time_point time_point, CompareBefore) {
    return delta_compare_days(out, time_point, datetime::Days{ -1 });
}

void update_before(const datetime::Context& ctx) {
    const auto time_point = event::make_time_point(ctx);
    update(time_point, ctx.utc, CompareBefore{});

    match.rising.last = match.rising.next;
    match.rising.next = event::DefaultTimePoint;

    match.setting.last = match.setting.next;
    match.setting.next = event::DefaultTimePoint;
}

} // namespace sun
#endif

// -----------------------------------------------------------------------------

#if TERMINAL_SUPPORT
namespace terminal {

#if SCHEDULER_SUN_SUPPORT
namespace internal {

struct Datetime {
    String last;
    String next;
};

Datetime sunrise_sunset(const sun::EventMatch& match) {
    return Datetime{
        .last = sun::format_last(match),
        .next = sun::format_next(match),
    };
}

void format_output(::terminal::CommandContext& ctx, const String& prefix, const Datetime& datetime) {
    ctx.output.printf_P(PSTR("- %s\n  last: %s\n  next: %s\n"),
        prefix.c_str(),
        datetime.last.c_str(),
        datetime.next.c_str());
}

void dump_sunrise_sunset(::terminal::CommandContext& ctx) {
    if (event::is_valid(sun::next_update)) {
        ctx.output.printf_P(PSTR("- Next sunrise & sunset update at %s\n"),
            datetime::format_local_tz(sun::next_update).c_str());
    }

    format_output(ctx,
        STRING_VIEW("Sunrise").toString(),
        sunrise_sunset(sun::match.rising));
    format_output(ctx,
        STRING_VIEW("Sunset").toString(),
        sunrise_sunset(sun::match.setting));
}

} // namespace internal
#endif

// SCHEDULE [<ID>]
// SCHEDULE CHECK [CHUNK(S)...]
PROGMEM_STRING(Entrypoint, "SCHEDULE");

void entrypoint(::terminal::CommandContext&& ctx) {
    if (ctx.argv.size() > 2 && (STRING_VIEW("CHECK").equalsIgnoreCase(ctx.argv[1]))) {
        String spec;

        size_t reserve = 0;
        std::for_each(
            ctx.argv.begin() + 2,
            ctx.argv.end(),
            [&](const String& arg) {
                reserve += 1 + arg.length();
            });
        spec.reserve(reserve);

        if (ctx.argv.size() > 3) {
            for (size_t index = 2; index < ctx.argv.size(); ++index) {
                if (index > 2) {
                    spec += ' ';
                }

                spec += ctx.argv[index];
            }
        } else {
            spec = std::move(ctx.argv[2]);
        }

        if (settings::validate(spec)) {
            terminalOK(ctx);
            return;
        }

        terminalError(ctx, error::InvalidTime);
        return;
    }

    if (ctx.argv.size() != 2) {
        settingsDump(ctx, settings::Settings);
        return;
    }

    size_t id;
    if (!tryParseId(ctx.argv[1], id)) {
        terminalError(ctx, F("Invalid ID"));
        return;
    }

    const auto last = find_last(id);
    if (last) {
        ctx.output.printf_P(PSTR("last action: %s\n"),
            format_time_point((*last).time_point).c_str());
    }

    settingsDump(ctx, settings::IndexedSettings, id);
    terminalOK(ctx);
}

PROGMEM_STRING(Event, "EVENT");

// EVENT [<NAME>] [<DATETIME>]
void event(::terminal::CommandContext&& ctx) {
    String name;

    if (ctx.argv.size() == 2) {
        name = std::move(ctx.argv[1]);
    }

    if (ctx.argv.size() != 3) {
        for (auto& entry : named_events) {
            if (name.length() && entry.name != name) {
                continue;
            }

            ctx.output.printf_P(PSTR("- \"%s\"\n    at: %s\n"),
                entry.name.c_str(),
                format_named_event(entry).c_str());

            if (name.length()) {
                terminalOK(ctx);
                return;
            }
        }

        if (name.length()) {
            terminalError(ctx, STRING_VIEW("Invalid name"));
            return;
        }

        if (!last_actions.empty()) {
            ctx.output.print(PSTR("Calendar events:\n"));
            for (auto& entry : last_actions) {
                ctx.output.printf_P(PSTR("- cal#%zu\n    at %s\n"),
                    entry.index,
                    format_last_action(entry).c_str());
            }
        }

#if SCHEDULER_SUN_SUPPORT
        internal::dump_sunrise_sunset(ctx);
#endif

        terminalOK(ctx);
        return;
    }

    if (named_event(std::move(ctx.argv[1]), ctx.argv[2])) {
        terminalOK(ctx);
        return;
    }

    terminalError(ctx, STRING_VIEW("Cannot set event"));
}

static constexpr ::terminal::Command Commands[] PROGMEM {
    {Entrypoint, entrypoint},
    {Event, event},
};

void setup() {
    espurna::terminal::add(Commands);
}

} // namespace terminal
#endif

// -----------------------------------------------------------------------------

#if API_SUPPORT
namespace api {
namespace keys {

STRING_VIEW_INLINE(Type, "type");
STRING_VIEW_INLINE(Restore, "restore");
STRING_VIEW_INLINE(Time, "time");
STRING_VIEW_INLINE(Action, "action");

} // namespace keys

using espurna::settings::internal::serialize;
using espurna::settings::internal::convert;

struct Schedule {
    size_t id;
    Type type;
    int restore;
    String time;
    String action;
};

void print(JsonObject& root, const Schedule& schedule) {
    root[keys::Type] = serialize(schedule.type);
    root[keys::Restore] = (1 == schedule.restore);
    root[keys::Action] = schedule.action;
    root[keys::Time] = schedule.time;
}

template <typename T>
bool set_typed(T& out, JsonObject& root, StringView key) {
    auto value = root[key];
    if (value.success()) {
        out = value.as<T>();
        return true;
    }

    return false;
}

template <>
bool set_typed<Type>(Type& out, JsonObject& root, StringView key) {
    auto value = root[key];
    if (!value.success()) {
        return false;
    }

    auto type = convert<Type>(value.as<String>());
    if (type != Type::Unknown) {
        out = type;
        return true;
    }

    return false;
}

template
bool set_typed<String>(String&, JsonObject&, StringView);

template
bool set_typed<bool>(bool&, JsonObject&, StringView);

void update_from(const Schedule& schedule) {
    setSetting({keys::Type, schedule.id}, serialize(schedule.type));
    setSetting({keys::Time, schedule.id}, schedule.time);
    setSetting({keys::Action, schedule.id}, schedule.action);

    if (schedule.restore != -1) {
        setSetting({keys::Restore, schedule.id}, serialize(1 == schedule.restore));
    }
}

bool set(JsonObject& root, const size_t id) {
    Schedule out;
    out.restore = -1;

    // always need type, time and action
    if (!set_typed(out.type, root, keys::Type)) {
        return false;
    }

    if (!set_typed(out.time, root, keys::Time)) {
        return false;
    }

    if (!set_typed(out.action, root, keys::Action)) {
        return false;
    }

    // optional restore flag
    bool restore;
    if (set_typed(restore, root, keys::Restore)) {
        out.restore = restore ? 1 : 0;
    }

    update_from(out);

    return true;
}

Schedule make_schedule(size_t id) {
    Schedule out;

    out.type = settings::type(id);
    if (out.type != Type::Unknown) {
        out.id = id;
        out.restore = settings::restore(id) ? 1 : 0;
        out.time = settings::time(id);
        out.action = settings::action(id);
    }

    return out;
}

namespace schedules {

bool get(ApiRequest&, JsonObject& root) {
    JsonArray& out = root.createNestedArray("schedules");

    for (size_t id = 0; id < build::max(); ++id) {
        const auto sch = make_schedule(id);
        if (sch.type == Type::Unknown) {
            break;
        }

        auto& root = out.createNestedObject();
        print(root, sch);
    }

    return true;
}

bool set(ApiRequest&, JsonObject& root) {
    size_t id = 0;
    while (hasSetting({settings::keys::Type, id})) {
        ++id;
    }

    if (id < build::max()) {
        return api::set(root, id);
    }

    return false;
}

} // namespace schedules

namespace schedule {

bool get(ApiRequest& req, JsonObject& root) {
    const auto param = req.wildcard(0);

    size_t id;
    if (tryParseId(param, id)) {
        const auto sch = make_schedule(id);
        if (sch.type == Type::Unknown) {
            return false;
        }

        print(root, sch);
        return true;
    }

    return false;
}

bool set(ApiRequest& req, JsonObject& root) {
    const auto param = req.wildcard(0);

    size_t id;
    if (tryParseId(param, id)) {
        return api::set(root, id);
    }

    return false;
}

} // namespace schedules

constexpr char Events[] = "events";
constexpr char Name[] = "name";
constexpr char Datetime[] = "datetime";

namespace events {

bool get(ApiRequest& req, JsonObject& root) {
    const auto param = req.wildcard(0);

    auto& events = root.createNestedArray(Events);
    for (auto& event : named_events) {
        auto& entry = events.createNestedObject();
        entry[Name] = event.name.c_str();
        entry[Datetime] = format_named_event(event);
    }

    return true;
}

} // namespace events

namespace event {

bool get(ApiRequest& req, JsonObject& root) {
    const auto param = req.wildcard(0);

    const auto it = find_named(param);
    if (it) {
        root[Datetime] = format_named_event(*it);
        return true;
    }

    return false;
}

bool set(ApiRequest& req, JsonObject& root) {
    const auto datetime = root[Datetime];
    if (!datetime.success() || !datetime.is<String>()) {
        return false;
    }

    return named_event(
        req.wildcard(0), datetime.as<String>());
}

} // namespace event

void setup() {
    apiRegister(F(MQTT_TOPIC_SCHEDULE), schedules::get, schedules::set);
    apiRegister(F(MQTT_TOPIC_SCHEDULE "/+"), schedule::get, schedule::set);

    apiRegister(F(MQTT_TOPIC_NAMED_EVENT), events::get, nullptr);
    apiRegister(F(MQTT_TOPIC_NAMED_EVENT "/+"), event::get, event::set);
}

} // namespace api
#endif  // API_SUPPORT

// -----------------------------------------------------------------------------
#if MQTT_SUPPORT
namespace mqtt {

void callback(unsigned int type, StringView topic, StringView payload) {
    if (type == MQTT_CONNECT_EVENT) {
        mqttSubscribe(MQTT_TOPIC_NAMED_EVENT "/+");
        return;
    }

    STRING_VIEW_INLINE(Topic, MQTT_TOPIC_NAMED_EVENT);
    if (type == MQTT_MESSAGE_EVENT) {
        const auto t = mqttMagnitude(topic);
        if (!t.startsWith(Topic)) {
            return;
        }

        const auto name = t.slice(Topic.length() + 1);
        if (!name.length()) {
            return;
        }

        named_event(name.toString(), payload);
        return;
    }
}

void setup() {
    ::mqttRegister(callback);
}

} // namespace mqtt
#endif

// -----------------------------------------------------------------------------

#if WEB_SUPPORT
namespace web {

bool onKey(StringView key, const JsonVariant&) {
    return key.startsWith(settings::Prefix);
}

void onVisible(JsonObject& root) {
    wsPayloadModule(root, settings::Prefix);
#if SCHEDULER_SUN_SUPPORT
    wsPayloadModule(root, sun::Module);
#endif

    for (const auto& pair : settings::Settings) {
        root[pair.key()] = pair.value();
    }

    espurna::web::ws::EnumerableTypes types{root, STRING_VIEW("schTypes")};
    types(settings::internal::Types);
}

void onConnected(JsonObject& root){
    espurna::web::ws::EnumerableConfig config{root, STRING_VIEW("schConfig")};
    config.replacement(
        settings::internal::type,
        [](JsonArray& out, size_t index) {
            out.add(std::to_underlying(settings::type(index)));
        });
    config(STRING_VIEW("schedules"), settings::count(), settings::IndexedSettings);

    auto& schedules = config.root();
    schedules["max"] = build::max();
}

void setup() {
    wsRegister()
        .onVisible(onVisible)
        .onConnected(onConnected)
        .onKeyCheck(onKey);
}

} // namespace web
#endif

// When terminal is disabled, still allow minimum set of actions that we available in v1

#if TERMINAL_SUPPORT == 0
namespace terminal_stub {

#if RELAY_SUPPORT
namespace relay {

void action(SplitStringView split) {
    if (!split.next()) {
        return;
    }

    size_t id;
    if (!::tryParseId(split.current(), relayCount(), id)) {
        return;
    }

    if (!split.next()) {
        return;
    }

    const auto status = relayParsePayload(split.current());
    switch (status) {
    case PayloadStatus::Unknown:
        break;

    case PayloadStatus::Off:
    case PayloadStatus::On:
        relayStatus(id, (status == PayloadStatus::On));
        break;

    case PayloadStatus::Toggle:
        relayToggle(id);
        break;
    }
}

} // namespace relay
#endif

#if LIGHT_PROVIDER != LIGHT_PROVIDER_NONE
namespace light {

void action(SplitStringView split) {
    if (!split.next()) {
        return;
    }

    size_t id;
    if (!tryParseId(split.current(), lightChannels(), id)) {
        return;
    }

    if (!split.next()) {
        return;
    }

    const auto convert = ::espurna::settings::internal::convert<long>;
    lightChannel(id, convert(split.current().toString()));
    lightUpdate();
}

} // namespace light
#endif

#if CURTAIN_SUPPORT
namespace curtain {

void action(SplitStringView split) {
    if (!split.next()) {
        return;
    }

    size_t id;
    if (!tryParseId(split.current(), curtainCount(), id)) {
        return;
    }

    if (!split.next()) {
        return;
    }

    const auto convert = ::espurna::settings::internal::convert<int>;
    curtainUpdate(id, convert(split.current().toString()));
}

} // namespace curtain
#endif

void parse_action(String action) {
    auto split = SplitStringView{ action };
    if (!split.next()) {
        return;
    }

    auto current = split.current();

#if RELAY_SUPPORT
    if (current == STRING_VIEW("relay")) {
        relay::action(split);
        return;
    }
#endif
#if LIGHT_PROVIDER != LIGHT_PROVIDER_NONE
    if (current == STRING_VIEW("channel")) {
        light::action(split);
        return;
    }
#endif
#if CURTAIN_SUPPORT
    if (current == STRING_VIEW("curtain")) {
        curtain::action(split);
        return;
    }
#endif

    DEBUG_MSG_P(PSTR("[SCH] Unknown action: %s\n"), action.c_str());
}

} // namespace terminal_stub

using terminal_stub::parse_action;

#else

void parse_action(String action) {
    if (!action.endsWith("\r\n") && !action.endsWith("\n")) {
        action.concat('\n');
    }

    static EphemeralPrint output;
    PrintString error(64);

    if (!espurna::terminal::api_find_and_call(action, output, error)) {
        DEBUG_MSG_P(PSTR("[SCH] %s\n"), error.c_str());
        return;
    }
}

#endif

Schedule load_schedule(size_t index) {
    auto out = settings::schedule(index);
    if (!out.ok) {
        return out;
    }

#if SCHEDULER_SUN_SUPPORT
    if (want_sunrise_sunset(out.time) && !sun::update_schedule(out)) {
        out.ok = false;
    }
#else
    if (want_sunrise_sunset(out.time)) {
        out.ok = false;
    }
#endif

    return out;
}

bool match_schedule(const Schedule& schedule, const tm& time_point) {
    if (!match(schedule.date, time_point)) {
        return false;
    }

    if (!match(schedule.weekdays, time_point)) {
        return false;
    }

    return match(schedule.time, time_point);
}

bool check_calendar(const datetime::Context& ctx, size_t index) {
    const auto schedule = load_schedule(index);
    return schedule.ok
        && match_schedule(schedule, select_time(ctx, schedule));
}

namespace restore {

[[gnu::used]]
void Context::init() {
#if SCHEDULER_SUN_SUPPORT
    const auto seconds = datetime::Seconds{ this->current.timestamp };
    const auto minutes =
        std::chrono::duration_cast<datetime::Minutes>(seconds);

    const auto time_point =
        datetime::Clock::time_point(minutes);

    sun::update(time_point, this->current.utc);
#endif
}

[[gnu::used]]
void Context::init_delta() {
#if SCHEDULER_SUN_SUPPORT
    init();

    for (auto& pending : this->pending) {
        // additional logic in handle_delta. keeps as pending when current value does not pass date match()
        pending.schedule.ok =
            sun::update_schedule(pending.schedule);
    }
#endif
}

[[gnu::used]]
void Context::destroy() {
#if SCHEDULER_SUN_SUPPORT
    sun::reset();
#endif
}

// otherwise, there are pending results that need extra days to check
void run_delta(Context& ctx) {
    if (!ctx.pending.size()) {
        return;
    }

    const auto days = settings::restoreDays();
    for (int day = 0; day < days; ++day) {
        if (!ctx.next()) {
            break;
        }

        for (auto it = ctx.pending.begin(); it != ctx.pending.end();) {
            if (handle_pending(ctx, *it)) {
                it = ctx.pending.erase(it);
            } else {
                it = std::next(it);
            }
        }
    }
}

// if schedule was due earlier today, make sure this gets checked first
void run_today(Context& ctx) {
    for (size_t index = 0; index < build::max(); ++index) {
        switch (settings::type(index)) {
        case Type::Unknown:
            return;

        case Type::Disabled:
        case Type::Relative:
            continue;

        case Type::Calendar:
            break;
        }

        if (!settings::restore(index)) {
            continue;
        }

        auto schedule = settings::schedule(index);
        if (!schedule.ok) {
            continue;
        }

#if SCHEDULER_SUN_SUPPORT
        if (!sun::update_schedule(schedule)) {
            ctx.push_pending(index, schedule);
            continue;
        }
#else
        if (want_sunrise_sunset(schedule.time)) {
            continue;
        }
#endif

        handle_today(ctx, index, schedule);
    }
}

void run(const datetime::Context& base) {
    Context ctx{ base };

    run_today(ctx);
    run_delta(ctx);

    ctx.sort();

    for (auto& result : ctx.results) {
        const auto action = settings::action(result.index);
        DEBUG_MSG_P(PSTR("[SCH] Restoring #%zu => %s (%sm)\n"),
            result.index, action.c_str(),
            String(result.offset.count(), 10).c_str());
        last_action(base, result.index);
        parse_action(action);
    }
}

} // namespace restore

namespace relative {

struct Source {
    virtual ~Source();

    virtual const event::time_point& time_point() const = 0;
    virtual bool before(const datetime::Context&) = 0;
    virtual bool after(const datetime::Context&) = 0;
};

Source::~Source() = default;

struct Calendar : public Source {
    Calendar(size_t index, std::shared_ptr<expect::Context> expect) :
        _expect(expect),
        _index(index)
    {}

    const event::time_point& time_point() const override {
        return _time_point;
    }

    bool before(const datetime::Context&) override;
    bool after(const datetime::Context&) override;

private:
    void _reset_expected(const datetime::Context& ctx, datetime::Minutes offset) {
        _time_point = datetime::make_time_point(to_minutes(ctx) + offset);
    }

    void _reset_expected(const datetime::Context& ctx, const expect::Context& expect) {
        _reset_expected(ctx, expect.results.back().offset);
    }

    std::shared_ptr<expect::Context> _expect;
    size_t _index;

    event::time_point _time_point{ event::DefaultTimePoint };
};

bool Calendar::before(const datetime::Context& ctx) {
    if (event::is_valid(_time_point)) {
        return true;
    }

    const auto it = std::find_if(
        _expect->results.begin(),
        _expect->results.end(),
        [&](const Offset& offset) {
            return offset.index == _index;
        });

    if (it != _expect->results.end()) {
        _reset_expected(ctx, (*it).offset);
        return true;
    }

    const auto schedule = load_schedule(_index);
    if (!schedule.ok) {
        return false;
    }

    if ((_expect->days == _expect->days.zero()) && handle_today(*_expect, _index, schedule)) {
        _reset_expected(ctx, *_expect);
        return true;
    }

    const auto pending = std::find_if(
        _expect->pending.begin(),
        _expect->pending.end(),
        [&](const Pending& pending) {
            return pending.index == _index;
        });

    if (pending == _expect->pending.end()) {
        return false;
    }

    if (handle_pending(*_expect, *pending)) {
        _reset_expected(ctx, *_expect);
        return true;
    }

    // assuming this only happen once, after +1day shift
    _expect->pending.erase(pending);

    return false;
}

bool Calendar::after(const datetime::Context& ctx) {
    _time_point = last_action(_index);
    return event::is_valid(_time_point);
}

struct Named : public Source {
    explicit Named(String&& name) :
        _name(std::move(name))
    {}

    const datetime::Clock::time_point& time_point() const override {
        return _time_point;
    }

    bool before(const datetime::Context&) override;
    bool after(const datetime::Context&) override;

private:
    bool _reset_named(StringView name) {
        const auto it = find_named(name);
        if (it) {
            _time_point = it->time_point;
        }

        return event::is_valid(_time_point);
    }

    String _name;
    datetime::Clock::time_point _time_point{ event::DefaultTimePoint };
};

bool Named::before(const datetime::Context&) {
    return event::is_valid(_time_point) || _reset_named(_name);
}

bool Named::after(const datetime::Context&) {
    return event::is_valid(_time_point) || _reset_named(_name);
}

#if SCHEDULER_SUN_SUPPORT
struct Sun : public Source {
    explicit Sun(const Event* event) :
        _event(event)
    {}

    bool before(const datetime::Context&) override {
        return _reset_time_point(_event->next);
    }

    bool after(const datetime::Context&) override {
        return _reset_time_point(_event->last);
    }

    const datetime::Clock::time_point& time_point() const override {
        return *_time_point;
    }

private:
    bool _reset_time_point(const datetime::Clock::time_point& time_point) {
        _time_point = event::is_valid(time_point)
            ? &time_point
            : &event::DefaultTimePoint;

        return event::is_valid(*_time_point);
    }

    const Event* _event;
    const event::time_point* _time_point{ &event::DefaultTimePoint };
};

struct Sunset : public Sun {
    Sunset() :
        Sun(&sun::match.setting)
    {}
};

struct Sunrise : public Sun {
    Sunrise() :
        Sun(&sun::match.rising)
    {}
};
#endif

struct EventOffset : public Offset {
    std::unique_ptr<Source> source;
    relative::Order order;
};

using EventOffsets = std::vector<EventOffset>;

void process_valid_event_offsets(const datetime::Context& ctx, EventOffsets& pending, relative::Order order) {
    std::vector<size_t> matched;

    auto it = pending.begin();
    while (it != pending.end()) {
        if ((*it).order != order) {
            goto next;
        }

        // expect required event time point ('next' or 'last') to exist for the requested 'order'
        {
            const auto& time_point = (*it).source->time_point();
            if (!event::is_valid(time_point)) {
                goto next;
            }

            const auto diff = event::difference(ctx, time_point);
            if (diff == (*it).offset) {
                matched.push_back((*it).index);
            }
        }

        // always fall-through and erase
        it = pending.erase(it);
        continue;

        // only move where when explicitly told to
next:
        it = std::next(it);
        continue;
    }

    for (const auto& match : matched) {
        parse_action(settings::action(match));
    }
}

struct Prepared {
    Span<const scheduler::Type> types;
    EventOffsets event_offsets;
    std::shared_ptr<expect::Context> expect;

    explicit operator bool() const {
        return event_offsets.size() > 0;
    }

    bool next() {
        return expect.get() != nullptr
            && expect.use_count() > 1
            && expect->pending.size()
            && expect->next();
    }
};

Prepared prepare_event_offsets(const datetime::Context& ctx, Span<const scheduler::Type> types) {
    Prepared out{
        .types = types,
        .event_offsets = {},
        .expect = {},
    };

    for (size_t index = 0; index < types.size(); ++index) {
        if (scheduler::Type::Relative != types[index]) {
            continue;
        }

        auto relative = settings::relative(index);
        if (!check_parsed(relative)) {
            continue;
        }

        if (Order::None == relative.order) {
            continue;
        }

        EventOffset tmp;
        tmp.index = index;
        tmp.order = relative.order;
        tmp.offset = relative.offset;

        if (Order::Before == relative.order) {
            tmp.offset = -tmp.offset;
        }

        switch (relative.type) {
        case Type::None:
            continue;

        case Type::Calendar:
            if (!out.expect) {
                out.expect = std::make_unique<expect::Context>(ctx);
            }
            tmp.source =
                std::make_unique<Calendar>(relative.data, out.expect);
            break;

        case Type::Named:
            tmp.source =
                std::make_unique<Named>(std::move(relative.name));
            break;

        case Type::Sunrise:
#if SCHEDULER_SUN_SUPPORT
            tmp.source = std::make_unique<Sunrise>();
            break;
#else
            continue;
#endif

        case Type::Sunset:
#if SCHEDULER_SUN_SUPPORT
            tmp.source = std::make_unique<Sunrise>();
            break;
#else
            continue;
#endif
        }

        if (tmp.source) {
            out.event_offsets.push_back(std::move(tmp));
        }
    }

    return out;
}

void handle_ordered(const datetime::Context& ctx, Prepared& prepared, Order order) {
    auto it = prepared.event_offsets.begin();
    while (it != prepared.event_offsets.end()) {
        if ((*it).order != order) {
            goto next;
        }

        switch (order) {
        case Order::None:
            goto next;

        case Order::Before:
            if (!(*it).source->before(ctx)) {
                goto erase;
            }
            goto next;

        case Order::After:
            if (!(*it).source->after(ctx)) {
                goto erase;
            }
            goto next;

        }

erase:
        it = prepared.event_offsets.erase(it);
        continue;

next:
        it = std::next(it);
        continue;
    }
}

void handle_before(const datetime::Context& ctx, Prepared& prepared) {
    handle_ordered(ctx, prepared, Order::Before);
    if (prepared.next()) {
        handle_ordered(ctx, prepared, Order::Before);
    }
}

void handle_after(const datetime::Context& ctx, Prepared& prepared) {
    handle_ordered(ctx, prepared, Order::After);
}

} // namespace relative

void handle_calendar(const datetime::Context& ctx, Span<const Type> types) {
    for (size_t index = 0; index < types.size(); ++index) {
        bool ok = false;

        switch (types[index]) {
        case Type::Unknown:
            return;

        case Type::Disabled:
        case Type::Relative:
            continue;

        case Type::Calendar:
            ok = check_calendar(ctx, index);
            break;
        }

        if (ok) {
            last_action(ctx, index);
            parse_action(settings::action(index));
        }
    }
}

void tick(NtpTick tick) {
    auto ctx = datetime::make_context(now());
    if (tick == NtpTick::EveryHour) {
        cleanup_last_actions(ctx);
        cleanup_named_events(ctx);
        return;
    }

    if (initial) {
        initial = false;
        settings::gc(settings::count());
        restore::run(ctx);
#if SCHEDULER_SUN_SUPPORT
        sun::update_before(ctx);
#endif
        settings::validate();
    }

#if SCHEDULER_SUN_SUPPORT
    sun::update_after(ctx);
#endif

    const auto types = settings::types();
    auto prepared =
        relative::prepare_event_offsets(ctx, make_span(types));

    if (prepared) {
        relative::handle_before(ctx, prepared);
        relative::process_valid_event_offsets(
            ctx, prepared.event_offsets, relative::Order::Before);
    }

    handle_calendar(ctx, prepared.types);

    if (prepared) {
        relative::handle_after(ctx, prepared);
        relative::process_valid_event_offsets(
            ctx, prepared.event_offsets, relative::Order::After);
    }
}

void setup() {
    migrateVersion(scheduler::settings::migrate);
    settings::setup();

    espurnaRegisterReload(settings::validate);

#if SCHEDULER_SUN_SUPPORT
    sun::setup();
#endif
#if TERMINAL_SUPPORT
    terminal::setup();
#endif
#if WEB_SUPPORT
    web::setup();
#endif
#if API_SUPPORT
    api::setup();
#endif
#if MQTT_SUPPORT
    mqtt::setup();
#endif

    ntpOnTick(tick);
}

} // namespace
} // namespace scheduler
} // namespace espurna

// -----------------------------------------------------------------------------

void schSetup() {
    espurna::scheduler::setup();
}

#endif // SCHEDULER_SUPPORT
