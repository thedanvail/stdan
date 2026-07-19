#pragma once

// Unused right now, but might use later on.
// I wish these could be supported in a standard way.
// Maybe they are and I'm just stupid. It's possible.
#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32) || defined(_MSC_VER)
#define STDAN_WIN
#endif

#include <memory>
#include <stdexcept>

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


struct move_only_value {
    std::unique_ptr<int> value;

    move_only_value()
        : value(std::make_unique<int>(-1)) {}

    explicit move_only_value(int v)
        : value(std::make_unique<int>(v)) {}

    move_only_value(move_only_value&&) noexcept = default;
    move_only_value& operator=(move_only_value&&) noexcept = default;
    move_only_value(const move_only_value&) = delete;
    move_only_value& operator=(const move_only_value&) = delete;
};

struct throwing_copy_value {
    inline static bool throw_on_copy = false;

    int value = 0;

    throwing_copy_value() = default;
    explicit throwing_copy_value(int v) noexcept
        : value(v) {}

    throwing_copy_value(const throwing_copy_value& other)
        : value(other.value) {
        if(throw_on_copy) { throw std::runtime_error("copy construction failed"); }
    }

    throwing_copy_value(throwing_copy_value&&) noexcept = default;
    throwing_copy_value& operator=(const throwing_copy_value& other) {
        if(throw_on_copy) { throw std::runtime_error("copy assignment failed"); }
        value = other.value;
        return *this;
    }

    throwing_copy_value& operator=(throwing_copy_value&&) noexcept = default;

    friend bool operator==(const throwing_copy_value& lhs, const throwing_copy_value& rhs) = default;
};

} // namespace test_support
