#pragma once

// Unused right now, but might use later on.
// I wish these could be supported in a standard way.
// Maybe they are and I'm just stupid. It's possible.
#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32) || defined(_MSC_VER)
#define STDAN_WIN
#endif

#ifndef STDAN_DEBUG
    #define dassert(expr, ...) ((void)0)
#else

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <iostream> // IWYU pragma: keep
#include <utility>


namespace stdan::assert {
inline void assertFailed(
    const char* expr,
    const char* file,
    int line
) {
    std::cerr << "  Assertion failed: " << expr
              << "\n  Location: " << file << ':' << line << '\n';
    assert(false);
}

template<typename... Args>
void assertFailed(
    const char* expr,
    const char* file,
    int line,
    std::format_string<Args...> fmt,
    Args&&... args
) {
    std::cerr << "  Assertion failed: " << expr
              << "\n  Message: " << std::format(fmt, std::forward<Args>(args)...)
              << "\n  Location: " << file << ':' << line << '\n';
    assert(false);
}
} // namespace

#define dassert(expr, ...)            \
do {                                  \
    if (!(expr)) {                    \
        stdan::assert::assertFailed(  \
            #expr, __FILE__, __LINE__ \
            __VA_OPT__(,) __VA_ARGS__ \
        );                            \
    }                                 \
} while (0)

#endif

#include <type_traits>

namespace stdan::concepts {
template<typename T>
concept copy_reusable = std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>;

template<typename T>
concept move_reusable = std::is_move_constructible_v<T> && std::is_move_assignable_v<T>;
} // namespace stdan::concepts

