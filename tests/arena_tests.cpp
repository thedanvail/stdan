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

using stdan::memory::alloc_error;
using stdan::memory::arena;
using stdan::memory::arena_alloc;
using stdan::memory::arena_construct;
using stdan::memory::arena_release;
using stdan::memory::arena_reset;
using stdan::memory::create_arena;

SCENARIO("an arena is created with a reasonable reserve size") {
    GIVEN("a nonzero reserve size") {
        constexpr std::size_t reserve_size = 4096;

        WHEN("the arena is created") {
            auto result = create_arena(reserve_size);

            THEN("an empty arena with sufficient reserved memory is returned") {
                REQUIRE(result.has_value());
                arena* a = result.value().get();
                REQUIRE(a != nullptr);
                REQUIRE(a->base_ptr != nullptr);
                REQUIRE(a->reserved_size >= reserve_size);
                REQUIRE(a->committed_size == 0);
                REQUIRE(a->current_offset == 0);
            }
        }
    }
}

SCENARIO("an arena is created with a zero reserve size") {
    GIVEN("a zero reserve size") {
        WHEN("the arena is created") {
            auto result = create_arena(0);

            THEN("creation fails with a zero-size error") {
                REQUIRE_FALSE(result.has_value());
                REQUIRE(result.error() == alloc_error::ZeroSize);
            }
        }
    }
}

SCENARIO("memory is allocated without an arena") {
    GIVEN("a null arena pointer") {
        WHEN("an allocation is requested") {
            auto result = arena_alloc(nullptr, 64);

            THEN("allocation fails with a null-arena error") {
                REQUIRE_FALSE(result.has_value());
                REQUIRE(result.error() == alloc_error::NullPointerArenaArg);
            }
        }
    }
}

SCENARIO("a zero-size allocation is requested") {
    GIVEN("a valid arena") {
        auto arena_result = create_arena(4096);
        REQUIRE(arena_result.has_value());

        WHEN("zero bytes are requested") {
            auto result = arena_alloc(arena_result.value().get(), 0);

            THEN("allocation fails with a zero-size error") {
                REQUIRE_FALSE(result.has_value());
                REQUIRE(result.error() == alloc_error::ZeroSize);
            }
        }
    }
}

SCENARIO("an allocation uses a non-power-of-two alignment") {
    GIVEN("a valid arena") {
        auto arena_result = create_arena(4096);
        REQUIRE(arena_result.has_value());

        WHEN("memory is requested with an alignment of three") {
            auto result = arena_alloc(arena_result.value().get(), 64, 3);

            THEN("allocation fails with a bad-alignment error") {
                REQUIRE_FALSE(result.has_value());
                REQUIRE(result.error() == alloc_error::BadAlignment);
            }
        }
    }
}

SCENARIO("an allocation requests explicit alignment") {
    GIVEN("a valid arena and a 64-byte alignment") {
        auto arena_result = create_arena(4096);
        REQUIRE(arena_result.has_value());
        constexpr std::size_t alignment = 64;

        WHEN("memory is allocated") {
            auto result = arena_alloc(arena_result.value().get(), 64, alignment);

            THEN("the returned address has the requested alignment") {
                REQUIRE(result.has_value());
                REQUIRE(reinterpret_cast<std::uintptr_t>(result.value()) % alignment == 0);
            }
        }
    }
}

SCENARIO("multiple regions are allocated from an arena") {
    GIVEN("a valid arena") {
        auto arena_result = create_arena(4096);
        REQUIRE(arena_result.has_value());

        WHEN("two regions are allocated") {
            auto first = arena_alloc(arena_result.value().get(), 32);
            auto second = arena_alloc(arena_result.value().get(), 32);

            THEN("the regions do not overlap") {
                REQUIRE(first.has_value());
                REQUIRE(second.has_value());
                REQUIRE(first.value() + 32 <= second.value());
            }
        }
    }
}

SCENARIO("an arena with live allocations is reset") {
    GIVEN("an arena with an allocation") {
        auto arena_result = create_arena(4096);
        REQUIRE(arena_result.has_value());
        arena* a = arena_result.value().get();
        auto allocation = arena_alloc(a, 64);
        REQUIRE(allocation.has_value());
        REQUIRE(a->current_offset >= 64);

        WHEN("the arena is reset") {
            arena_reset(a);

            THEN("the current offset returns to zero") {
                REQUIRE(a->current_offset == 0);
            }
        }
    }
}

SCENARIO("a trivial object is constructed in an arena") {
    GIVEN("a valid arena") {
        auto arena_result = create_arena(4096);
        REQUIRE(arena_result.has_value());

        WHEN("an integer is constructed") {
            auto result = arena_construct<int>(arena_result.value().get(), 42);

            THEN("the constructed value is returned") {
                REQUIRE(result.has_value());
                REQUIRE(*result.value() == 42);
            }
        }
    }
}

SCENARIO("an allocation exceeds the arena capacity") {
    GIVEN("an arena with 4096 reserved bytes") {
        auto arena_result = create_arena(4096);
        REQUIRE(arena_result.has_value());

        WHEN("8192 bytes are requested") {
            auto result = arena_alloc(arena_result.value().get(), 8192);

            THEN("allocation fails with an out-of-memory error") {
                REQUIRE_FALSE(result.has_value());
                REQUIRE(result.error() == alloc_error::NotEnoughMemory);
            }
        }
    }
}

SCENARIO("an allocation requests alignment larger than the OS page size") {
    GIVEN("an arena large enough for an over-aligned allocation") {
        const std::size_t alignment = std::bit_ceil(stdan::memory::PAGE_SIZE + 1);
        auto arena_result = create_arena(alignment * 2);
        REQUIRE(arena_result.has_value());

        WHEN("a byte is allocated with the larger alignment") {
            auto result = arena_alloc(arena_result.value().get(), 1, alignment);

            THEN("the returned address has the requested alignment") {
                REQUIRE(result.has_value());
                REQUIRE(reinterpret_cast<std::uintptr_t>(result.value()) % alignment == 0);
            }
        }
    }
}

SCENARIO("an over-aligned object is constructed in an arena") {
    GIVEN("an arena large enough for the object") {
        constexpr std::size_t alignment = alignof(over_aligned_value);
        auto arena_result = create_arena(alignment * 2);
        REQUIRE(arena_result.has_value());

        WHEN("the object is constructed") {
            auto result = arena_construct<over_aligned_value>(arena_result.value().get(), 42);

            THEN("the object is aligned and initialized") {
                REQUIRE(result.has_value());
                REQUIRE(reinterpret_cast<std::uintptr_t>(result.value()) % alignment == 0);
                REQUIRE(result.value()->value == 42);
            }
        }
    }
}

SCENARIO("an owned arena is explicitly released") {
    GIVEN("an arena removed from its automatic owner") {
        auto arena_result = create_arena(4096);
        REQUIRE(arena_result.has_value());
        arena* raw_arena = arena_result.value().release();

        WHEN("the arena is released") {
            auto result = arena_release(raw_arena);

            THEN("release succeeds") {
                REQUIRE(result.has_value());
            }
        }
    }
}

SCENARIO("a null arena is released") {
    GIVEN("a null arena pointer") {
        WHEN("the arena is released") {
            auto result = arena_release(nullptr);

            THEN("release succeeds as an idempotent no-op") {
                REQUIRE(result.has_value());
            }
        }
    }
}
