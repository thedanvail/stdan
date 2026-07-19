#include "storage/arena_array.hpp"

#include "test_base.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <type_traits>

namespace {
    using test_support::arena_tracked_value;

    struct alignas(8192) over_aligned_value {
        inline static int live_instances = 0;

        explicit over_aligned_value(int initial_value) noexcept
            : value(initial_value) {
            ++live_instances;
        }

        ~over_aligned_value() { --live_instances; }

        int value;
    };

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

        WHEN("values are accessed through a const array reference") {
            values.emplace_back(7);
            values.emplace_back(11);

            const auto& const_values = values;

            THEN("the const overload returns the live values") {
                static_assert(std::is_same_v<decltype(const_values.get(0)), stdan::storage::transient_ptr<const int>>);
                REQUIRE(const_values.get(0) != nullptr);
                REQUIRE(const_values.get(1) != nullptr);
                const_values.get(0).apply_non_null([](const int& value) { REQUIRE(value == 7); });
                const_values.get(1).apply_non_null([](const int& value) { REQUIRE(value == 11); });
            }
        }

        WHEN("a slot beyond the live range is queried") {
            values.emplace_back(7);

            THEN("the lookup reports that no live value exists there") {
                REQUIRE(values.size() == 1);
                REQUIRE(values.get(1) == nullptr);
            }
        }
    }
}

SCENARIO_METHOD(arena_array_fixture, "appending past capacity") {
    GIVEN("an array that is already full") {
        auto values = make_values<int, 2>();
        REQUIRE(values.emplace_back(7));
        REQUIRE(values.emplace_back(11));

        WHEN("another value is appended") {
            const bool appended = values.emplace_back(13);

            THEN("the array remains unchanged") {
                REQUIRE_FALSE(appended);
                REQUIRE(values.size() == 2);
                REQUIRE(values.get(0) != nullptr);
                REQUIRE(values.get(1) != nullptr);
                values.get(0).apply_non_null([](int& value) { REQUIRE(value == 7); });
                values.get(1).apply_non_null([](int& value) { REQUIRE(value == 11); });
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
                REQUIRE(values.get(0) == nullptr);
                REQUIRE(values.get(1) == nullptr);
            }

            THEN("new values can be written from the beginning again") {
                REQUIRE(values.emplace_back(99));

                REQUIRE(values.size() == 1);
                REQUIRE(values.get(0) != nullptr);
                values.get(0).apply_non_null([](int& value) { REQUIRE(value == 99); });
            }
        }
    }

    GIVEN("a tracked array with live non-trivial elements") {
        arena_tracked_value::reset();

        auto values = make_values<arena_tracked_value, 3>();
        REQUIRE(values.emplace_back(7));
        REQUIRE(values.emplace_back(11));

        std::size_t observed = 0;
        values.apply([&observed](arena_tracked_value* candidate) {
            REQUIRE(candidate != nullptr);
            ++observed;
        });
        REQUIRE(observed == 2);
        REQUIRE(values.size() == 2);
        REQUIRE(arena_tracked_value::live_instances == 2);

        WHEN("the tracked array is reset") {
            values.reset();

            THEN("all live tracked values are destroyed and the array becomes empty") {
                REQUIRE(arena_tracked_value::destructor_calls == 2);
                REQUIRE(arena_tracked_value::live_instances == 0);
                REQUIRE(values.size() == 0);
                REQUIRE(values.get(0) == nullptr);
                REQUIRE(values.get(1) == nullptr);
            }

            THEN("the array storage can be reused from the beginning") {
                REQUIRE(values.emplace_back(arena_tracked_value{99}));

                REQUIRE(values.size() == 1);
                REQUIRE(values.get(0) != nullptr);
                values.get(0).apply_non_null([](arena_tracked_value& value) {
                    REQUIRE(value.value == 99);
                });
                REQUIRE(arena_tracked_value::live_instances == 1);
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
                REQUIRE(values.get(0) == nullptr);
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
            THEN("access uses the aligned address returned by the arena") {
                REQUIRE(values.get(0) != nullptr);
                REQUIRE(values.get(1) != nullptr);
                values.get(0).apply_non_null([](over_aligned_value& value) {
                    REQUIRE(reinterpret_cast<std::uintptr_t>(std::addressof(value)) % alignof(over_aligned_value) == 0);
                    REQUIRE(value.value == 7);
                });
                values.get(1).apply_non_null([](over_aligned_value& value) {
                    REQUIRE(reinterpret_cast<std::uintptr_t>(std::addressof(value)) % alignof(over_aligned_value) == 0);
                    REQUIRE(value.value == 11);
                });

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
                REQUIRE(values.get(0) != nullptr);
                values.get(0).apply_non_null([](over_aligned_value& value) {
                    REQUIRE(value.value == 99);
                });
            }
        }
    }
}

SCENARIO_METHOD(arena_array_fixture, "an arena_array holding non-trivial values is destroyed") {
    GIVEN("a tracked type with two live instances") {
        arena_tracked_value::reset();

        WHEN("the array goes out of scope") {
            {
                auto values = make_values<arena_tracked_value, 4>();
                REQUIRE(values.emplace_back(arena_tracked_value{7}));
                REQUIRE(values.emplace_back(arena_tracked_value{11}));

                REQUIRE(values.size() == 2);
                REQUIRE(arena_tracked_value::live_instances == 2);
                arena_tracked_value::destructor_calls = 0;
            }

            THEN("the live elements are destroyed") {
                REQUIRE(arena_tracked_value::destructor_calls == 2);
                REQUIRE(arena_tracked_value::live_instances == 0);
            }
        }
    }
}
