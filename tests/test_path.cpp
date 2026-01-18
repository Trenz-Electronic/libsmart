#include <catch2/catch_test_macros.hpp>
#include <smart/Path.h>

TEST_CASE("Path::isSeparator detects path separators", "[path]") {
    SECTION("forward slash is always a separator") {
        REQUIRE(smart::Path::isSeparator('/'));
    }

#if defined(WIN32)
    SECTION("backslash is separator on Windows") {
        REQUIRE(smart::Path::isSeparator('\\'));
    }
#endif

    SECTION("regular characters are not separators") {
        REQUIRE_FALSE(smart::Path::isSeparator('a'));
        REQUIRE_FALSE(smart::Path::isSeparator('.'));
        REQUIRE_FALSE(smart::Path::isSeparator(':'));
    }
}

TEST_CASE("Path::combine joins paths", "[path]") {
    SECTION("two simple paths") {
        std::string result = smart::Path::combine("dir", "file.txt");
        REQUIRE((result == "dir/file.txt" || result == "dir\\file.txt"));
    }

    SECTION("first path with trailing separator adds another") {
        // Note: combine always adds a separator; it doesn't strip existing ones
        std::string result = smart::Path::combine("dir/", "file.txt");
        REQUIRE((result == "dir//file.txt" || result == "dir/\\file.txt"));
    }

    SECTION("second path with leading separator adds another") {
        // Note: combine always adds a separator; it doesn't strip existing ones
        std::string result = smart::Path::combine("dir", "/file.txt");
        REQUIRE((result == "dir//file.txt" || result == "dir\\/file.txt"));
    }

    SECTION("empty first path") {
        std::string result = smart::Path::combine("", "file.txt");
        REQUIRE(result == "file.txt");
    }

    SECTION("empty second path adds trailing separator") {
        // Note: combine always adds a separator when first path is non-empty
        std::string result = smart::Path::combine("dir", "");
        REQUIRE((result == "dir/" || result == "dir\\"));
    }

    SECTION("three paths") {
        std::string result = smart::Path::combine("a", "b", "c");
        REQUIRE((result == "a/b/c" || result == "a\\b\\c"));
    }
}

TEST_CASE("Path::getFilename extracts filename", "[path]") {
    SECTION("simple path with forward slash") {
        REQUIRE(smart::Path::getFilename("/path/to/file.txt") == "file.txt");
    }

#if defined(WIN32)
    SECTION("simple path with backslash on Windows") {
        REQUIRE(smart::Path::getFilename("C:\\path\\to\\file.txt") == "file.txt");
    }
#else
    SECTION("backslash is not separator on Linux") {
        // On Linux, backslash is not a path separator
        REQUIRE(smart::Path::getFilename("C:\\path\\to\\file.txt") == "C:\\path\\to\\file.txt");
    }
#endif

    SECTION("filename only") {
        REQUIRE(smart::Path::getFilename("file.txt") == "file.txt");
    }

    SECTION("path with trailing separator") {
        std::string result = smart::Path::getFilename("/path/to/dir/");
        REQUIRE(result.empty());
    }

    SECTION("empty path") {
        REQUIRE(smart::Path::getFilename("") == "");
    }
}

TEST_CASE("Path::getDirectoryName extracts directory", "[path]") {
    SECTION("simple path with forward slash") {
        REQUIRE(smart::Path::getDirectoryName("/path/to/file.txt") == "/path/to");
    }

#if defined(WIN32)
    SECTION("simple path with backslash on Windows") {
        REQUIRE(smart::Path::getDirectoryName("C:\\path\\to\\file.txt") == "C:\\path\\to");
    }
#else
    SECTION("backslash is not separator on Linux") {
        // On Linux, backslash is not a path separator, so no directory is found
        REQUIRE(smart::Path::getDirectoryName("C:\\path\\to\\file.txt") == "");
    }
#endif

    SECTION("filename only") {
        REQUIRE(smart::Path::getDirectoryName("file.txt") == "");
    }

    SECTION("root path") {
        REQUIRE(smart::Path::getDirectoryName("/file.txt") == "");
    }

    SECTION("empty path") {
        REQUIRE(smart::Path::getDirectoryName("") == "");
    }
}

TEST_CASE("Path::getFilenameWoExt removes extension", "[path]") {
    SECTION("simple filename with extension") {
        REQUIRE(smart::Path::getFilenameWoExt("file.txt") == "file");
    }

    SECTION("path with extension") {
        REQUIRE(smart::Path::getFilenameWoExt("/path/to/file.txt") == "/path/to/file");
    }

    SECTION("multiple extensions") {
        REQUIRE(smart::Path::getFilenameWoExt("file.tar.gz") == "file.tar");
    }

    SECTION("no extension") {
        REQUIRE(smart::Path::getFilenameWoExt("file") == "file");
    }

    SECTION("hidden file on Unix") {
        REQUIRE(smart::Path::getFilenameWoExt(".bashrc") == "");
    }
}
