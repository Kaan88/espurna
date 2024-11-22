#pragma once

#include "../types.h"

#include <array>

namespace espurna {

// Buffer char data and yield portion of the data when the received value has specific char
// storage works like a circular buffer; whenever buffer size exceedes capacity, we return
// to the start of the buffer and reset size.
// when buffer overflows, store internal flag until the storage is reset to the default state
struct DelimiterBuffer {
    // **only valid until the next append()**
    struct Result {
        StringView value;
        bool overflow;
    };

    DelimiterBuffer() = delete;

    explicit DelimiterBuffer(char* storage, size_t capacity, char delim) :
        _storage(storage),
        _capacity(capacity),
        _delimiter(delim)
    {}

    Result next();

    void reset() {
        _overflow = false;
        _cursor = 0;
        _size = 0;
    }

    size_t capacity() const {
        return _capacity;
    }

    size_t size() const {
        return _size;
    }

    bool overflow() const {
        return _overflow;
    }

    void append(const char*, size_t);

    void append(StringView value) {
        append(value.c_str(), value.length());
    }

    void append(Stream&, size_t);

    void append(Stream& stream) {
        const auto available = stream.available();
        if (available > 0) {
            append(stream, static_cast<size_t>(available));
        }
    }

    void append(char value) {
        append(&value, 1);
    }

private:
    char* _storage;
    size_t _capacity { 0 };
    size_t _size { 0 };
    size_t _cursor { 0 };

    char _delimiter;

    bool _overflow { false };
};

template <size_t Capacity>
struct LineBuffer {
    using Result = DelimiterBuffer::Result;

    LineBuffer() :
        LineBuffer('\n')
    {}

    explicit LineBuffer(char delimiter) :
        _buffer(_storage.data(), _storage.size(), delimiter)
    {}

    Result next() {
        return _buffer.next();
    }

    void reset() {
        _buffer.reset();
    }

    static constexpr size_t capacity() {
        return Capacity;
    }

    size_t size() const {
        return _buffer.size();
    }

    bool overflow() const {
        return _buffer.overflow();
    }

    void append(const char* data, size_t length) {
        _buffer.append(data, length);
    }

    void append(StringView value) {
        _buffer.append(value);
    }

    void append(Stream& stream, size_t length) {
        _buffer.append(stream, length);
    }

    void append(Stream& stream) {
        _buffer.append(stream);
    }

    void append(char value) {
        _buffer.append(value);
    }

private:
    using Storage = std::array<char, Capacity>;
    Storage _storage{};

    DelimiterBuffer _buffer;
};

// Similar to delimited buffer, but instead work on an already existing string
// and yield these stringview chunks on each call to next()
struct DelimiterView {
    DelimiterView() = delete;

    explicit DelimiterView(StringView view, char delimiter) :
        _view(view),
        _delimiter(delimiter)
    {}

    StringView next();

    explicit operator bool() const {
        return _cursor != _view.length();
    }

    const char* begin() const {
        return _view.begin() + _cursor;
    }

    const char* end() const {
        return _view.end();
    }

    size_t length() const {
        return std::distance(begin(), end());
    }

    StringView get() const {
        return StringView{begin(), end()};
    }

private:
    StringView _view;
    char _delimiter;

    size_t _cursor { 0 };
};

struct LineView : public DelimiterView {
    explicit LineView(StringView view) :
        DelimiterView(view, '\n')
    {}
};

} // namespace espurna
