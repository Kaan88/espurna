/*

Part of the SYSTEM MODULE

Copyright (C) 2019-2021 by Maxim Prokhorov <prokhorov dot max at outlook dot com>

*/
#pragma once

#include <Arduino.h>
#include <sys/pgmspace.h>

#include <chrono>
#include <memory>

#include "compat.h"

// missing in our original header
extern "C" int memcmp_P(const void*, const void*, size_t);

namespace espurna {
namespace duration {

// Only micros are 64bit, millis stored as 32bit to match what is actually returned & used by Core functions
using Microseconds = std::chrono::duration<uint64_t, std::micro>;
using Milliseconds = std::chrono::duration<uint32_t, std::milli>;

// Our own helper types, a lot of things are based off of the `millis()`
// (and it can be seamlessly used with any Core functions accepting u32 millisecond inputs)
using rep_type = uint32_t;

using Seconds = std::chrono::duration<rep_type, std::ratio<1> >;
using Minutes = std::chrono::duration<rep_type, std::ratio<60> >;
using Hours = std::chrono::duration<rep_type, std::ratio<Minutes::period::num * 60> >;
using Days = std::chrono::duration<rep_type, std::ratio<Hours::period::num * 24> >;
using Weeks = std::chrono::duration<rep_type, std::ratio<Days::period::num * 7> >;

} // namespace duration

// base class for loop / oneshot / generic callbacks that do not need arguments
// *not expected* to be used instead of std function at all times.
// main purpose of this special class is to circumvent the need for rtti in
// our gcc stl implementation and retrieve the 'target function' pointer
// (*should* be different in gcc 11 / 12 though, target() became constexpr)
struct Callback {
    using Type = void (*)();
    using WrapperType = std::function<void()>;

    Callback() = default;

    Callback(const Callback& other) :
        _storage(nullptr),
        _type(other._type)
    {
        copy(other);
    }

    Callback& operator=(const Callback& other) {
        reset();
        copy(other);
        return *this;
    }

    Callback(const Callback&&) = delete;
    Callback(Callback&& other) noexcept :
        _storage(nullptr),
        _type(other._type)
    {
        move(other);
    }

    Callback& operator=(Callback&& other) noexcept;

    template <typename T>
    using is_callback = std::is_same<std::remove_cvref<T>, Callback>;

    template <typename T>
    using is_type = std::is_same<T, Type>;

    template <typename T>
    using type_convertible = std::is_convertible<T, Type>;

    template <typename T>
    using wrapper_convertible = std::is_convertible<T, WrapperType>;

    // when T *can* be converted into Callback::Type
    // usually, function pointer *or* lambda without capture list
    template <typename T,
              typename = typename std::enable_if<
                  is_type<T>::value
               || type_convertible<T>::value>::type>
    constexpr Callback(T callback) noexcept :
        _storage(Type(callback)),
        _type(StorageType::Simple)
    {}

    // anything else convertible into std function
    template <typename T,
              typename = typename std::enable_if<
                !is_callback<T>::value>::type,
              typename = typename std::enable_if<
                wrapper_convertible<T>::value>::type,
              typename = typename std::enable_if<
                !type_convertible<T>::value>::type>
    Callback(T callback) :
        _storage(WrapperType(std::move(callback))),
        _type(StorageType::Wrapper)
    {
        static_assert(!is_callback<T>::value, "");
    }

    ~Callback() {
        reset();
    }

    bool isEmpty() const {
        return (_type == StorageType::Empty);
    }

    bool isSimple() const {
        return (_type == StorageType::Simple);
    }

    bool isWrapped() const {
        return (_type == StorageType::Wrapper);
    }

    bool operator==(Type callback) const {
        return isSimple() && (_storage.simple == callback);
    }

    void reset();
    void swap(Callback&) noexcept;
    void operator()() const;

private:
    union Storage {
        WrapperType wrapper;
        Type simple;

        ~Storage() {
        }

        explicit Storage(WrapperType callback) :
            wrapper(std::move(callback))
        {}

        constexpr explicit Storage(Type callback) :
            simple(callback)
        {}

        constexpr explicit Storage(std::nullptr_t) :
            simple(nullptr)
        {}
    };

    enum class StorageType {
        Empty,
        Simple,
        Wrapper,
    };

    void copy(const Callback&);
    void move(Callback&) noexcept;

    Storage _storage { nullptr };
    StorageType _type { StorageType::Empty };
};

// aka `std::source_location`
struct SourceLocation {
    int line;
    const char* file;
    const char* func;
};

inline bool operator==(SourceLocation lhs, SourceLocation rhs) {
    return lhs.line == rhs.line
        && lhs.file == rhs.file
        && lhs.func == rhs.func;
}

inline SourceLocation trim_source_location(SourceLocation value) {
    for (auto* ptr = value.file; *ptr != '\0'; ++ptr) {
        if ((*ptr == '/') || (*ptr == '\\')) {
            value.file = ptr + 1;
        }
    }

    return value;
}

inline constexpr SourceLocation make_source_location(
        int line = __builtin_LINE(),
        const char* file = __builtin_FILE(),
        const char* func = __builtin_FUNCTION())
{
    return SourceLocation{
        .line = line,
        .file = file,
        .func = func
    };
}

// disallow re-locking, tracking external `bool`
struct ReentryLock {
    ReentryLock() = delete;

    ReentryLock(const ReentryLock&) = delete;
    ReentryLock& operator=(const ReentryLock&) = delete;

    ReentryLock(ReentryLock&&) = default;
    ReentryLock& operator=(ReentryLock&&) = delete;

    explicit ReentryLock(bool& handle) :
        _initialized(!handle),
        _handle(handle)
    {
        lock();
    }

    ~ReentryLock() {
        unlock();
    }

    explicit operator bool() const {
        return initialized();
    }

    bool initialized() const {
        return _initialized;
    }

    void lock() {
        if (initialized()) {
            _handle = true;
        }
    }

    void unlock() {
        if (initialized()) {
            _handle = false;
        }
    }
private:
    bool _initialized;
    bool& _handle;
};

struct StringView {
    constexpr StringView() noexcept :
        _ptr(nullptr),
        _len(0)
    {}

    ~StringView() = default;

    StringView(std::nullptr_t) = delete;

    constexpr StringView(const StringView&) noexcept = default;
    constexpr StringView(StringView&&) noexcept = default;

#if __cplusplus > 201103L
    constexpr StringView& operator=(const StringView&) noexcept = default;
    constexpr StringView& operator=(StringView&&) noexcept = default;
#else
    StringView& operator=(const StringView&) noexcept = default;
    StringView& operator=(StringView&&) noexcept = default;
#endif

    constexpr StringView(const char* ptr, size_t len) noexcept :
        _ptr(ptr),
        _len(len)
    {}

    template <typename T, typename = typename std::enable_if<std::is_pointer<T>::value>::type>
    constexpr StringView(T ptr) noexcept :
        StringView(ptr, __builtin_strlen(ptr))
    {}

    template <size_t Size>
    constexpr StringView(const char (&string)[Size]) noexcept :
        StringView(&string[0], Size - 1)
    {}

    constexpr StringView(const char* begin, const char* end) noexcept :
        StringView(begin, end - begin)
    {}

    explicit StringView(const __FlashStringHelper* ptr) noexcept :
        _ptr(reinterpret_cast<const char*>(ptr)),
        _len(strlen_P(_ptr))
    {}

    StringView(const String& string) noexcept :
        StringView(string.c_str(), string.length())
    {}

    StringView& operator=(const String& string) noexcept {
        _ptr = string.c_str();
        _len = string.length();
        return *this;
    }

    template <size_t Size>
    constexpr StringView& operator=(const char (&string)[Size]) noexcept {
        _ptr = &string[0];
        _len = Size - 1;
        return *this;
    }

    constexpr const char* begin() const noexcept {
        return _ptr;
    }

    constexpr const char* end() const noexcept {
        return _ptr + _len;
    }

    constexpr const char* c_str() const {
        return _ptr;
    }

    constexpr const char* data() const {
        return _ptr;
    }

    constexpr const char& operator[](size_t offset) const {
        return *(_ptr + offset);
    }

    constexpr size_t length() const {
        return _len;
    }

    String toString() const {
        String out;
        out.concat(_ptr, _len);
        return out;
    }

    explicit operator String() const {
        return toString();
    }

    bool equals(StringView) const;
    bool equalsIgnoreCase(StringView) const;

    bool startsWith(StringView) const;
    bool endsWith(StringView) const;

    StringView slice(size_t index, size_t end) const;
    StringView slice(size_t index) const;

private:
#if defined(HOST_MOCK)
    constexpr static bool inFlash(const char*) {
        return false;
    }
#else
    static bool inFlash(const char* ptr) {
        // common comparison would use >=0x40000000
        // instead, slightly reduce the footprint by
        // checking *only* for numbers below it
        static constexpr uintptr_t Mask { 1 << 30 };
        return (reinterpret_cast<uintptr_t>(ptr) & Mask) > 0;
    }
#endif

    const char* _ptr;
    size_t _len;
};

inline bool operator==(StringView lhs, StringView rhs) {
    return lhs.equals(rhs);
}

inline bool operator!=(StringView lhs, StringView rhs) {
    return !lhs.equals(rhs);
}

inline String operator+(String&& lhs, StringView rhs) {
    lhs.concat(rhs.c_str(), rhs.length());
    return lhs;
}

inline String operator+=(String& lhs, StringView rhs) {
    lhs.concat(rhs.c_str(), rhs.length());
    return lhs;
}

inline String operator+(StringView lhs, const String& rhs) {
    String out;
    out += lhs.toString();
    out += rhs;
    return out;
}

#if defined(ARDUINO_ESP8266_RELEASE_2_7_4)
inline String operator+(const String& lhs, const String& rhs) {
    String out;
    out.reserve(lhs.length() + rhs.length());

    out += lhs;
    out += rhs;

    return out;
}

inline String operator+(String&& lhs, const String& rhs) {
    String out(std::move(lhs));
    out += rhs;
    return out;
}

inline String operator+(const String& lhs, const __FlashStringHelper* rhs) {
    return lhs + String(rhs);
}

inline String operator+(String&& lhs, const __FlashStringHelper* rhs) {
    return std::move(lhs) + String(rhs);
}
#endif

#ifndef PROGMEM_STRING_ATTR
#define PROGMEM_STRING_ATTR __attribute__((section( "\".irom0.pstr." __FILE__ "." __STRINGIZE(__LINE__) "."  __STRINGIZE(__COUNTER__) "\", \"aSM\", @progbits, 1 #")))
#endif

#ifndef PROGMEM_STRING
#define PROGMEM_STRING(NAME, X)\
        alignas(4) static constexpr char NAME[] PROGMEM_STRING_ATTR = (X)
#endif

#ifndef STRING_VIEW
#define STRING_VIEW(X) ({\
        alignas(4) static constexpr char __pstr__[] PROGMEM_STRING_ATTR = (X);\
        ::espurna::StringView{__pstr__};\
    })
#endif

#ifndef STRING_VIEW_INLINE
#define STRING_VIEW_INLINE(NAME, X)\
        alignas(4) static constexpr char __pstr__ ## NAME ##  __ [] PROGMEM_STRING_ATTR = (X);\
        constexpr auto NAME = ::espurna::StringView(__pstr__ ## NAME ## __)
#endif

#define STRING_VIEW_SETTING(X)\
    ((__builtin_strlen(X) > 0) ? STRING_VIEW(X) : StringView())

template <typename T>
struct Span {
    constexpr explicit Span(std::nullptr_t) :
        _data(nullptr),
        _size(0)
    {}

    constexpr Span() noexcept :
        Span(nullptr)
    {}

    constexpr Span(T* data, size_t size) :
        _data(data),
        _size(size)
    {}

    constexpr Span(T* begin, T* end) :
        _data(begin),
        _size(end - begin)
    {}

    constexpr T* data() const {
        return _data;
    }

    constexpr bool is_null() const {
        return _data == nullptr;
    }

    constexpr bool empty() const {
        return is_null() || _size == 0;
    }

    constexpr size_t size() const {
        return _size;
    }

    constexpr T* begin() const {
        return _data;
    }

    constexpr T* end() const {
        return _data + _size;
    }

    constexpr T& operator[](size_t index) const {
        return _data[index];
    }

    constexpr T& front() const {
        return _data[0];
    }

    constexpr T& back() const {
        return _data[_size - 1];
    }

    constexpr Span<T> slice(size_t index, size_t size) const {
        return Span<T>(_data + std::min(index, _size), std::min(size, _size - index));
    }

    constexpr Span<T> slice(size_t index) const {
        return slice(index, _size - index);
    }

    Span<T>& advance(size_t index) {
        return *this = slice(index);
    }

private:
    T* _data;
    size_t _size;
};

template <size_t Size>
inline Span<uint8_t> make_span(uint8_t (&data)[Size]) {
    return Span<uint8_t>(&data[0], Size);
}

template <size_t Size>
inline Span<const uint8_t> make_span(const uint8_t (&data)[Size]) {
    return Span<const uint8_t>(&data[0], Size);
}

template <size_t Size>
inline Span<uint8_t> make_span(std::array<uint8_t, Size>& data) {
    return Span<uint8_t>(data.data(), data.size());
}

template <size_t Size>
inline Span<const uint8_t> make_span(const std::array<uint8_t, Size>& data) {
    return Span<const uint8_t>(data.data(), data.size());
}

template <typename T>
inline Span<T> make_span(std::vector<T>& data) {
    return Span<T>(data.data(), data.size());
}

template <typename T>
inline Span<const T> make_span(const std::vector<T>& data) {
    return Span<const T>(data.data(), data.size());
}

struct SplitStringView {
    explicit SplitStringView(StringView view) :
        _source(view)
    {}

    SplitStringView(StringView view, char delim) :
        _source(view),
        _delim(delim)
    {}

    StringView current() const {
        return _current;
    }

    StringView remaining() const {
        return _view;
    }

    bool next();

private:
    StringView _source;

    StringView _view { _source };
    char _delim { ' ' };

    StringView _current;
};

namespace duration {

struct Pair {
    Seconds seconds{};
    Microseconds microseconds{};
};

template <typename T, typename Rep = typename T::rep, typename Period = typename T::period>
std::chrono::duration<Rep, Period> to_chrono(Pair result) {
    using Type = std::chrono::duration<Rep, Period>;
    return std::chrono::duration_cast<Type>(result.seconds)
        + std::chrono::duration_cast<Type>(result.microseconds);
}

struct PairResult {
    Pair value;
    bool ok { false };
};

// Attempt to parse the given string with the specific ratio
// Same as chrono, std::ratio<1> is a second
PairResult parse(StringView, int num, int den);

template <intmax_t Num, intmax_t Den>
PairResult parse(StringView view, std::ratio<Num, Den>) {
    return parse(view, Num, Den);
}

template <typename T>
T unchecked_parse(StringView view) {
    const auto result = parse(view, typename T::period{});
    if (result.ok) {
        return to_chrono<T>(result.value);
    }

    return T{}.min();
}

} // namespace duration
} // namespace espurna
