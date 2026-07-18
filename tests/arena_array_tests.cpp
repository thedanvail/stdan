#include "storage/arena_array.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <utility>

namespace {
    struct tracked_value {
        inline static int destructor_calls = 0;
        inline static int live_instances = 0;

        int value = 0;

        tracked_value() { ++live_instances; }
        explicit tracked_value(int v)
            : value(v) {
            ++live_instances;
        }

        tracked_value(const tracked_value& other)
            : value(other.value) {
            ++live_instances;
        }

        tracked_value(tracked_value&& other) noexcept
            : value(other.value) {
            ++live_instances;
        }

        tracked_value& operator=(const tracked_value&) = default;
        tracked_value& operator=(tracked_value&&)      = default;

        ~tracked_value() {
            ++destructor_calls;
            --live_instances;
        }
    };

    struct alignas(8192) over_aligned_value {
        inline static int live_instances = 0;

        explicit over_aligned_value(int initial_value) noexcept
            : value(initial_value) {
            ++live_instances;
        }

        ~over_aligned_value() { --live_instances; }

        int value;
    };

    template<typename Ptr>
    auto ptr_or_null(Ptr ptr) {
        using result_type = decltype(std::move(ptr).apply([](auto* candidate) { return candidate; }));
        if(!ptr) { return static_cast<result_type>(nullptr); }
        return std::move(ptr).apply([](auto* candidate) { return candidate; });
    }

    struct arena_array_fixture {
        template<typename T, std::size_t ElementCapacity>
        stdan::storage::arena_array<T, ElementCapacity> make_values() {
            return stdan::storage::arena_array<T, ElementCapacity>();
        }
    };

} // namespace

SCENARIO_METHOD(arena_array_fixture, "an arena_array is created with a valid arena") {
    GIVEN("an array with room for three integers") {
        auto values = make_values<int, 3>();

        THEN("it starts empty and reports its configured capacity") {
            REQUIRE(values.size() == 0);
            REQUIRE(values.capacity() == 3);
        }
    }
}

SCENARIO_METHOD(arena_array_fixture, "values are appended to an arena_array") {
    GIVEN("an empty array with spare capacity") {
        auto values = make_values<int, 3>();

        WHEN("two values are appended") {
            values.emplace_back(7);
            values.emplace_back(11);

            THEN("the size grows and the values can be retrieved in order") {
                REQUIRE(values.size() == 2);
                REQUIRE(values.get(0) != nullptr);
                REQUIRE(values.get(1) != nullptr);
                REQUIRE(values.get(0) == 7);
                REQUIRE(values.get(1) == 11);
            }
        }

        WHEN("a slot beyond the live range is queried") {
            values.emplace_back(7);

            THEN("the lookup reports that no live value exists there") {
                REQUIRE(values.size() == 1);
                REQUIRE(ptr_or_null(values.get(1)) == nullptr);
            }
        }
    }
}

SCENARIO_METHOD(arena_array_fixture, "appending past capacity") {
    GIVEN("an array that is already full") {
        auto values = make_values<int, 2>();
        values.emplace_back(7);
        values.emplace_back(11);

        WHEN("another value is appended") {
            values.emplace_back(13);

            THEN("the array remains unchanged") {
                REQUIRE(values.size() == 2);
                int* first = ptr_or_null(values.get(0));
                int* second = ptr_or_null(values.get(1));
                REQUIRE(first != nullptr);
                REQUIRE(second != nullptr);
                REQUIRE(*first == 7);
                REQUIRE(*second == 11);
            }
        }
    }
}

SCENARIO_METHOD(arena_array_fixture, "resetting an arena_array") {
    GIVEN("an array with live elements") {
        auto values = make_values<int, 3>();
        values.emplace_back(7);
        values.emplace_back(11);

        WHEN("the array is reset") {
            values.reset();

            THEN("it becomes empty") {
                REQUIRE(values.size() == 0);
                REQUIRE(ptr_or_null(values.get(0)) == nullptr);
                REQUIRE(ptr_or_null(values.get(1)) == nullptr);
            }

            THEN("new values can be written from the beginning again") {
                values.emplace_back(99);

                REQUIRE(values.size() == 1);
                int* first = ptr_or_null(values.get(0));
                REQUIRE(first != nullptr);
                REQUIRE(*first == 99);
            }
        }
    }

    GIVEN("a tracked array with live non-trivial elements") {
        tracked_value::destructor_calls = 0;
        tracked_value::live_instances = 0;

        auto values = make_values<tracked_value, 3>();
        values.emplace_back(tracked_value{7});
        values.emplace_back(tracked_value{11});

        tracked_value* first = ptr_or_null(values.get(0));
        tracked_value* second = ptr_or_null(values.get(1));
        REQUIRE(first != nullptr);
        REQUIRE(second != nullptr);
        REQUIRE(first->value == 7);
        REQUIRE(second->value == 11);
        REQUIRE(values.size() == 2);
        REQUIRE(tracked_value::live_instances == 2);

        const auto destructor_calls_before_reset = tracked_value::destructor_calls;

        WHEN("the tracked array is reset") {
            values.reset();

            THEN("all live tracked values are destroyed and the array becomes empty") {
                REQUIRE(tracked_value::destructor_calls == destructor_calls_before_reset + 2);
                REQUIRE(tracked_value::live_instances == 0);
                REQUIRE(values.size() == 0);
                REQUIRE(ptr_or_null(values.get(0)) == nullptr);
                REQUIRE(ptr_or_null(values.get(1)) == nullptr);
            }

            THEN("the array storage can be reused from the beginning") {
                values.emplace_back(tracked_value{99});

                REQUIRE(values.size() == 1);
                tracked_value* first_again = ptr_or_null(values.get(0));
                REQUIRE(first_again != nullptr);
                REQUIRE(first_again->value == 99);
                REQUIRE(tracked_value::live_instances == 1);
            }
        }
    }
}

SCENARIO_METHOD(arena_array_fixture, "a zero-capacity arena_array remains empty") {
    GIVEN("an array with zero element capacity") {
        auto values = make_values<int, 0>();

        WHEN("an element is appended") {
            const bool appended = values.emplace_back(42);

            THEN("the append is rejected and no arena storage is needed") {
                REQUIRE_FALSE(appended);
                REQUIRE(values.capacity() == 0);
                REQUIRE(values.size() == 0);
                REQUIRE(ptr_or_null(values.get(0)) == nullptr);
            }
        }
    }
}

SCENARIO_METHOD(arena_array_fixture, "an arena_array stores over-aligned values") {
    GIVEN("an array with two over-aligned live values") {
        over_aligned_value::live_instances = 0;
        auto values = make_values<over_aligned_value, 2>();
        REQUIRE(values.emplace_back(7));
        REQUIRE(values.emplace_back(11));
        REQUIRE(over_aligned_value::live_instances == 2);

        WHEN("the values are accessed") {
            auto* first = ptr_or_null(values.get(0));
            auto* second = ptr_or_null(values.get(1));

            THEN("access uses the aligned address returned by the arena") {
                REQUIRE(first != nullptr);
                REQUIRE(second != nullptr);
                REQUIRE(reinterpret_cast<std::uintptr_t>(first) % alignof(over_aligned_value) == 0);
                REQUIRE(reinterpret_cast<std::uintptr_t>(second) % alignof(over_aligned_value) == 0);
                REQUIRE(first->value == 7);
                REQUIRE(second->value == 11);

                std::size_t applied = 0;
                values.apply([&applied](over_aligned_value* value) {
                    REQUIRE(reinterpret_cast<std::uintptr_t>(value) % alignof(over_aligned_value) == 0);
                    ++applied;
                });
                REQUIRE(applied == 2);
            }
        }

        WHEN("the array is reset") {
            values.reset();

            THEN("the aligned values are destroyed and storage can be reused") {
                REQUIRE(over_aligned_value::live_instances == 0);
                REQUIRE(values.size() == 0);
                REQUIRE(values.emplace_back(99));
                auto* first = ptr_or_null(values.get(0));
                REQUIRE(first != nullptr);
                REQUIRE(first->value == 99);
            }
        }
    }
}

SCENARIO_METHOD(arena_array_fixture, "an arena_array holding non-trivial values is destroyed") {
    GIVEN("a tracked type with two live instances") {
        tracked_value::destructor_calls = 0;
        tracked_value::live_instances = 0;

        WHEN("the array goes out of scope") {
            {
                auto values = make_values<tracked_value, 4>();
                values.emplace_back(tracked_value{7});
                values.emplace_back(tracked_value{11});

                REQUIRE(values.size() == 2);
                REQUIRE(tracked_value::live_instances == 2);
                tracked_value::destructor_calls = 0;
            }

            THEN("the live elements are destroyed") {
                REQUIRE(tracked_value::destructor_calls == 2);
                REQUIRE(tracked_value::live_instances == 0);
            }
        }
    }
}
