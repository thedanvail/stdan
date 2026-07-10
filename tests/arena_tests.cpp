#include <catch2/catch_test_macros.hpp>

#include "memory/arena.hpp"

#include <bit>
#include <cstdint>
#include <type_traits>

namespace {
    struct alignas(64 * 1024) over_aligned_value {
        explicit over_aligned_value(int initial_value) noexcept
            : value(initial_value) {}

        int value;
    };

    static_assert(!std::is_copy_constructible_v<stdan::memory::arena_owner>);
    static_assert(std::is_move_constructible_v<stdan::memory::arena_owner>);
}

using stdan::memory::arena;
using stdan::memory::alloc_error;
using stdan::memory::arena_alloc;
using stdan::memory::arena_construct;
using stdan::memory::arena_release;
using stdan::memory::arena_reset;
using stdan::memory::create_arena;

TEST_CASE("create_arena returns a valid arena for reasonable sizes") {
    auto result = create_arena(4096);
    REQUIRE(result.has_value());

    arena* a = result.value().get();
    REQUIRE(a != nullptr);
    REQUIRE(a->base_ptr != nullptr);
    REQUIRE(a->reserved_size >= 4096);
    REQUIRE(a->committed_size == 0);
    REQUIRE(a->current_offset == 0);
}

TEST_CASE("create_arena fails for zero size") {
    auto result = create_arena(0);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == alloc_error::ZeroSize);
}

TEST_CASE("arena_alloc rejects null arena") {
    auto result = arena_alloc(nullptr, 64);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == alloc_error::NullPointerArenaArg);
}

TEST_CASE("arena_alloc rejects zero size") {
    auto arena_result = create_arena(4096);
    REQUIRE(arena_result.has_value());
    arena* a = arena_result.value().get();

    auto result = arena_alloc(a, 0);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == alloc_error::ZeroSize);
}

TEST_CASE("arena_alloc rejects non-power-of-two alignment") {
    auto arena_result = create_arena(4096);
    REQUIRE(arena_result.has_value());
    arena* a = arena_result.value().get();

    auto result = arena_alloc(a, 64, 3);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == alloc_error::BadAlignment);
}

TEST_CASE("arena_alloc returns memory aligned to the requested alignment") {
    auto arena_result = create_arena(4096);
    REQUIRE(arena_result.has_value());
    arena* a = arena_result.value().get();

    auto result = arena_alloc(a, 64, 64);
    REQUIRE(result.has_value());
    REQUIRE(reinterpret_cast<std::uintptr_t>(result.value()) % 64 == 0);
}

TEST_CASE("arena_alloc returns non-overlapping regions") {
    auto arena_result = create_arena(4096);
    REQUIRE(arena_result.has_value());
    arena* a = arena_result.value().get();

    auto p1 = arena_alloc(a, 32);
    auto p2 = arena_alloc(a, 32);
    REQUIRE(p1.has_value());
    REQUIRE(p2.has_value());
    REQUIRE(p1.value() + 32 <= p2.value());
}

TEST_CASE("arena_reset resets the current offset") {
    auto arena_result = create_arena(4096);
    REQUIRE(arena_result.has_value());
    arena* a = arena_result.value().get();

    auto alloc = arena_alloc(a, 64);
    REQUIRE(alloc.has_value());
    REQUIRE(a->current_offset >= 64);

    arena_reset(a);
    REQUIRE(a->current_offset == 0);
}

TEST_CASE("arena_construct builds a trivial type at the allocated address") {
    auto arena_result = create_arena(4096);
    REQUIRE(arena_result.has_value());
    arena* a = arena_result.value().get();

    auto p = arena_construct<int>(a, 42);
    REQUIRE(p.has_value());
    REQUIRE(*p.value() == 42);
}

TEST_CASE("arena_alloc propagates out-of-space failures") {
    auto arena_result = create_arena(4096);
    REQUIRE(arena_result.has_value());
    arena* a = arena_result.value().get();

    auto result = arena_alloc(a, 8192);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == alloc_error::NotEnoughMemory);
}

TEST_CASE("arena_alloc supports alignment larger than the OS page size") {
    const std::size_t alignment = std::bit_ceil(stdan::memory::PAGE_SIZE + 1);
    auto arena_result = create_arena(alignment * 2);
    REQUIRE(arena_result.has_value());

    auto result = arena_alloc(arena_result.value().get(), 1, alignment);
    REQUIRE(result.has_value());
    REQUIRE(reinterpret_cast<std::uintptr_t>(result.value()) % alignment == 0);
}

TEST_CASE("arena_construct supports over-aligned types") {
    constexpr std::size_t alignment = alignof(over_aligned_value);
    auto arena_result = create_arena(alignment * 2);
    REQUIRE(arena_result.has_value());

    auto result = arena_construct<over_aligned_value>(arena_result.value().get(), 42);
    REQUIRE(result.has_value());
    REQUIRE(reinterpret_cast<std::uintptr_t>(result.value()) % alignment == 0);
    REQUIRE(result.value()->value == 42);
}

TEST_CASE("arena_release reports successful explicit release") {
    auto arena_result = create_arena(4096);
    REQUIRE(arena_result.has_value());

    arena* raw_arena = arena_result.value().release();
    auto release_result = arena_release(raw_arena);
    REQUIRE(release_result.has_value());
}

TEST_CASE("arena_release treats a null pointer as an already released arena") {
    auto result = arena_release(nullptr);
    REQUIRE(result.has_value());
}
