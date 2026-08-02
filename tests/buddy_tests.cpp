#include "memory/buddy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

using stdan::memory::buddy_alloc;

namespace {
template<std::size_t TotalSize, std::size_t Depth>
concept valid_buddy_geometry = requires { typename buddy_alloc<TotalSize, Depth>; };

struct alignas(64) over_aligned_value {
    std::uint64_t value;
};

[[nodiscard]] bool regions_are_disjoint(
    const void* lhs,
    std::size_t lhs_size,
    const void* rhs,
    std::size_t rhs_size) {
    const auto lhs_begin = reinterpret_cast<std::uintptr_t>(lhs);
    const auto rhs_begin = reinterpret_cast<std::uintptr_t>(rhs);
    return lhs_begin + lhs_size <= rhs_begin || rhs_begin + rhs_size <= lhs_begin;
}

static_assert(valid_buddy_geometry<64, 3>);
static_assert(!valid_buddy_geometry<0, 0>);
static_assert(!valid_buddy_geometry<65, 3>);
static_assert(!valid_buddy_geometry<
    64,
    std::numeric_limits<unsigned long long>::digits - 1>);
}

SCENARIO("an allocation prevents allocation of an overlapping ancestor block") {
    GIVEN("one half of a buddy allocator is allocated") {
        buddy_alloc<16, 1> allocator;
        void* half = allocator.alloc(8, 1);
        REQUIRE(half != nullptr);

        WHEN("the entire pool is requested") {
            void* whole = allocator.alloc(16, 1);

            THEN("the overlapping allocation is rejected") {
                REQUIRE(whole == nullptr);
            }
        }
    }
}

SCENARIO("an allocation prevents allocation of an overlapping descendant block") {
    GIVEN("the entire buddy allocator is allocated") {
        buddy_alloc<16, 1> allocator;
        void* whole = allocator.alloc(16, 1);
        REQUIRE(whole != nullptr);

        WHEN("a smaller block is requested") {
            void* half = allocator.alloc(8, 1);

            THEN("the overlapping allocation is rejected") {
                REQUIRE(half == nullptr);
            }
        }
    }
}

SCENARIO("deallocating one buddy preserves the other buddy") {
    GIVEN("both minimum-sized buddy blocks are allocated") {
        buddy_alloc<16, 1> allocator;
        void* first = allocator.alloc(8, 1);
        void* second = allocator.alloc(8, 1);
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        REQUIRE(regions_are_disjoint(first, 8, second, 8));

        auto* second_bytes = static_cast<std::byte*>(second);
        std::fill_n(second_bytes, 8, std::byte{0x5a});

        WHEN("only one buddy is deallocated") {
            allocator.dealloc(first);

            THEN("the pool cannot coalesce across the live buddy") {
                REQUIRE(allocator.alloc(16, 1) == nullptr);
                for(std::size_t i = 0; i < 8; ++i) {
                    REQUIRE(second_bytes[i] == std::byte{0x5a});
                }
            }

            AND_WHEN("the remaining buddy is also deallocated") {
                allocator.dealloc(second);

                THEN("the entire pool can be allocated again") {
                    REQUIRE(allocator.alloc(16, 1) != nullptr);
                }
            }
        }
    }
}

SCENARIO("minimum-sized allocations exhaust and reuse the pool without overlap") {
    GIVEN("every minimum-sized block is allocated") {
        buddy_alloc<32, 2> allocator;
        std::array<void*, 4> allocations{};

        for(std::size_t i = 0; i < allocations.size(); ++i) {
            allocations[i] = allocator.alloc(8, 1);
            REQUIRE(allocations[i] != nullptr);
            std::fill_n(
                static_cast<std::byte*>(allocations[i]),
                8,
                std::byte{static_cast<unsigned char>(i + 1)});
        }

        for(std::size_t i = 0; i < allocations.size(); ++i) {
            for(std::size_t j = i + 1; j < allocations.size(); ++j) {
                REQUIRE(regions_are_disjoint(allocations[i], 8, allocations[j], 8));
            }
        }

        WHEN("another minimum-sized block is requested") {
            THEN("allocation fails because the pool is exhausted") {
                REQUIRE(allocator.alloc(8, 1) == nullptr);
            }
        }

        WHEN("one block is deallocated and another is requested") {
            constexpr std::size_t released_index = 1;
            allocator.dealloc(allocations[released_index]);
            void* replacement = allocator.alloc(8, 1);

            THEN("one block becomes available without corrupting live allocations") {
                REQUIRE(replacement != nullptr);
                for(std::size_t i = 0; i < allocations.size(); ++i) {
                    if(i == released_index) { continue; }
                    REQUIRE(regions_are_disjoint(replacement, 8, allocations[i], 8));
                    const auto expected = std::byte{static_cast<unsigned char>(i + 1)};
                    const auto* bytes = static_cast<const std::byte*>(allocations[i]);
                    for(std::size_t byte = 0; byte < 8; ++byte) {
                        REQUIRE(bytes[byte] == expected);
                    }
                }
            }
        }
    }
}

SCENARIO("allocations of different sizes expose disjoint requested regions") {
    GIVEN("several live allocations with different sizes") {
        buddy_alloc<128, 4> allocator;
        void* small = allocator.alloc(5, 1);
        void* medium = allocator.alloc(9, 1);
        void* large = allocator.alloc(17, 1);

        THEN("every requested region is disjoint from every other region") {
            REQUIRE(small != nullptr);
            REQUIRE(medium != nullptr);
            REQUIRE(large != nullptr);
            REQUIRE(regions_are_disjoint(small, 5, medium, 9));
            REQUIRE(regions_are_disjoint(small, 5, large, 17));
            REQUIRE(regions_are_disjoint(medium, 9, large, 17));
        }
    }
}

SCENARIO("every explicitly aligned allocation satisfies its alignment") {
    GIVEN("a pool with room for several explicitly aligned allocations") {
        buddy_alloc<256, 5> allocator;
        std::array<void*, 3> allocations{};

        WHEN("several small allocations request 64-byte alignment") {
            for(void*& allocation : allocations) {
                allocation = allocator.alloc(1, 64);
            }

            THEN("every allocation succeeds at a distinct aligned address") {
                for(void* allocation : allocations) {
                    REQUIRE(allocation != nullptr);
                    REQUIRE(reinterpret_cast<std::uintptr_t>(allocation) % 64 == 0);
                }
                for(std::size_t i = 0; i < allocations.size(); ++i) {
                    for(std::size_t j = i + 1; j < allocations.size(); ++j) {
                        REQUIRE(regions_are_disjoint(
                            allocations[i], 1, allocations[j], 1));
                    }
                }
            }
        }
    }
}

SCENARIO("a buddy allocator honors ordinary and over-aligned object storage") {
    GIVEN("a buddy allocator") {
        buddy_alloc<256, 5> allocator;

        WHEN("ordinary storage is requested") {
            void* ordinary = allocator.alloc(1);

            THEN("the address has the default alignment") {
                REQUIRE(ordinary != nullptr);
                REQUIRE(reinterpret_cast<std::uintptr_t>(ordinary)
                    % alignof(std::max_align_t) == 0);
            }
        }

        WHEN("an over-aligned object is constructed in allocated storage") {
            void* raw = allocator.alloc(
                sizeof(over_aligned_value), alignof(over_aligned_value));
            REQUIRE(raw != nullptr);
            over_aligned_value* value = std::construct_at(
                static_cast<over_aligned_value*>(raw), over_aligned_value{42});

            THEN("the object has the requested alignment and remains usable") {
                REQUIRE(reinterpret_cast<std::uintptr_t>(value)
                    % alignof(over_aligned_value) == 0);
                REQUIRE(value->value == 42);
            }

            std::destroy_at(value);
        }
    }
}

SCENARIO("a buddy allocator rejects unsupported alignment requests") {
    GIVEN("a buddy allocator") {
        buddy_alloc<64, 3> allocator;

        THEN("zero alignment is rejected") {
            REQUIRE(allocator.alloc(8, 0) == nullptr);
        }

        THEN("non-power-of-two alignment is rejected") {
            REQUIRE(allocator.alloc(8, 3) == nullptr);
        }

        THEN("invalid allocation sizes and excessive alignment are rejected") {
            REQUIRE(allocator.alloc(0, 1) == nullptr);
            REQUIRE(allocator.alloc(65, 1) == nullptr);
            REQUIRE(allocator.alloc(8, 128) == nullptr);
        }
    }
}
