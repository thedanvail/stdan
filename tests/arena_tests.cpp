#include <catch2/catch_test_macros.hpp>

#include "arena.hpp"

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

    arena* a = result.value();
    REQUIRE(a != nullptr);
    REQUIRE(a->base_ptr != nullptr);
    REQUIRE(a->reserved_size >= 4096);
    REQUIRE(a->committed_size == 0);
    REQUIRE(a->current_offset == 0);

    arena_release(a);
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
    arena* a = arena_result.value();

    auto result = arena_alloc(a, 0);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == alloc_error::ZeroSize);

    arena_release(a);
}

TEST_CASE("arena_alloc rejects non-power-of-two alignment") {
    auto arena_result = create_arena(4096);
    REQUIRE(arena_result.has_value());
    arena* a = arena_result.value();

    auto result = arena_alloc(a, 64, 3);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == alloc_error::BadAlignment);

    arena_release(a);
}

TEST_CASE("arena_alloc returns memory aligned to the requested alignment") {
    auto arena_result = create_arena(4096);
    REQUIRE(arena_result.has_value());
    arena* a = arena_result.value();

    auto result = arena_alloc(a, 64, 64);
    REQUIRE(result.has_value());
    REQUIRE(reinterpret_cast<std::uintptr_t>(result.value()) % 64 == 0);

    arena_release(a);
}

TEST_CASE("arena_alloc returns non-overlapping regions") {
    auto arena_result = create_arena(4096);
    REQUIRE(arena_result.has_value());
    arena* a = arena_result.value();

    auto p1 = arena_alloc(a, 32);
    auto p2 = arena_alloc(a, 32);
    REQUIRE(p1.has_value());
    REQUIRE(p2.has_value());
    REQUIRE(p1.value() + 32 <= p2.value());

    arena_release(a);
}

TEST_CASE("arena_reset resets the current offset") {
    auto arena_result = create_arena(4096);
    REQUIRE(arena_result.has_value());
    arena* a = arena_result.value();

    auto alloc = arena_alloc(a, 64);
    REQUIRE(alloc.has_value());
    REQUIRE(a->current_offset >= 64);

    arena_reset(a);
    REQUIRE(a->current_offset == 0);

    arena_release(a);
}

TEST_CASE("arena_construct builds a trivial type at the allocated address") {
    auto arena_result = create_arena(4096);
    REQUIRE(arena_result.has_value());
    arena* a = arena_result.value();

    auto p = arena_construct<int>(a, 42);
    REQUIRE(p.has_value());
    REQUIRE(*p.value() == 42);

    arena_release(a);
}

TEST_CASE("arena_alloc propagates out-of-space failures") {
    auto arena_result = create_arena(4096);
    REQUIRE(arena_result.has_value());
    arena* a = arena_result.value();

    auto result = arena_alloc(a, 8192);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == alloc_error::NotEnoughMemory);

    arena_release(a);
}
