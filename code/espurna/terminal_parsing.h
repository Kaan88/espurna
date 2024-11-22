/*

Part of the TERMINAL MODULE

Copyright (C) 2016-2019 by Xose Pérez <xose dot perez at gmail dot com>
Copyright (C) 2020 by Maxim Prokhorov <prokhorov dot max at outlook dot com>

*/

#pragma once

#include <Arduino.h>

#include <cstring>
#include <iterator>
#include <vector>

#include "types.h"

namespace espurna {
namespace terminal {

using Argv = std::vector<String>;

namespace parser {

enum class Error {
    Ok,
    Uninitialized,     // parser never started / no text
    Busy,              // parser was already parsing something
    UnterminatedQuote, // parsing stopped without terminating a quoted entry
    NoSpaceAfterQuote, // parsing stopped since there was no space after quote
    InvalidEscape,     // escaped text was invalid
    UnexpectedLineEnd, // unexpected \r encounteted in the input
};

String error(Error);

} // namespace parser

struct CommandLine {
    Argv argv;
    parser::Error error;
};

// Type wrapper that flushes output on finding '\n'.
// Inherit from `String` to allow us to manage internal buffer directly.
// (and also seamlessly behave like a usual `String`, we import most of its methods)
template <typename T>
class PrintLine final : public Print, public String {
private:
    static_assert(!std::is_reference<T>::value, "");

    using String::wbuffer;
    using String::buffer;
    using String::setLen;

public:
    using String::begin;
    using String::end;
    using String::reserve;
    using String::c_str;
    using String::length;

    PrintLine() = default;
    ~PrintLine() {
        send();
    }

    template <typename... Args>
    PrintLine(Args&&... args) :
        _output(std::forward<Args>(args)...)
    {}

    T& output() {
        return _output;
    }

    void flush() override {
        if (end() != std::find(begin(), end(), '\n')) {
            send();
            setLen(0);
            *wbuffer() = '\0';
        }
    }

    size_t write(const uint8_t* data, size_t size) override {
        if (!_lock && size && data && *data != '\0') {
            ReentryLock lock(_lock);

            concat(reinterpret_cast<const char*>(data), size);
            flush();

            return size;
        }

        return 0;
    }

    size_t write(uint8_t ch) override {
        return write(&ch, 1);
    }

private:
    void send() {
        _output(buffer());
    }

    T _output;
    bool _lock { false };
};

// Generic command line parser
// - `argv` array contains copies or every 'split' string found in the source line
//   (usual `argc` is expected to be equal to the `argv.size()`)
// - `error` set to any parser errors encountered, or `Ok` when everything is fine
CommandLine parse_line(StringView line);

} // namespace terminal
} // namespace espurna
