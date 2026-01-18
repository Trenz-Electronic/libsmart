#include <catch2/catch_test_macros.hpp>
#include <smart/string.h>

TEST_CASE("int_of parses decimal integers", "[string]") {
    int result;

    SECTION("positive integers") {
        REQUIRE(smart::int_of("123", result));
        REQUIRE(result == 123);
    }

    SECTION("negative integers") {
        REQUIRE(smart::int_of("-456", result));
        REQUIRE(result == -456);
    }

    SECTION("zero") {
        REQUIRE(smart::int_of("0", result));
        REQUIRE(result == 0);
    }

    SECTION("invalid input returns false") {
        REQUIRE_FALSE(smart::int_of("abc", result));
        REQUIRE_FALSE(smart::int_of("", result));
    }
}

TEST_CASE("int_of parses hexadecimal integers", "[string]") {
    int result;

    SECTION("lowercase 0x prefix") {
        REQUIRE(smart::int_of("0xff", result));
        REQUIRE(result == 255);
    }

    SECTION("uppercase 0X prefix") {
        REQUIRE(smart::int_of("0XFF", result));
        REQUIRE(result == 255);
    }

    SECTION("mixed case hex digits") {
        REQUIRE(smart::int_of("0xAbCd", result));
        REQUIRE(result == 0xABCD);
    }
}

TEST_CASE("uint_of parses unsigned integers", "[string]") {
    unsigned int result;

    SECTION("positive integers") {
        REQUIRE(smart::uint_of("123", result));
        REQUIRE(result == 123u);
    }

    SECTION("zero") {
        REQUIRE(smart::uint_of("0", result));
        REQUIRE(result == 0u);
    }

    SECTION("large values") {
        REQUIRE(smart::uint_of("4294967295", result));
        REQUIRE(result == 4294967295u);
    }

    SECTION("invalid input returns false") {
        REQUIRE_FALSE(smart::uint_of("abc", result));
        REQUIRE_FALSE(smart::uint_of("", result));
    }
}

TEST_CASE("uint_of throwing version", "[string]") {
    SECTION("valid input returns value") {
        REQUIRE(smart::uint_of("42") == 42u);
    }

    SECTION("invalid input throws") {
        REQUIRE_THROWS(smart::uint_of("invalid"));
    }
}

TEST_CASE("trim removes whitespace", "[string]") {
    SECTION("leading spaces") {
        REQUIRE(smart::trim("   hello") == "hello");
    }

    SECTION("trailing spaces") {
        REQUIRE(smart::trim("hello   ") == "hello");
    }

    SECTION("both ends") {
        REQUIRE(smart::trim("  hello  ") == "hello");
    }

    SECTION("tabs and newlines") {
        REQUIRE(smart::trim("\t\nhello\r\n") == "hello");
    }

    SECTION("empty string") {
        REQUIRE(smart::trim("") == "");
    }

    SECTION("only whitespace") {
        REQUIRE(smart::trim("   \t\n  ") == "");
    }

    SECTION("no whitespace") {
        REQUIRE(smart::trim("hello") == "hello");
    }
}

TEST_CASE("split divides string by separators", "[string]") {
    std::vector<std::string> parts;

    SECTION("simple split by comma") {
        int count = smart::split(parts, "a,b,c", ",");
        REQUIRE(count == 3);
        REQUIRE(parts.size() == 3);
        REQUIRE(parts[0] == "a");
        REQUIRE(parts[1] == "b");
        REQUIRE(parts[2] == "c");
    }

    SECTION("multiple separators treated as one") {
        parts.clear();
        int count = smart::split(parts, "a,,b", ",");
        REQUIRE(count == 2);
        REQUIRE(parts[0] == "a");
        REQUIRE(parts[1] == "b");
    }

    SECTION("multiple separator characters") {
        parts.clear();
        int count = smart::split(parts, "a,b;c", ",;");
        REQUIRE(count == 3);
        REQUIRE(parts[0] == "a");
        REQUIRE(parts[1] == "b");
        REQUIRE(parts[2] == "c");
    }

    SECTION("empty string returns one empty element") {
        parts.clear();
        int count = smart::split(parts, "", ",");
        REQUIRE(count == 1);
        REQUIRE(parts[0] == "");
    }
}

TEST_CASE("ends_with checks string suffix", "[string]") {
    SECTION("matching suffix") {
        REQUIRE(smart::ends_with("hello.txt", ".txt"));
    }

    SECTION("non-matching suffix") {
        REQUIRE_FALSE(smart::ends_with("hello.txt", ".pdf"));
    }

    SECTION("suffix longer than string") {
        REQUIRE_FALSE(smart::ends_with("hi", "hello"));
    }

    SECTION("empty suffix returns false") {
        // Note: this implementation requires suffix to be non-empty and shorter than string
        REQUIRE_FALSE(smart::ends_with("hello", ""));
    }

    SECTION("exact match returns false") {
        // Note: this implementation requires suffix to be strictly shorter than string
        REQUIRE_FALSE(smart::ends_with("hello", "hello"));
    }
}

TEST_CASE("starts_with checks string prefix", "[string]") {
    SECTION("matching prefix") {
        REQUIRE(smart::starts_with("hello world", "hello"));
    }

    SECTION("non-matching prefix") {
        REQUIRE_FALSE(smart::starts_with("hello world", "world"));
    }

    SECTION("prefix longer than string") {
        REQUIRE_FALSE(smart::starts_with("hi", "hello"));
    }

    SECTION("empty prefix") {
        REQUIRE(smart::starts_with("hello", ""));
    }

    SECTION("exact match") {
        REQUIRE(smart::starts_with("hello", "hello"));
    }
}
