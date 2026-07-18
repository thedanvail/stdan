#pragma once

namespace test_support {

struct tracked_value {
    inline static int live = 0;
    inline static int destroyed = 0;

    int value = 0;

    tracked_value() noexcept { ++live; }
    explicit tracked_value(int v) noexcept
        : value(v) {
        ++live;
    }

    tracked_value(const tracked_value& other) noexcept
        : value(other.value) {
        ++live;
    }

    tracked_value(tracked_value&& other) noexcept
        : value(other.value) {
        ++live;
        other.value = -1;
    }

    tracked_value& operator=(const tracked_value& other) noexcept {
        value = other.value;
        return *this;
    }

    tracked_value& operator=(tracked_value&& other) noexcept {
        value = other.value;
        other.value = -1;
        return *this;
    }

    ~tracked_value() noexcept {
        --live;
        ++destroyed;
    }

    static void reset() noexcept {
        live = 0;
        destroyed = 0;
    }
};

struct arena_tracked_value {
    inline static int live_instances = 0;
    inline static int destructor_calls = 0;

    int value = 0;

    arena_tracked_value() noexcept { ++live_instances; }
    explicit arena_tracked_value(int v) noexcept
        : value(v) {
        ++live_instances;
    }

    arena_tracked_value(const arena_tracked_value& other)
        : value(other.value) {
        ++live_instances;
    }

    arena_tracked_value(arena_tracked_value&& other) noexcept
        : value(other.value) {
        ++live_instances;
    }

    arena_tracked_value& operator=(const arena_tracked_value&) = default;
    arena_tracked_value& operator=(arena_tracked_value&&) = default;

    ~arena_tracked_value() noexcept {
        --live_instances;
        ++destructor_calls;
    }

    static void reset() noexcept {
        live_instances = 0;
        destructor_calls = 0;
    }
};

} // namespace test_support
