/*

Part of the TERMINAL MODULE

Copyright (C) 2016-2019 by Xose Pérez <xose dot perez at gmail dot com>
Copyright (C) 2020 by Maxim Prokhorov <prokhorov dot max at outlook dot com>

*/

#include <vector>
#include <cctype>

#include "terminal_parsing.h"
#include "libs/Delimiter.h"

namespace espurna {
namespace terminal {
namespace parser {

String error(Error value) {
    StringView out;

    switch (value) {
    case Error::Ok:
        out = STRING_VIEW("Ok");
        break;
    case Error::Uninitialized:
        out = STRING_VIEW("Uninitialized");
        break;
    case Error::Busy:
        out = STRING_VIEW("Busy");
        break;
    case Error::UnterminatedQuote:
        out = STRING_VIEW("UnterminatedQuote");
        break;
    case Error::InvalidEscape:
        out = STRING_VIEW("InvalidEscape");
        break;
    case Error::UnexpectedLineEnd:
        out = STRING_VIEW("UnexpectedLineEnd");
        break;
    case Error::NoSpaceAfterQuote:
        out = STRING_VIEW("NoSpaceAfterQuote");
        break;
    }

    return out.toString();
}

namespace {

// Original code is part of the SDSLib 2.0 -- A C dynamic strings library
// - https://github.com/antirez/sds/blob/master/sds.c
// - https://github.com/antirez/redis/blob/unstable/src/networking.c
// Replaced with a stateful parser to avoid random look-ahead issues in the code,
// and really make sure we **never** go out of bounds of the given view.
// (e.g. when we want to parse only a part of a larger string)

// Helper functions to handle \xHH codes that could encode
// non-printable characters for commands or arguments
bool is_hex_digit(char c) {
    switch (c) {
    case '0' ... '9':
    case 'a' ... 'f':
    case 'A' ... 'F':
        return true;
    }

    return false;
}

char hex_digit_to_byte(char c) {
    switch (c) {
    case '0'...'9':
        return c - '0';
    case 'a':
    case 'A':
        return 10;
    case 'b':
    case 'B':
        return 11;
    case 'c':
    case 'C':
        return 12;
    case 'd':
    case 'D':
        return 13;
    case 'e':
    case 'E':
        return 14;
    case 'f':
    case 'F':
        return 15;
    }

    return 0;
}

char hex_digit_to_value(char lhs, char rhs) {
    return (hex_digit_to_byte(lhs) << 4) | hex_digit_to_byte(rhs);
}

// allowed 'special' input characters
char unescape_char(char c) {
    switch (c) {
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case 't':
        return '\t';
    case 'b':
        return '\b';
    case 'a':
        return '\a';
    }

    return c;
}

struct Result {
    Result() = default;

    Result& operator=(Error error) {
        _error = error;
        _argv.clear();
        return *this;
    }

    Result& operator=(Argv&& argv) {
        _argv = std::move(argv);
        _error = Error::Ok;
        return *this;
    }

    explicit operator bool() const {
        return _error == Error::Ok;
    }

    Error error() const {
        return _error;
    }

    CommandLine get() {
        auto out = CommandLine{
            .argv = std::move(_argv),
            .error = _error };

        _error = Error::Uninitialized;
        return out;
    }

private:
    Error _error { Error::Uninitialized };
    Argv _argv;
};

struct Parser {
    Parser() = default;
    Result operator()(StringView);

private:
    // only tracked within our `operator()(<LINE>)`
    enum class State {
        Done,
        Initial,
        Text,
        CarriageReturn,
        CarriageReturnAfterText,
        SkipUntilNewLine,
        EscapedText,
        EscapedByteLhs,
        EscapedByteRhs,
        SingleQuote,
        EscapedQuote,
        DoubleQuote,
        AfterQuote,
    };

    // our storage for
    // - ARGV resulting list
    // - text buffer or (interim) text span / range
    // - escaped character (since we don't look ahead when iterating)
    struct Values {
        struct Span {
            const char* begin { nullptr };
            const char* end { nullptr };
        };

        Span span;
        String chunk;
        char byte_lhs { 0 };

        Argv argv;

        void append_span(const char* ptr) {
            if (!span.begin) {
                span.begin = ptr;
            }

            span.end = !span.end
                ? std::next(span.begin)
                : std::next(ptr);
        }

        void push_span() {
            if (span.begin && span.end) {
                StringView view(span.begin, span.end);
                chunk.concat(view.c_str(), view.length());
                span = Values::Span{};
            }
        }

        void append_chunk(char c) {
            push_span();
            chunk.concat(&c, 1);
        }

        void append_byte_lhs(char c) {
            byte_lhs = c;
        }

        void append_byte_rhs(char c) {
            append_chunk(hex_digit_to_value(byte_lhs, c));
        }

        void push_chunk() {
            push_span();
            argv.push_back(chunk);
            chunk = "";
        }
    };

    bool _parsing { false };
};

Result Parser::operator()(StringView line) {
    Result result;
    Values values;

    State state { State::Initial };

    ReentryLock lock(_parsing);
    if (!lock.initialized()) {
        result = Error::Busy;
        goto out;
    }

    for (auto it = line.begin(); it != line.end(); ++it) {
        switch (State(state)) {
        case State::Initial:
            switch (*it) {
            case ' ':
            case '\t':
                break;
            case '\r':
                state = State::CarriageReturn;
                break;
            case '\n':
                state = State::Done;
                break;
            default:
                state = State::Text;
                goto text;
            }
            break;

        case State::Done:
            goto out;

        case State::Text:
text:
            switch (*it) {
            case ' ':
            case '\t':
                values.push_chunk();
                state = State::Initial;
                break;
            case '"':
                state = State::DoubleQuote;
                break;
            case '\'':
                state = State::SingleQuote;
                break;
            case '\r':
                state = State::CarriageReturnAfterText;
                break;
            case '\n':
                values.push_chunk();
                state = State::Done;
                break;
            default:
                values.append_span(it);
                break;
            }
            break;

        case State::CarriageReturn:
            if ((*it) == '\n') {
                state = State::Done;
            } else {
                result = Error::UnexpectedLineEnd;
                goto out;
            }
            break;

        case State::CarriageReturnAfterText:
            if ((*it) == '\n') {
                values.push_chunk();
                state = State::Done;
            } else {
                result = Error::UnexpectedLineEnd;
                goto out;
            }
            break;

        case State::SkipUntilNewLine:
            switch (*it) {
            case '\r':
                state = State::CarriageReturn;
                break;
            case '\n':
                state = State::Initial;
                break;
            }
            break;

        case State::EscapedText: {
            switch (*it) {
            case '\r':
            case '\n':
                result = Error::UnexpectedLineEnd;
                goto out;
            case 'x':
                state = State::EscapedByteLhs;
                break;
            default:
                values.append_chunk(unescape_char(*it));
                break;
            }
            break;
        }

        case State::EscapedByteLhs:
            if (is_hex_digit(*it)) {
                values.append_byte_lhs(*it);
                state = State::EscapedByteRhs;
            } else {
                result = Error::InvalidEscape;
                goto out;
            }
            break;

        case State::EscapedByteRhs:
            if (is_hex_digit(*it)) {
                values.append_byte_rhs(*it);
                state = State::DoubleQuote;
            } else {
                result = Error::InvalidEscape;
                goto out;
            }
            break;

        case State::SingleQuote:
            switch (*it) {
            case '\r':
            case '\n':
                result = Error::UnterminatedQuote;
                goto out;
            case '\\':
                state = State::EscapedQuote;
                break;
            case '\'':
                state = State::AfterQuote;
                break;
            default:
                values.append_span(it);
                break;
            }
            break;

        case State::EscapedQuote:
            switch (*it) {
            case '\'':
                values.chunk.concat(*it);
                state = State::SingleQuote;
                break;
            default:
                result = Error::InvalidEscape;
                goto out;
            }
            break;

        case State::AfterQuote:
            switch (*it) {
            case '\r':
                state = State::CarriageReturnAfterText;
                break;
            case ' ':
            case '\t':
                values.push_chunk();
                state = State::Initial;
                break;
            case '\n':
                values.push_chunk();
                state = State::Done;
                break;
            default:
                result = Error::NoSpaceAfterQuote;
                goto out;
            }
            break;

        case State::DoubleQuote:
            switch (*it) {
            case '\r':
            case '\n':
                result = Error::UnterminatedQuote;
                goto out;
            case '"':
                state = State::AfterQuote;
                break;
            case '\\':
                state = State::EscapedText;
                break;
            default:
                values.append_span(it);
                break;
            }
            break;

        }
    }

out:
    if (state == State::Done) {
        result = std::move(values.argv);
    }

    // whenever line ends before we are done parsing, make sure
    // result contains a valid error condition (same as in the switch above)
    if (result.error() == Error::Uninitialized) {
        switch (state) {
        case State::Done:
            break;
        case State::CarriageReturn:
        case State::CarriageReturnAfterText:
        case State::Text:
        case State::Initial:
        case State::SkipUntilNewLine:
            result = Error::UnexpectedLineEnd;
            break;
        case State::EscapedByteLhs:
        case State::EscapedByteRhs:
        case State::EscapedText:
        case State::EscapedQuote:
            result = Error::InvalidEscape;
            break;
        case State::SingleQuote:
        case State::DoubleQuote:
            result = Error::UnterminatedQuote;
            break;
        case State::AfterQuote:
            result = Error::NoSpaceAfterQuote;
            break;
        }
    }

    return result;
}

CommandLine parse_line(StringView line) {
    static Parser parser;
    return parser(line).get();
}

} // namespace

// Fowler–Noll–Vo hash function to hash command strings that treats input as lowercase
// ref: https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
//
// This is here in case `std::unordered_map` becomes viable
// TODO: afaik, map implementation should handle collisions (however rare they are in our case)
// if not, we can always roll static commands allocation and just match strings with strcmp_P

uint32_t lowercase_fnv1_hash(StringView value) {
    constexpr uint32_t fnv_prime = 16777619u;
    constexpr uint32_t fnv_basis = 2166136261u;

    uint32_t hash = fnv_basis;
    for (auto it = value.begin(); it != value.end(); ++it) {
        hash = hash ^ static_cast<uint32_t>(tolower(pgm_read_byte(it)));
        hash = hash * fnv_prime;
    }

    return hash;
}

} // namespace parser

CommandLine parse_line(StringView value) {
    return parser::parse_line(value);
}

} // namespace terminal

DelimiterBuffer::Result DelimiterBuffer::next() {
    const auto begin = &_storage[_cursor];
    const auto end = &_storage[_size];

    if (begin != end) {
        const auto eol = std::find(begin, end, _delimiter);
        if (eol != end) {
            const auto after = std::next(eol);
            const auto out = Result{
                .value = StringView{begin, after},
                .overflow = _overflow };

            if (after != end) {
                _cursor = std::distance(&_storage[0], after);
            } else {
                reset();
            }

            return out;
        }
    }

    return Result{
        .value = StringView(),
        .overflow = _overflow };
}

void DelimiterBuffer::append(const char* data, size_t length) {
    // adjust pointer and length when they immediatelly cause overflow
    auto output = &_storage[_size];

    auto capacity = _capacity - _size;
    while (length > capacity) {
        data += capacity;
        length -= capacity;
        capacity = _capacity;
        output = &_storage[0];
        _size = 0;
        _overflow = true;
    }

    if (length) {
        std::memcpy(output, data, length);
        _size += length;
    }
}

void DelimiterBuffer::append(Stream& stream, size_t length) {
    auto output = &_storage[_size];
    auto capacity = _capacity - _size;

    while (length > capacity) {
        printf("%zu vs. %zu\n", length, capacity);
        const auto chunk = std::min(_capacity - _size, length);

#if defined(ARDUINO_ESP8266_RELEASE_2_7_2) \
|| defined(ARDUINO_ESP8266_RELEASE_2_7_3) \
|| defined(ARDUINO_ESP8266_RELEASE_2_7_4)
        stream.readBytes(_storage, chunk);
#else
        stream.peekConsume(chunk);
#endif

        length -= capacity;
        capacity = _capacity;
        output = &_storage[0];

        _size = 0;
        _overflow = true;
    }

    if (length) {
        stream.readBytes(output, length);
        _size += length;
    }
}

StringView DelimiterView::next() {
    const auto Begin = begin();
    const auto End = end();

    if (Begin != End) {
        const auto eol = std::find(Begin, End, _delimiter);
        if (eol != End) {
            const auto after = std::next(eol);
            if (after != End) {
                _cursor = std::distance(_view.begin(), after);
            } else {
                _cursor = _view.length();
            }

            return StringView{Begin, after};
        }
    }

    return StringView();
}

} // namespace espurna
