#include <catch2/catch_test_macros.hpp>
#include <smart/CircularBuffer.h>

TEST_CASE("CircularBuffer initialization", "[circular_buffer]") {
    smart::CircularBuffer<int> buffer(10);

    SECTION("starts empty") {
        REQUIRE(buffer.empty());
        REQUIRE_FALSE(buffer.full());
        REQUIRE(buffer.size() == 0);
    }

    SECTION("capacity is size minus one") {
        REQUIRE(buffer.capacity() == 9);
    }

    SECTION("available equals capacity when empty") {
        REQUIRE(buffer.available() == 9);
    }
}

TEST_CASE("CircularBuffer push and pop single elements", "[circular_buffer]") {
    smart::CircularBuffer<int> buffer(5);

    SECTION("push increases size") {
        REQUIRE(buffer.push(42));
        REQUIRE(buffer.size() == 1);
        REQUIRE_FALSE(buffer.empty());
    }

    SECTION("pop returns pushed value") {
        buffer.push(42);
        int value;
        REQUIRE(buffer.pop(value));
        REQUIRE(value == 42);
        REQUIRE(buffer.empty());
    }

    SECTION("peek returns value without removing") {
        buffer.push(42);
        int peeked = buffer.peek();
        REQUIRE(peeked == 42);
        REQUIRE(buffer.size() == 1);
    }

    SECTION("multiple push and pop") {
        buffer.push(1);
        buffer.push(2);
        buffer.push(3);

        REQUIRE(buffer.size() == 3);

        int v;
        REQUIRE(buffer.pop(v));
        REQUIRE(v == 1);
        REQUIRE(buffer.pop(v));
        REQUIRE(v == 2);
        REQUIRE(buffer.pop(v));
        REQUIRE(v == 3);

        REQUIRE(buffer.empty());
    }
}

TEST_CASE("CircularBuffer full detection", "[circular_buffer]") {
    smart::CircularBuffer<int> buffer(4);  // capacity = 3

    SECTION("becomes full when capacity reached") {
        REQUIRE(buffer.push(1));
        REQUIRE(buffer.push(2));
        REQUIRE(buffer.push(3));

        REQUIRE(buffer.full());
        REQUIRE(buffer.size() == 3);
        REQUIRE(buffer.available() == 0);
    }

    SECTION("push fails when full") {
        buffer.push(1);
        buffer.push(2);
        buffer.push(3);

        REQUIRE_FALSE(buffer.push(4));
        REQUIRE(buffer.size() == 3);
    }

    SECTION("can push after pop from full buffer") {
        buffer.push(1);
        buffer.push(2);
        buffer.push(3);

        int v;
        buffer.pop(v);

        REQUIRE(buffer.push(4));
        REQUIRE(buffer.size() == 3);
    }
}

TEST_CASE("CircularBuffer wrap-around", "[circular_buffer]") {
    smart::CircularBuffer<int> buffer(4);  // capacity = 3

    SECTION("wraps correctly after multiple cycles") {
        // Fill and empty multiple times
        for (int cycle = 0; cycle < 3; ++cycle) {
            for (int i = 0; i < 3; ++i) {
                REQUIRE(buffer.push(cycle * 10 + i));
            }

            for (int i = 0; i < 3; ++i) {
                int v;
                REQUIRE(buffer.pop(v));
                REQUIRE(v == cycle * 10 + i);
            }

            REQUIRE(buffer.empty());
        }
    }

    SECTION("partial fill and empty preserves order") {
        buffer.push(1);
        buffer.push(2);

        int v;
        buffer.pop(v);
        REQUIRE(v == 1);

        buffer.push(3);
        buffer.push(4);

        buffer.pop(v);
        REQUIRE(v == 2);
        buffer.pop(v);
        REQUIRE(v == 3);
        buffer.pop(v);
        REQUIRE(v == 4);

        REQUIRE(buffer.empty());
    }
}

TEST_CASE("CircularBuffer clear", "[circular_buffer]") {
    smart::CircularBuffer<int> buffer(5);

    buffer.push(1);
    buffer.push(2);
    buffer.push(3);

    REQUIRE(buffer.size() == 3);

    buffer.clear();

    REQUIRE(buffer.empty());
    REQUIRE(buffer.size() == 0);
    REQUIRE(buffer.available() == 4);
}

TEST_CASE("CircularBuffer bulk operations", "[circular_buffer]") {
    smart::CircularBuffer<int> buffer(10);

    SECTION("push range") {
        int data[] = {1, 2, 3, 4, 5};
        REQUIRE(buffer.push(data, data + 5));
        REQUIRE(buffer.size() == 5);

        int v;
        for (int i = 1; i <= 5; ++i) {
            buffer.pop(v);
            REQUIRE(v == i);
        }
    }

    SECTION("push range fails when not enough space") {
        int data[10] = {0};
        REQUIRE_FALSE(buffer.push(data, data + 10));  // capacity is 9
    }

    SECTION("pop_n retrieves multiple elements") {
        for (int i = 0; i < 5; ++i) {
            buffer.push(i);
        }

        int out[3];
        unsigned int count = buffer.pop_n(out, 3);

        REQUIRE(count == 3);
        REQUIRE(out[0] == 0);
        REQUIRE(out[1] == 1);
        REQUIRE(out[2] == 2);
        REQUIRE(buffer.size() == 2);
    }

    SECTION("peek retrieves without removing") {
        for (int i = 0; i < 5; ++i) {
            buffer.push(i);
        }

        int out[3];
        unsigned int count = buffer.peek(out, 3);

        REQUIRE(count == 3);
        REQUIRE(out[0] == 0);
        REQUIRE(out[1] == 1);
        REQUIRE(out[2] == 2);
        REQUIRE(buffer.size() == 5);  // unchanged
    }
}

TEST_CASE("CircularBuffer with different types", "[circular_buffer]") {
    SECTION("works with doubles") {
        smart::CircularBuffer<double> buffer(5);
        buffer.push(3.14);
        buffer.push(2.71);

        double v;
        buffer.pop(v);
        REQUIRE(v == 3.14);
    }

    SECTION("works with structs") {
        struct Point { int x, y; };
        smart::CircularBuffer<Point> buffer(5);

        buffer.push({10, 20});
        buffer.push({30, 40});

        Point p;
        buffer.pop(p);
        REQUIRE(p.x == 10);
        REQUIRE(p.y == 20);
    }
}
