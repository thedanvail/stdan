#include "memory/arena.hpp"
#include "storage/arena_array.hpp"

#include <catch2/catch_test_macros.hpp>

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

    struct arena_array_fixture {
        static stdan::memory::arena* require_arena(std::size_t reserve_size = 4096) {
            auto result = stdan::memory::create_arena(reserve_size);
            REQUIRE(result.has_value());
            return result.value();
        }

        template<typename T, std::size_t ElementCapacity>
            stdan::storage::arena_array<T, ElementCapacity> make_values(std::size_t reserve_size = 4096) {
                return stdan::storage::arena_array<T, ElementCapacity>(require_arena(reserve_size));
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

SCENARIO_METHOD(arena_array_fixture, "an arena_array is created with a null arena") {
    GIVEN("a null arena pointer") {
        stdan::storage::arena_array<int, 3> values(static_cast<stdan::memory::arena*>(nullptr));

        THEN("the array reports zero capacity and stays empty") {
            REQUIRE(values.size() == 0);
            REQUIRE(values.capacity() == 0);
        }

        WHEN("an element is appended") {
            values.emplace_back(42);

            THEN("the append is ignored") {
                REQUIRE(values.size() == 0);
                REQUIRE_FALSE(values.get(0).has_value());
            }
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
                REQUIRE(values.get(0).has_value());
                REQUIRE(values.get(1).has_value());
                REQUIRE(values.get(0)->get() == 7);
                REQUIRE(values.get(1)->get() == 11);
            }
        }

        WHEN("a slot beyond the live range is queried") {
            values.emplace_back(7);

            THEN("the lookup reports that no live value exists there") {
                REQUIRE(values.size() == 1);
                REQUIRE_FALSE(values.get(1).has_value());
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
                REQUIRE(values.get(0).has_value());
                REQUIRE(values.get(1).has_value());
                REQUIRE(values.get(0)->get() == 7);
                REQUIRE(values.get(1)->get() == 11);
            }
        }
    }
}

SCENARIO_METHOD(arena_array_fixture, "resetting an arena_array") {
    GIVEN("an integer array with live elements") {
        auto values = make_values<int, 3>();
        values.emplace_back(7);
        values.emplace_back(11);

        WHEN("the array is reset") {
            values.reset();

            THEN("it becomes empty") {
                REQUIRE(values.size() == 0);
                REQUIRE_FALSE(values.get(0).has_value());
                REQUIRE_FALSE(values.get(1).has_value());
            }

            THEN("new values can be written from the beginning again") {
                values.emplace_back(99);

                REQUIRE(values.size() == 1);
                REQUIRE(values.get(0).has_value());
                REQUIRE(values.get(0)->get() == 99);
            }
        }
    }

    GIVEN("a tracked array with live non-trivial elements") {
        tracked_value::destructor_calls = 0;
        tracked_value::live_instances = 0;

        auto values = make_values<tracked_value, 3>();
        values.emplace_back(tracked_value{7});
        values.emplace_back(tracked_value{11});

        auto first = values.get(0);
        auto second = values.get(1);
        REQUIRE(first.has_value());
        REQUIRE(second.has_value());
        REQUIRE(first->get().value == 7);
        REQUIRE(second->get().value == 11);
        REQUIRE(values.size() == 2);
        REQUIRE(tracked_value::live_instances == 2);

        const auto destructor_calls_before_reset = tracked_value::destructor_calls;

        WHEN("the tracked array is reset") {
            values.reset();

            THEN("all live tracked values are destroyed and the array becomes empty") {
                REQUIRE(tracked_value::destructor_calls == destructor_calls_before_reset + 2);
                REQUIRE(tracked_value::live_instances == 0);
                REQUIRE(values.size() == 0);
                REQUIRE_FALSE(values.get(0).has_value());
                REQUIRE_FALSE(values.get(1).has_value());
            }

            THEN("the array storage can be reused from the beginning") {
                values.emplace_back(tracked_value{99});

                REQUIRE(values.size() == 1);
                REQUIRE(values.get(0).has_value());
                REQUIRE(values.get(0)->get().value == 99);
                REQUIRE(tracked_value::live_instances == 1);
            }
        }
    }
}

SCENARIO_METHOD(arena_array_fixture, "an arena_array holding non-trivial values is destroyed") {
    GIVEN("a tracked type with two live instances") {
        tracked_value::destructor_calls = 0;
        tracked_value::live_instances = 0;

        WHEN("the array goes out of scope") { {
                auto values = make_values<tracked_value, 4>();
                values.emplace_back(tracked_value{7});
                values.emplace_back(tracked_value{11});

                REQUIRE(values.size() == 2);
                REQUIRE(tracked_value::live_instances == 2);
                tracked_value::destructor_calls = 0;
            }

            THEN("the live elements are destroyed") {
                REQUIRE(tracked_value::destructor_calls == 2);
                REQUIRE(tracked_value::destructor_calls >= 2);
                REQUIRE(tracked_value::live_instances == 0);
            }
        }
    }
}
