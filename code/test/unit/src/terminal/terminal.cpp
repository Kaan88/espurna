#include <unity.h>
#include <Arduino.h>
#include <StreamString.h>

#include <espurna/libs/Delimiter.h>
#include <espurna/libs/PrintString.h>
#include <espurna/terminal_commands.h>

namespace espurna {
namespace terminal {
namespace test {
namespace {

// Default 'print nothing' case
struct BlackHole : public Print {
    size_t write(uint8_t) override {
        return 0;
    }

    size_t write(const uint8_t*, size_t) override {
        return 0;
    }
};

BlackHole DefaultOutput;

// We need to make sure that our changes to split_args actually worked
void test_hex_codes() {
    {
        const char input[] = "abc \"\\x";
        const auto result = parse_line(input);
        TEST_ASSERT_EQUAL_STRING("InvalidEscape", parser::error(result.error).c_str());
    }

    {
        const char input[] = "abc \"\\x5";
        const auto result = parse_line(input);
        TEST_ASSERT_EQUAL_STRING("InvalidEscape", parser::error(result.error).c_str());
    }

    {
        static bool abc_done = false;

        add("abc", [](CommandContext&& ctx) {
            TEST_ASSERT_EQUAL(2, ctx.argv.size());
            TEST_ASSERT_EQUAL_STRING("abc", ctx.argv[0].c_str());
            TEST_ASSERT_EQUAL_STRING("abc", ctx.argv[1].c_str());
            abc_done = true;
        });

        const char input[] = "abc \"\\x61\\x62\\x63\"\r\n";

        const auto result = parse_line(input);
        TEST_ASSERT_EQUAL(2, result.tokens.size());
        TEST_ASSERT_EQUAL_STRING("Ok", parser::error(result.error).c_str());
        TEST_ASSERT_EQUAL_STRING("abc", result.tokens[0].toString().c_str());
        TEST_ASSERT_EQUAL_STRING("abc", result.tokens[1].toString().c_str());

        TEST_ASSERT(find_and_call(result, DefaultOutput));
        TEST_ASSERT(abc_done);
    }
}

// Ensure parsing function does not cause nearby strings to be included
void test_parse_overlap() {
    const char input[] =
        "three\r\n"
        "two\r\n"
        "one\r\n";

    const auto* ptr = &input[0];
    const auto* end = &input[__builtin_strlen(input)];

    {
        const auto eol = std::find(ptr, end, '\n');
        const auto result = parse_line(StringView(ptr, std::next(eol)));
        TEST_ASSERT_EQUAL(parser::Error::Ok, result.error);
        TEST_ASSERT(std::next(eol) != end);
        ptr = std::next(eol);

        TEST_ASSERT_EQUAL(1, result.tokens.size());
        TEST_ASSERT_EQUAL_STRING("three", result.tokens[0].toString().c_str());
    }

    {
        const auto eol = std::find(ptr, end, '\n');
        const auto result = parse_line(StringView(ptr, std::next(eol)));
        TEST_ASSERT_EQUAL(parser::Error::Ok, result.error);
        TEST_ASSERT(std::next(eol) != end);
        ptr = std::next(eol);

        TEST_ASSERT_EQUAL(1, result.tokens.size());
        TEST_ASSERT_EQUAL_STRING("two", result.tokens[0].toString().c_str());
    }

    {
        const auto eol = std::find(ptr, end, '\n');
        const auto result = parse_line(StringView(ptr, std::next(eol)));
        TEST_ASSERT_EQUAL(parser::Error::Ok, result.error);
        TEST_ASSERT(std::next(eol) == end);
        TEST_ASSERT_EQUAL(1, result.tokens.size());
        TEST_ASSERT_EQUAL_STRING("one", result.tokens[0].toString().c_str());
    }
}

// Ensure non-terminated string is only parsed when asked for
void test_parse_inject() {
    STRING_VIEW_INLINE(Multiple, "this\r\nshould\nbe\r\nparsed");

    StringView inputs[] {
        "this",
        "should",
        "be",
        "parsed",
    };

    auto input = Multiple;

    // first three tokens are successfully parsed
    for (size_t index = 0; index < 3; ++index) {
        const auto result = parse_line(input);

        TEST_ASSERT_EQUAL(parser::Error::Ok, result.error);
        TEST_ASSERT_GREATER_THAN(0, result.remaining.length());
        TEST_ASSERT_EQUAL(1, result.tokens.size());
        TEST_ASSERT_EQUAL(0, result.buffer.size());

        TEST_ASSERT(inputs[index] == result.tokens[0]);

        TEST_ASSERT(result.remaining.length() > 0);
        input = result.remaining;
    }

    // last one is missing line ending
    {
        const auto result = parse_line(input);
        TEST_ASSERT_EQUAL(parser::Error::UnexpectedLineEnd, result.error);
    }

    // but should be parsed when implicitly terminated
    {
        const auto result = parse_terminated(input);
        TEST_ASSERT_EQUAL(parser::Error::Ok, result.error);

        TEST_ASSERT_EQUAL(1, result.tokens.size());
        TEST_ASSERT_EQUAL(0, result.buffer.size());
        TEST_ASSERT_EQUAL(0, result.remaining.length());

        TEST_ASSERT(inputs[3] == result.tokens[0]);
        TEST_ASSERT(result.remaining.length() == 0);
    }

    // incomplete newlines are not normally parsed
    {
        const auto result = parse_line("incomplete\r");
        TEST_ASSERT_EQUAL(parser::Error::UnexpectedLineEnd, result.error);
    }

    // but should be when implicitly terminated
    {
        const auto result = parse_terminated("incomplete\r");
        TEST_ASSERT_EQUAL(parser::Error::Ok, result.error);

        TEST_ASSERT_EQUAL(1, result.tokens.size());
        TEST_ASSERT_EQUAL(0, result.buffer.size());
        TEST_ASSERT_EQUAL(0, result.remaining.length());

        TEST_ASSERT_EQUAL_STRING("incomplete",
            result.tokens[0].toString().c_str());
    }
}

// recent terminal version also allows a static commands list instead of passing
// each individual name+func one by one
void test_commands_array() {
    static bool results[] {
        false,
        false,
        false,
    };

    static Command commands[] {
        Command{.name = "array.one", .func = [](CommandContext&&) {
            results[0] = true;
        }},
        Command{.name = "array.two", .func = [](CommandContext&&) {
            results[1] = true;
        }},
        Command{.name = "array.three", .func = [](CommandContext&&) {
            results[2] = true;
        }},
    };

    const auto before = size();
    add(Commands{std::begin(commands), std::end(commands)});

    TEST_ASSERT_EQUAL(before + 3, size());

    const char input[] = "array.one\narray.two\narray.three\n";

    PrintString out(64);
    const auto result = api_find_and_call(input, out);
    TEST_ASSERT_MESSAGE(out.isEmpty(), out.c_str());
    TEST_ASSERT(result);

    TEST_ASSERT(results[0]);
    TEST_ASSERT(results[1]);
    TEST_ASSERT(results[2]);
}

// Ensure that we can register multiple commands (at least 3, might want to test much more in the future?)
// Ensure that registered commands can be called and they are called in order
void test_multiple_commands() {
    // make sure commands execute in sequence
    static bool results[] {
        false,
        false,
        false,
        false,
    };

    add("test1", [](CommandContext&& ctx) {
        TEST_ASSERT_EQUAL_STRING("test1", ctx.argv[0].c_str());
        TEST_ASSERT_EQUAL(1, ctx.argv.size());
        TEST_ASSERT_FALSE(results[0]);
        TEST_ASSERT_FALSE(results[1]);
        TEST_ASSERT_FALSE(results[2]);
        TEST_ASSERT_FALSE(results[3]);
        results[0] = true;
    });

    add("test2", [](CommandContext&& ctx) {
        TEST_ASSERT_EQUAL_STRING("test2", ctx.argv[0].c_str());
        TEST_ASSERT_EQUAL(1, ctx.argv.size());
        TEST_ASSERT(results[0]);
        TEST_ASSERT_FALSE(results[1]);
        TEST_ASSERT_FALSE(results[2]);
        TEST_ASSERT_FALSE(results[3]);
        results[1] = true;
    });

    add("test3", [](CommandContext&& ctx) {
        TEST_ASSERT_EQUAL_STRING("test3", ctx.argv[0].c_str());
        TEST_ASSERT_EQUAL(1, ctx.argv.size());
        TEST_ASSERT(results[0]);
        TEST_ASSERT(results[1]);
        TEST_ASSERT_FALSE(results[2]);
        TEST_ASSERT_FALSE(results[3]);
        results[2] = true;
    });

    add("test4", [](CommandContext&& ctx) {
        TEST_ASSERT_EQUAL_STRING("test4", ctx.argv[0].c_str());
        TEST_ASSERT_EQUAL(1, ctx.argv.size());
        TEST_ASSERT(results[0]);
        TEST_ASSERT(results[1]);
        TEST_ASSERT(results[2]);
        TEST_ASSERT_FALSE(results[3]);
        results[3] = true;
    });

    const char input[] = "test1; test2\n test3\r\n test4";
    TEST_ASSERT(api_find_and_call(input, DefaultOutput));

    TEST_ASSERT(results[0]);
    TEST_ASSERT(results[1]);
    TEST_ASSERT(results[2]);
    TEST_ASSERT(results[3]);
}

void test_command() {
    static int counter = 0;

    add("test.command", [](CommandContext&& ctx) {
        TEST_ASSERT_EQUAL_MESSAGE(1, ctx.argv.size(),
            "Command without args should have argc == 1");
        ++counter;
    });

    const char command[] = "test.command";
    TEST_ASSERT(find_and_call(command, DefaultOutput));
    TEST_ASSERT_EQUAL_MESSAGE(1, counter,
        "`test.command` cannot be called more than one time");

    TEST_ASSERT(find_and_call(command, DefaultOutput));
    TEST_ASSERT_EQUAL_MESSAGE(2, counter,
        "`test.command` cannot be called more than two times");

    const char lf[] = "test.command\n";
    TEST_ASSERT(find_and_call(lf, DefaultOutput));
    TEST_ASSERT_EQUAL_MESSAGE(3, counter,
        "`test.command` cannot be called more than three times");

    const char crlf[] = "test.command\r\n";
    TEST_ASSERT(find_and_call(crlf, DefaultOutput));
    TEST_ASSERT_EQUAL_MESSAGE(4, counter,
        "`test.command` cannot be called more than four times");
}

// Ensure that we can properly handle arguments
void test_command_args() {
    static bool waiting = false;

    add("test.command.arg1", [](CommandContext&& ctx) {
        TEST_ASSERT_EQUAL(2, ctx.argv.size());
        waiting = false;
    });

    add("test.command.arg1_empty", [](CommandContext&& ctx) {
        TEST_ASSERT_EQUAL(2, ctx.argv.size());
        TEST_ASSERT(!ctx.argv[1].length());
        waiting = false;
    });

    waiting = true;

    PrintString out(64);
    const char empty[] = "test.command.arg1_empty \"\"";
    const auto result = find_and_call(empty, out);
    TEST_ASSERT_EQUAL_MESSAGE(0, out.length(), out.c_str());
    TEST_ASSERT(result);
    TEST_ASSERT(!waiting);

    waiting = true;

    const char one_arg[] = "test.command.arg1 test";
    TEST_ASSERT(find_and_call(one_arg, DefaultOutput));
    TEST_ASSERT(!waiting);
}

// both \r\n and \n are valid line separators
void test_new_line() {
    {
        const auto result = parse_line("test.new.line\r\n");
        TEST_ASSERT_EQUAL(1, result.tokens.size());
        TEST_ASSERT_EQUAL_STRING("test.new.line",
            result.tokens[0].toString().c_str());
    }

    {
        const auto result = parse_line("test.new.line\n");
        TEST_ASSERT_EQUAL(1, result.tokens.size());
        TEST_ASSERT_EQUAL_STRING("test.new.line",
            result.tokens[0].toString().c_str());
    }

    {
        const auto result = parse_line("test.new.line\r");
        TEST_ASSERT_EQUAL_STRING("UnexpectedLineEnd",
            parser::error(result.error).c_str());
        TEST_ASSERT_EQUAL(0, result.tokens.size());
    }
}

// various parser errors related to quoting
void test_quotes() {
    {
        const auto result = parse_line("test.quotes \"quote that does not\"feel right");
        TEST_ASSERT_EQUAL_STRING("NoSpaceAfterQuote",
            parser::error(result.error).c_str());
        TEST_ASSERT_EQUAL(0, result.tokens.size());
    }

    {
        const auto result = parse_line("test.quotes \"quote that does not line break\"");
        TEST_ASSERT_EQUAL_STRING("NoSpaceAfterQuote",
            parser::error(result.error).c_str());
        TEST_ASSERT_EQUAL(0, result.tokens.size());
    }

    {
        const auto result = parse_line("test.quotes \"quote without a pair\r\n");
        TEST_ASSERT_EQUAL_STRING("UnterminatedQuote",
            parser::error(result.error).c_str());
        TEST_ASSERT_EQUAL(0, result.tokens.size());
    }

    {
        const auto result = parse_line("test.quotes 'quote without a pair\r\n");
        TEST_ASSERT_EQUAL_STRING("UnterminatedQuote",
            parser::error(result.error).c_str());
        TEST_ASSERT_EQUAL(0, result.tokens.size());
    }

    {
        const auto result = parse_line("test.quotes ''\r\n");
        TEST_ASSERT_EQUAL(2, result.tokens.size());
    }

    {
        const auto result = parse_line("test.quotes \"\"\r\n");
        TEST_ASSERT_EQUAL(2, result.tokens.size());
    }
}

// we specify that commands lowercase == UPPERCASE
// last registered one should be called, we don't check for duplicates at this time
void test_case_insensitive() {
    add("test.lowercase1", [](CommandContext&&) {
        TEST_FAIL_MESSAGE("`test.lowercase1` was registered first, but there's another function by the same name. This should not be called");
    });

    add("TEST.LOWERCASE1", [](CommandContext&&) {
        __asm__ volatile ("nop");
    });

    const char input[] = "TeSt.lOwErCaSe1";
    TEST_ASSERT(find_and_call(input, DefaultOutput));
}

// We can use command ctx.output to send something back into the stream
void test_output() {
    add("test.output", [](CommandContext&& ctx) {
        if (ctx.argv.size() == 2) {
            ctx.output.print(ctx.argv[1]);
        }
    });

    const char input[] = "test.output test1234567890";

    PrintString output(64);
    TEST_ASSERT(find_and_call(input, output));

    const char match[] = "test1234567890";
    TEST_ASSERT_EQUAL_STRING(match, output.c_str());
}

// un-buffered view returning multiple times until strings are exhausted
void test_line_view() {
    const char input[] = "one\r\ntwo\nthree\r\n";
    LineView view(input);

    const auto one = view.next();
    TEST_ASSERT_EQUAL_STRING("one",
        one.toString().c_str());

    const auto two = view.next();
    TEST_ASSERT_EQUAL_STRING("two",
        two.toString().c_str());

    const auto three = view.next();
    TEST_ASSERT_EQUAL_STRING("three",
        three.toString().c_str());

    TEST_ASSERT_EQUAL(0, view.next().length());
}

// Ensure that we keep buffering when input is incomplete
void test_line_buffer() {
    const char input[] =
        "aaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaa";

    LineBuffer<256> buffer;
    buffer.append(input);

    TEST_ASSERT_EQUAL(buffer.size(), __builtin_strlen(input));
    TEST_ASSERT_EQUAL(0, buffer.next().value.length());

    buffer.append("\r\n");

    const auto next = buffer.next();
    TEST_ASSERT_EQUAL(0, buffer.size());
    TEST_ASSERT_EQUAL(__builtin_strlen(input), next.value.length());
    TEST_ASSERT_EQUAL_CHAR_ARRAY(
        &input[0], next.value.data(), __builtin_strlen(input));
}

// Ensure that when buffer overflows, we set 'overflow' flags
// on both the buffer and the returned 'line' result
void test_line_buffer_overflow() {
    using Buffer = LineBuffer<16>;
    static_assert(Buffer::capacity() == 16, "");

    Buffer buffer;
    TEST_ASSERT_EQUAL(0, buffer.size());
    TEST_ASSERT(!buffer.overflow());

    // verify our expansion works, buffer needs to overflow
    std::array<char, (Buffer::capacity() * 2) + 2> data;
    std::fill(std::begin(data), std::end(data), 'd');
    data.back() = '\n';

    buffer.append(data.data(), data.size());
    TEST_ASSERT(buffer.overflow());

    const auto result = buffer.next();
    TEST_ASSERT(result.overflow);

    TEST_ASSERT_EQUAL(0, buffer.size());
    TEST_ASSERT(!buffer.overflow());

    // TODO: can't compare string_view directly, not null terminated
    const auto line = result.value.toString();
    TEST_ASSERT_EQUAL_STRING("d", line.c_str());
}

// When input has multiple 'new-line' characters, generated result only has one line at a time
void test_line_buffer_multiple() {
    LineBuffer<64> buffer;

    constexpr auto First = StringView("first\n");
    buffer.append(First);

    constexpr auto Second = StringView("second\n");
    buffer.append(Second);

    TEST_ASSERT_EQUAL(First.length() + Second.length(),
            buffer.size());
    TEST_ASSERT(!buffer.overflow());

    // both entries remain in the buffer and are available
    // if we don't touch the buffer via another append().
    // (in theory, could also add refcount... right now seems like an overkill)

    const auto first = buffer.next();
    TEST_ASSERT_GREATER_THAN(0, buffer.size());
    TEST_ASSERT(First.slice(0, First.length() - 1) == first.value);

    // second entry resets everything
    const auto second = buffer.next();
    TEST_ASSERT_EQUAL(0, buffer.size());
    TEST_ASSERT(Second.slice(0, Second.length() - 1) == second.value);
}

void test_error_output() {
    PrintString out(64);
    PrintString err(64);

    add("test.error1", [](CommandContext&& ctx) {
        ctx.error.print("foo");
    });

    TEST_ASSERT(find_and_call("test.error1", out, err));
    TEST_ASSERT_EQUAL_MESSAGE(0, out.length(), out.c_str());
    TEST_ASSERT_EQUAL_STRING("foo", err.c_str());

    out.clear();
    err.clear();

    add("test.error2", [](CommandContext&& ctx) {
        ctx.output.print("bar");
    });

    TEST_ASSERT(find_and_call("test.error2", out, err));
    TEST_ASSERT_EQUAL_STRING("bar", out.c_str());
    TEST_ASSERT_EQUAL_MESSAGE(0, err.length(), err.c_str());

    out.clear();
    err.clear();

    TEST_ASSERT(!find_and_call("test.error3", out, err));
    TEST_ASSERT_EQUAL_MESSAGE(0, out.length(), out.c_str());
    TEST_ASSERT_NOT_EQUAL_MESSAGE(0, err.length(), err.c_str());
}

} // namespace
} // namespace test
} // namespace terminal
} // namespace espurna

// When adding test functions, don't forget to add RUN_TEST(...) in the main()

int main(int, char**) {
    UNITY_BEGIN();

    using namespace espurna::terminal::test;
    RUN_TEST(test_command);
    RUN_TEST(test_command_args);
    RUN_TEST(test_parse_overlap);
    RUN_TEST(test_parse_inject);
    RUN_TEST(test_commands_array);
    RUN_TEST(test_multiple_commands);
    RUN_TEST(test_hex_codes);
    RUN_TEST(test_quotes);
    RUN_TEST(test_case_insensitive);
    RUN_TEST(test_output);
    RUN_TEST(test_new_line);
    RUN_TEST(test_line_view);
    RUN_TEST(test_line_buffer);
    RUN_TEST(test_line_buffer_overflow);
    RUN_TEST(test_line_buffer_multiple);
    RUN_TEST(test_error_output);

    return UNITY_END();
}
