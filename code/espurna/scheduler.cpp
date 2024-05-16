/*

SCHEDULER MODULE

Copyright (C) 2017 by faina09
Adapted by Xose Pérez <xose dot perez at gmail dot com>

Copyright (C) 2019-2024 by Maxim Prokhorov <prokhorov dot max at outlook dot com>

*/

#include "espurna.h"

#if SCHEDULER_SUPPORT

#include "api.h"
#include "datetime.h"
#include "light.h"
#include "mqtt.h"
#include "ntp.h"
#include "ntp_timelib.h"
#include "curtain_kingart.h"
#include "relay.h"
#include "scheduler.h"
#include "ws.h"

#include "libs/EphemeralPrint.h"
#include "libs/PrintString.h"

#include <bitset>

// -----------------------------------------------------------------------------

#include "scheduler_common.ipp"
#include "scheduler_time.re.ipp"

#if SCHEDULER_SUN_SUPPORT
#include "scheduler_sun.ipp"
#endif

namespace espurna {
namespace scheduler {

// TODO recurrent in addition to calendar?
enum class Type : int {
    Unknown = 0,
    Disabled,
    Calendar,
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

#if SCHEDULER_SUN_SUPPORT
namespace sun {

struct Match {
    TimeMatch m;
    time_t timestamp { -1 };
};

Location location;

Match match_rising;
Match match_setting;

void setup();

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
    return false;
}

constexpr int restoreDays() {
    return - (SCHEDULER_RESTORE_DAYS); 
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

static constexpr std::array<Enumeration<Type>, 3> Types PROGMEM {
    {{Type::Unknown, Unknown},
     {Type::Disabled, Disabled},
     {Type::Calendar, Calendar}}
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

struct Schedule {
    DateMatch date;
    WeekdayMatch weekdays;
    TimeMatch time;

    bool ok { false };
};

} // namespace
} // namespace scheduler
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

} // namespace internal

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
    Schedule out;

    bool parsed_date { false };
    bool parsed_weekdays { false };
    bool parsed_time { false };

    const auto time = settings::time(index);
    auto split = SplitStringView(time);

    while (split.next()) {
        auto elem = split.current();

        // most expected order, starting with date
        if (!parsed_date && ((parsed_date = parse_date(out.date, elem)))) {
            continue;
        }

        // then weekdays
        if (!parsed_weekdays && ((parsed_weekdays = parse_weekdays(out.weekdays, elem)))) {
            continue;
        }

        // then time
        if (!parsed_time && ((parsed_time = parse_time(out.time, elem)))) {
            continue;
        }

        // and keyword is always at the end. forcibly stop the parsing, regardless of the state
        if (parse_time_keyword(out.time, elem)) {
            if (want_utc(out.time)) {
                break;
            }

#if SCHEDULER_SUN_SUPPORT
            // do not want both time and sun{rise,set}
            if (want_sunrise_sunset(out.time)) {
                parsed_time = !parsed_time;
            }
#endif

            break;
        }
    }

    out.ok = parsed_date
        || parsed_weekdays
        || parsed_time;

    return out;
}

size_t count() {
    size_t out { 0 };

    for (size_t index = 0; index < build::max(); ++index) {
        const auto type = settings::type(index);
        if (type == Type::Unknown) {
            break;
        }

        ++out;
    }

    return out;
}

void gc(size_t total) {
    for (size_t index = total; index < build::max(); ++index) {
        for (auto setting : IndexedSettings) {
            delSetting({setting.prefix(), index});
        }
    }
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

} // namespace settings

String convert_time(const String& weekdays, int hour, int minute) {
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
                settings::minute(index)));

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

void setup() {
    location.latitude = settings::latitude();
    location.longitude = settings::longitude();
    location.altitude = settings::altitude();
}

void update_match(Match& match, time_t timestamp) {
    tm tmp{};
    gmtime_r(&timestamp, &tmp);

    match.timestamp = timestamp;

    match.m.hour.reset();
    match.m.hour[tmp.tm_hour] = true;

    match.m.minute.reset();
    match.m.minute[tmp.tm_min] = true;

    match.m.flags = FlagUtc;
}

// keep existing sunrise and sunset timestamp for at least a minute
bool needs_update(time_t timestamp) {
    return ((match_rising.timestamp < (timestamp + 60))
         || (match_setting.timestamp < (timestamp + 60)));
}

template <typename T>
void delta_compare(tm& out, time_t, T);

template <>
void delta_compare(tm& out, time_t timestamp, std::greater<time_t>) {
    datetime::delta_utc(
        out, datetime::Seconds{ timestamp },
        datetime::Days{ 1 });
}

template <>
void delta_compare(tm& out, time_t timestamp, std::less<time_t>) {
    datetime::delta_utc(
        out, datetime::Seconds{ timestamp },
        datetime::Days{ -1 });
}

template <typename T>
void update(time_t timestamp, const tm& today, T compare) {
    auto result = sun::sunrise_sunset(location, today);
    if ((result.sunrise < 0) || (result.sunset < 0)) {
        DEBUG_MSG_P(PSTR("[SCH] Sunrise and sunset cannot be calculated\n"));
        return;
    }

    if (compare(timestamp, result.sunrise) || compare(timestamp, result.sunset)) {
        tm tmp;
        std::memcpy(&tmp, &today, sizeof(tmp));
        delta_compare(tmp, timestamp, compare);

        const auto other = sun::sunrise_sunset(location, tmp);
        if ((other.sunrise < 0) || (other.sunset < 0)) {
            DEBUG_MSG_P(PSTR("[SCH] Sunrise and sunset cannot be calculated\n"));
            return;
        }

        if (compare(timestamp, result.sunrise)) {
            result.sunrise = other.sunrise;
        }

        if (compare(timestamp, result.sunset)) {
            result.sunset = other.sunset;
        }
    }

    update_match(match_rising, result.sunrise);
    update_match(match_setting, result.sunset);
}

// restoration needs timestamps in the past, ensure sun matchers are before current timestamp
void before_restore(const datetime::Context& ctx) {
    update(ctx.timestamp, ctx.utc, std::less<time_t>{});

    if (match_rising.timestamp > 0) {
        DEBUG_MSG_P(PSTR("[SCH] Previous sunrise at %s\n"),
            datetime::format_local(match_rising.timestamp).c_str());
    }

    if (match_setting.timestamp > 0) {
        DEBUG_MSG_P(PSTR("[SCH] Previous sunset at %s\n"),
            datetime::format_local(match_setting.timestamp).c_str());
    }
}

void after_restore() {
    match_rising = Match{};
    match_setting = Match{};
}

// while check needs timestamps in the future
void before_check(const datetime::Context& ctx) {
    if (!needs_update(ctx.timestamp)) {
        return;
    }

    update(ctx.timestamp, ctx.utc, std::greater<time_t>{});

    if (match_rising.timestamp > 0) {
        DEBUG_MSG_P(PSTR("[SCH] Sunrise at %s\n"),
            datetime::format_local(match_rising.timestamp).c_str());
    }

    if (match_setting.timestamp > 0) {
        DEBUG_MSG_P(PSTR("[SCH] Sunset at %s\n"),
            datetime::format_local(match_setting.timestamp).c_str());
    }
}

} // namespace sun
#endif

// -----------------------------------------------------------------------------

#if TERMINAL_SUPPORT
namespace terminal {

PROGMEM_STRING(Dump, "SCHEDULE");

static void dump(::terminal::CommandContext&& ctx) {
    if (ctx.argv.size() != 2) {
        terminalError(ctx, STRING_VIEW("SCHEDULE <ID>").toString());
        return;
    }

    size_t id;
    if (!tryParseId(ctx.argv[1], id)) {
        terminalError(ctx, F("Invalid ID"));
        return;
    }

    settingsDump(ctx, settings::IndexedSettings, id);
    terminalOK(ctx);
}

static constexpr ::terminal::Command Commands[] PROGMEM {
    {Dump, dump},
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

} // namespace schedule

void setup() {
    apiRegister(F(MQTT_TOPIC_SCHEDULE), schedules::get, schedules::set);
    apiRegister(F(MQTT_TOPIC_SCHEDULE "/+"), schedule::get, schedule::set);
}

} // namespace api
#endif  // API_SUPPORT

// -----------------------------------------------------------------------------

#if WEB_SUPPORT
namespace web {

bool onKey(StringView key, const JsonVariant&) {
    return key.startsWith(settings::Prefix);
}

void onVisible(JsonObject& root) {
    wsPayloadModule(root, settings::Prefix);
}

void onConnected(JsonObject& root){
    espurna::web::ws::EnumerableConfig config{ root, STRING_VIEW("schConfig") };
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
    }
}

#endif

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

duration::Minutes to_minutes(int hour, int minute) {
    return duration::Hours{ hour } + duration::Minutes{ minute };
}

duration::Minutes to_minutes(const tm& t) {
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
    tmp.tm_sec = 59;

    const auto result = closest_delta(out, lhs, tmp);
    if (result) {
        out -= datetime::Minutes{ 1 };
        return true;
    }

    return false;
}

const tm& select_time(const datetime::Context& ctx, const Schedule& schedule) {
    return want_utc(schedule.time)
        ? ctx.utc
        : ctx.local;
}

Schedule load_schedule(size_t index) {
    auto schedule = settings::schedule(index);

#if SCHEDULER_SUN_SUPPORT
    if (schedule.ok) {
        if (want_sunrise(schedule.time)) {
            schedule.time = sun::match_rising.m;
        } else if (want_sunset(schedule.time)) {
            schedule.time = sun::match_setting.m;
        }
    }
#endif

    return schedule;
}

struct Restore {
    size_t index;
    datetime::Minutes offset;
    String action;
};

void update_restore(std::vector<Restore>& out, size_t index, datetime::Minutes offset) {
    out.push_back(
        Restore{
            .index = index,
            .offset = offset,
            .action = settings::action(index),
        });
}

std::vector<Restore> prepare_restore(const datetime::Context& today, int extra_days) {
    std::vector<Restore> out;

    for (size_t index = 0; index < build::max(); ++index) {
        switch (settings::type(index)) {
        case Type::Unknown:
            goto return_out;

        case Type::Disabled:
            continue;

        case Type::Calendar:
            break;
        }

        if (!settings::restore(index)) {
            continue;
        }

        const auto schedule = load_schedule(index);
        if (!schedule.ok) {
            break;
        }

        // In case today was a match, store earliest possible execution time as well as the action itself
        const auto& today_time = select_time(today, schedule);

        if (match(schedule.date, today_time) && match(schedule.weekdays, today_time)) {
            datetime::Minutes offset{};
            if (closest_delta(offset, schedule.time, today_time)) {
                update_restore(out, index, offset);
                continue;
            }
        }

        // Otherwise, check additional number of days
        datetime::Minutes offset{};

        for (int day = 0; day < extra_days; ++day) {
            const auto extra = datetime::delta(today, datetime::Days{ -1 - day });

            const auto& time = select_time(extra, schedule);
            if (match(schedule.date, time) && match(schedule.weekdays, time)) {
                if (closest_delta_end_of_day(offset, schedule.time, time)) {
                    offset -= to_minutes(today_time);
                    update_restore(out, index, offset);
                }
            }

            offset -= datetime::Days{ 1 };
        }
    }

return_out:
    return out;
}

void restore(const datetime::Context& ctx) {
    // pending schedules, in .offset ascending order so oldest ones are executed first
    auto restored = prepare_restore(ctx, settings::restoreDays());
    std::sort(
        restored.begin(),
        restored.end(),
        [](const Restore& lhs, const Restore& rhs) {
            return lhs.offset < rhs.offset;
        });

    for (auto& restore : restored) {
        DEBUG_MSG_P(PSTR("[SCH] Restoring #%zu => %s (%sm)\n"),
            restore.index, restore.action.c_str(),
            String(restore.offset.count(), 10).c_str());
        parse_action(restore.action);
    }
}

void check(const datetime::Context& ctx) {
    for (size_t index = 0; index < build::max(); ++index) {
        switch (settings::type(index)) {
        case Type::Unknown:
            return;

        case Type::Disabled:
            continue;

        case Type::Calendar:
            break;
        }

        auto schedule = load_schedule(index);
        if (!schedule.ok) {
            continue;
        }

        const auto& time = select_time(ctx, schedule);

        if (!match(schedule.date, time)) {
            continue;
        }

        if (!match(schedule.weekdays, time)) {
            continue;
        }

        if (!match(schedule.time, time)) {
            continue;
        }

        DEBUG_MSG_P(PSTR("[SCH] Action #%zu triggered\n"), index);
        parse_action(settings::action(index));
    }
}

void tick(NtpTick tick) {
    if (tick != NtpTick::EveryMinute) {
        return;
    }

    auto ctx = datetime::make_context(now());

    auto count = settings::count();
    if (initial) {
        DEBUG_MSG_P(PSTR("[SCH] Registered %zu schedule(s)\n"), count);

        settings::gc(count);

#if SCHEDULER_SUN_SUPPORT
        sun::before_restore(ctx);
#endif

        restore(ctx);

#if SCHEDULER_SUN_SUPPORT
        sun::after_restore();
#endif

        initial = false;
    }

#if SCHEDULER_SUN_SUPPORT
    sun::before_check(ctx);
#endif
    check(ctx);
}

void setup() {
    migrateVersion(scheduler::settings::migrate);

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
