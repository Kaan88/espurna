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

// scheduler itself has minutes precision, while seconds are used in debug and calculations
struct Event {
    datetime::Minutes minutes{ -1 };
    datetime::Seconds seconds{ -1 };
};

datetime::Seconds event_seconds(const Event& event) {
    return std::chrono::duration_cast<datetime::Seconds>(event.minutes) + event.seconds;
}

bool event_valid(const Event& event) {
    return (event.minutes > datetime::Minutes::zero())
        && (event.seconds > datetime::Seconds::zero());
}

struct EventMatch {
    datetime::Date date;
    TimeMatch time;
    Event last;
};

struct Match {
    EventMatch rising;
    EventMatch setting;
};

Location location;
Match match;

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

tm time_point_from_seconds(datetime::Seconds seconds) {
    tm out{};
    time_t timestamp{ seconds.count() };
    gmtime_r(&timestamp, &out);

    return out;
}

Event make_invalid_event() {
    Event out;

    out.seconds = datetime::Seconds{ -1 };
    out.minutes = datetime::Minutes{ -1 };

    return out;
}

Event make_event(datetime::Seconds seconds) {
    Event out;

    out.seconds = seconds;
    out.minutes =
        std::chrono::duration_cast<datetime::Minutes>(out.seconds);
    out.seconds -= out.minutes;

    return out;
}

datetime::Date date_point(const tm& time_point) {
    datetime::Date out;

    out.year = time_point.tm_year + 1900;
    out.month = time_point.tm_mon + 1;
    out.day = time_point.tm_mday;

    return out;
}

TimeMatch time_match(const tm& time_point) {
    TimeMatch out;

    out.hour[time_point.tm_hour] = true;
    out.minute[time_point.tm_min] = true;
    out.flags = FlagUtc;

    return out;
}

void update_event_match(EventMatch& match, datetime::Seconds seconds) {
    if (seconds <= datetime::Seconds::zero()) {
        match.last = make_invalid_event();
        return;
    }

    const auto time_point = time_point_from_seconds(seconds);
    match.date = date_point(time_point);
    match.time = time_match(time_point);

    match.last = make_event(seconds);
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
        return false;
    }

    // in case calculation failed, no use here
    if (!event_valid((*selected).last)) {
        return false;
    }

    // make sure event can actually trigger with this date spec
    if (::espurna::scheduler::match(schedule.date, (*selected).date)) {
        update_schedule_from(schedule, *selected);
        return true;
    }

    return false;
}

bool needs_update(datetime::Minutes minutes) {
    return ((match.rising.last.minutes < minutes)
         || (match.setting.last.minutes < minutes));
}

template <typename T>
void delta_compare(tm& out, datetime::Minutes, T);

void update(datetime::Minutes minutes, const tm& today) {
    const auto result = sun::sunrise_sunset(location, today);
    update_event_match(match.rising, result.sunrise);
    update_event_match(match.setting, result.sunset);
}

template <typename T>
void update(datetime::Minutes minutes, const tm& today, T compare) {
    auto result = sun::sunrise_sunset(location, today);
    if ((result.sunrise.count() < 0) || (result.sunset.count() < 0)) {
        return;
    }

    if (compare(minutes, result.sunrise) || compare(minutes, result.sunset)) {
        tm tmp;
        std::memcpy(&tmp, &today, sizeof(tmp));
        delta_compare(tmp, minutes, compare);

        const auto other = sun::sunrise_sunset(location, tmp);
        if ((other.sunrise.count() < 0) || (other.sunset.count() < 0)) {
            return;
        }

        if (compare(minutes, result.sunrise)) {
            result.sunrise = other.sunrise;
        }

        if (compare(minutes, result.sunset)) {
            result.sunset = other.sunset;
        }
    }

    update_event_match(match.rising, result.sunrise);
    update_event_match(match.setting, result.sunset);
}

template <typename T>
void update(time_t timestamp, const tm& today, T&& compare) {
    update(datetime::Seconds{ timestamp }, today, std::forward<T>(compare));
}

String format_match(const EventMatch& match) {
    return datetime::format_local_tz(
        datetime::make_context(event_seconds(match.last)));
}

// check() needs current or future events, discard timestamps in the past
// std::greater is type-fixed, make sure minutes vs. seconds actually works
struct CheckCompare {
    bool operator()(const datetime::Minutes& lhs, const datetime::Seconds& rhs) {
        return lhs > rhs;
    }
};

template <>
void delta_compare(tm& out, datetime::Minutes minutes, CheckCompare) {
    datetime::delta_utc(
        out, datetime::Seconds{ minutes },
        datetime::Days{ 1 });
}

void update_after(const datetime::Context& ctx) {
    const auto seconds = datetime::Seconds{ ctx.timestamp };
    const auto minutes =
        std::chrono::duration_cast<datetime::Minutes>(seconds);

    if (!needs_update(minutes)) {
        return;
    }

    update(minutes, ctx.utc, CheckCompare{});

    if (match.rising.last.minutes.count() > 0) {
        DEBUG_MSG_P(PSTR("[SCH] Sunrise at %s\n"),
            format_match(match.rising).c_str());
    }

    if (match.setting.last.minutes.count() > 0) {
        DEBUG_MSG_P(PSTR("[SCH] Sunset at %s\n"),
            format_match(match.setting).c_str());
    }
}

} // namespace sun
#endif

// -----------------------------------------------------------------------------

#if TERMINAL_SUPPORT
namespace terminal {

#if SCHEDULER_SUN_SUPPORT
namespace internal {

String sunrise_sunset(const sun::EventMatch& match) {
    if (match.last.minutes > datetime::Minutes::zero()) {
        return sun::format_match(match);
    }

    return STRING_VIEW("value not set").toString();
}

void format_output(::terminal::CommandContext& ctx, const String& prefix, const String& value) {
    ctx.output.printf_P(PSTR("%s%s%s\n"),
        prefix.c_str(),
        value.length()
            ? PSTR(" at ")
            : " ",
        value.c_str());
}

void dump_sunrise_sunset(::terminal::CommandContext& ctx) {
    format_output(ctx,
        STRING_VIEW("Sunrise").toString(),
        sunrise_sunset(sun::match.rising));
    format_output(ctx,
        STRING_VIEW("Sunset").toString(),
        sunrise_sunset(sun::match.setting));
}

} // namespace internal
#endif

PROGMEM_STRING(Dump, "SCHEDULE");

void dump(::terminal::CommandContext&& ctx) {
    if (ctx.argv.size() != 2) {
#if SCHEDULER_SUN_SUPPORT
        internal::dump_sunrise_sunset(ctx);
#endif
        settingsDump(ctx, settings::Settings);
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
#if SCHEDULER_SUN_SUPPORT
    wsPayloadModule(root, sun::Module);
#endif

    for (const auto& pair : settings::Settings) {
        root[pair.key()] = pair.value();
    }
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

namespace restore {

[[gnu::used]]
void Context::init() {
#if SCHEDULER_SUN_SUPPORT
    const auto seconds = datetime::Seconds{ this->current.timestamp };
    const auto minutes =
        std::chrono::duration_cast<datetime::Minutes>(seconds);

    sun::update(minutes, this->current.utc);
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
            if (handle_delta(ctx, *it)) {
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
            context_pending(ctx, index, schedule);
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

void sort(Context& ctx) {
    std::sort(
        ctx.results.begin(),
        ctx.results.end(),
        [](const Result& lhs, const Result& rhs) {
            return lhs.offset < rhs.offset;
        });
}

void run(const datetime::Context& base) {
    Context ctx{ base };

    run_today(ctx);
    run_delta(ctx);
    sort(ctx);

    for (auto& result : ctx.results) {
        const auto action = settings::action(result.index);
        DEBUG_MSG_P(PSTR("[SCH] Restoring #%zu => %s (%sm)\n"),
            result.index, action.c_str(),
            String(result.offset.count(), 10).c_str());
        parse_action(action);
    }
}

} // namespace restore

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

        auto schedule = settings::schedule(index);
        if (!schedule.ok) {
            continue;
        }

#if SCHEDULER_SUN_SUPPORT
        if (!sun::update_schedule(schedule)) {
            continue;
        }
#else
        if (want_sunrise_sunset(schedule.time)) {
            continue;
        }
#endif

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

    if (initial) {
        initial = false;
        settings::gc(settings::count());
        restore::run(ctx);
    }

#if SCHEDULER_SUN_SUPPORT
    sun::update_after(ctx);
#endif

    check(ctx);
}

void setup() {
    migrateVersion(scheduler::settings::migrate);
    settings::setup();

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
