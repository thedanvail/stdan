#pragma once

#include <memory>
#include <stdexcept>

namespace test_support {

struct tracked_value {
    inline static int live = 0;
    inline static int destroyed = 0;
    inline static int& live_instances = live;
    inline static int& destructor_calls = destroyed;

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

using arena_tracked_value = tracked_value;


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
