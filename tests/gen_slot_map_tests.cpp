#include <catch2/catch_test_macros.hpp>

#include "storage/gen_slot_map.hpp"

#include "test_base.hpp"

#include <cstdint>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using int_map = stdan::storage::generational_slot_map<int>;

static_assert(std::forward_iterator<int_map::iterator>);
static_assert(std::forward_iterator<int_map::const_iterator>);
static_assert(std::is_same_v<std::iter_reference_t<int_map::iterator>, int&>);
static_assert(std::is_same_v<std::iter_reference_t<int_map::const_iterator>, const int&>);
static_assert(std::is_constructible_v<int_map::const_iterator, int_map::iterator>);
static_assert(!std::is_constructible_v<int_map::iterator, int_map::const_iterator>);

using test_support::tracked_value;
using test_support::move_only_value;
using test_support::throwing_copy_value;
} // namespace

SCENARIO("a generational slot map is newly created") {
    GIVEN("an empty map") {
        int_map values;

        THEN("it reports no live values") {
            REQUIRE(values.empty());
            REQUIRE(values.size() == 0);
            REQUIRE(values.capacity() == 0);
        }
    }
}

SCENARIO("values are inserted into a generational slot map") {
    GIVEN("an empty map") {
        int_map values;
        const int source = 7;

        WHEN("an lvalue is inserted") {
            const auto key = values.insert(source);

            THEN("the value is live and retrievable") {
                REQUIRE_FALSE(values.empty());
                REQUIRE(values.size() == 1);
                REQUIRE(values.contains(key));
                REQUIRE(values.get(key) != nullptr);
                const int retrieved = values.get(key).apply_non_null([](int& candidate) {
                    return candidate;
                });
                REQUIRE(retrieved == 7);
            }
        }
    }

    GIVEN("an empty map and a move-only value") {
        stdan::storage::generational_slot_map<move_only_value> values;
        move_only_value source{11};

        WHEN("the value is inserted as an rvalue") {
            const auto key = values.insert(std::move(source));

            THEN("ownership is transferred into the map") {
                REQUIRE(source.value == nullptr);
                REQUIRE(values.size() == 1);
                REQUIRE(values.get(key) != nullptr);
                values.get(key).apply_non_null([](move_only_value& stored) {
                    REQUIRE(stored.value != nullptr);
                    REQUIRE(*stored.value == 11);
                });
            }
        }
    }
}

SCENARIO("a generational slot map grows beyond its initial allocation") {
    GIVEN("a map containing non-trivial values") {
        tracked_value::reset();
        stdan::storage::generational_slot_map<tracked_value> values;

        WHEN("enough values are inserted to force repeated growth") {
            tracked_value::reset();

            for(int value = 0; value < 128; ++value) {
                const int live_before = tracked_value::live;
                values.emplace(tracked_value{value});
                REQUIRE(tracked_value::live == live_before + 1);
            }

            THEN("every value remains live and retrievable") {
                REQUIRE(values.size() == 128);
                REQUIRE(tracked_value::live == 128);
            }
        }
    }
}

SCENARIO("keys are validated before lookup") {
    GIVEN("a map containing one value") {
        int_map values;
        const auto key = values.insert(7);

        WHEN("a key has an index outside the slot array") {
            const int_map::key invalid{key.index + 100, key.generation};

            THEN("lookup and containment reject it") {
                REQUIRE(values.get(invalid) == nullptr);
                REQUIRE_FALSE(values.contains(invalid));
            }
        }

        WHEN("a key has the wrong generation") {
            const int_map::key invalid{key.index, static_cast<std::uint32_t>(key.generation + 1)};

            THEN("lookup and containment reject it") {
                REQUIRE(values.get(invalid) == nullptr);
                REQUIRE_FALSE(values.contains(invalid));
            }
        }
    }
}

SCENARIO("a value is removed from a generational slot map") {
    GIVEN("a map containing one value") {
        int_map values;
        const auto key = values.insert(7);

        WHEN("the value is removed with its current key") {
            const bool removed = values.remove(key);

            THEN("the value is no longer live") {
                REQUIRE(removed);
                REQUIRE(values.empty());
                REQUIRE(values.size() == 0);
                REQUIRE_FALSE(values.contains(key));
                REQUIRE(values.get(key) == nullptr);
            }

            THEN("removing the same key again is rejected") {
                REQUIRE_FALSE(values.remove(key));
            }
        }
    }

    GIVEN("a map containing a tracked value") {
        tracked_value::reset();
        stdan::storage::generational_slot_map<tracked_value> values;
        const auto key = values.emplace(tracked_value{7});
        const int destroyed_before_remove = tracked_value::destroyed;

        WHEN("the value is removed") {
            REQUIRE(values.remove(key));

            THEN("the live object is destroyed exactly once") {
                REQUIRE(tracked_value::live == 0);
                REQUIRE(tracked_value::destroyed == destroyed_before_remove + 1);
            }
        }
    }
}

SCENARIO("a removed slot is reused") {
    GIVEN("a map with a removed value") {
        int_map values;
        const auto stale_key = values.insert(7);
        REQUIRE(values.remove(stale_key));

        WHEN("another value is inserted") {
            const auto current_key = values.insert(11);

            THEN("the free slot is reused with a new generation") {
                REQUIRE(current_key.index == stale_key.index);
                REQUIRE(current_key.generation != stale_key.generation);
                REQUIRE(values.size() == 1);
                REQUIRE(values.get(current_key) != nullptr);
                values.get(current_key).apply_non_null([](int& value) {
                    REQUIRE(value == 11);
                });
            }

            THEN("the stale key cannot access or remove the replacement") {
                REQUIRE(values.get(stale_key) == nullptr);
                REQUIRE_FALSE(values.contains(stale_key));
                REQUIRE_FALSE(values.remove(stale_key));
                REQUIRE(values.size() == 1);
            }
        }
    }
}

SCENARIO("multiple removed slots are reused") {
    GIVEN("a map with two slots removed in a known order") {
        int_map values;
        const auto first = values.insert(1);
        const auto second = values.insert(2);
        REQUIRE(values.remove(first));
        REQUIRE(values.remove(second));

        WHEN("two replacement values are inserted") {
            const auto first_replacement = values.insert(3);
            const auto second_replacement = values.insert(4);

            THEN("the free-list supplies both removed slots") {
                REQUIRE(first_replacement.index == second.index);
                REQUIRE(second_replacement.index == first.index);
                REQUIRE(values.size() == 2);
            }
        }
    }
}

SCENARIO("a generational slot map is iterated") {
    GIVEN("an empty map") {
        int_map values;

        THEN("its iterator range is empty") {
            REQUIRE(values.begin() == values.end());
            REQUIRE(values.cbegin() == values.cend());
        }
    }

    GIVEN("a map containing active and removed slots") {
        int_map values;
        values.insert(10);
        const auto removed = values.insert(20);
        values.insert(30);
        REQUIRE(values.remove(removed));

        WHEN("the map is traversed") {
            std::vector<int> observed;
            for(const int& value : values) { observed.push_back(value); }

            THEN("only active values are produced in slot order") {
                REQUIRE(observed == std::vector<int>{10, 30});
            }
        }

        WHEN("every active value is modified through an iterator") {
            for(int& value : values) { value *= 2; }

            THEN("the stored values are modified") {
                std::vector<int> observed;
                for(const int& value : values) { observed.push_back(value); }
                REQUIRE(observed == std::vector<int>{20, 60});
            }
        }
    }

    GIVEN("a map whose first and last physical slots are inactive") {
        int_map values;
        const auto first = values.insert(1);
        values.insert(2);
        const auto last = values.insert(3);
        REQUIRE(values.remove(first));
        REQUIRE(values.remove(last));

        WHEN("prefix and postfix increment are used") {
            auto current = values.begin();
            auto previous = current++;

            THEN("begin skips the leading hole and increment reaches the end") {
                REQUIRE(*previous == 2);
                REQUIRE(current == values.end());
            }
        }
    }

    GIVEN("a const map view containing active values") {
        int_map values;
        values.insert(5);
        values.insert(8);
        const int_map& const_values = values;

        WHEN("the const range is traversed") {
            std::vector<int> observed;
            for(const int& value : const_values) { observed.push_back(value); }

            THEN("all active values are readable") {
                int_map::const_iterator converted = values.begin();
                REQUIRE(observed == std::vector<int>{5, 8});
                REQUIRE(converted == const_values.begin());
                REQUIRE(values.begin() == const_values.begin());
                REQUIRE(const_values.begin() == values.begin());
                REQUIRE(const_values.begin() == const_values.cbegin());
                REQUIRE(const_values.end() == const_values.cend());
            }
        }
    }
}

SCENARIO("a generational slot map is accessed through const qualification") {
    GIVEN("a const view of a map containing a value") {
        int_map values;
        const auto key = values.insert(29);
        const int_map& const_values = values;

        WHEN("the value is looked up") {
            THEN("a readable const transient pointer is returned") {
                REQUIRE(const_values.get(key) != nullptr);
                const_values.get(key).apply_non_null([](const int& value) {
                    REQUIRE(value == 29);
                });
            }
        }
    }
}

SCENARIO("capacity is reserved for a generational slot map") {
    GIVEN("an empty map") {
        int_map values;

        WHEN("capacity is reserved") {
            values.reserve(32);

            THEN("the requested capacity is available without changing the live size") {
                REQUIRE(values.capacity() >= 32);
                REQUIRE(values.size() == 0);
                REQUIRE(values.empty());
            }
        }
    }
}

SCENARIO("a generational slot map is cleared") {
    GIVEN("a map containing live and removed values") {
        tracked_value::reset();
        stdan::storage::generational_slot_map<tracked_value> values;
        const auto first = values.emplace(tracked_value{1});
        values.emplace(tracked_value{2});
        REQUIRE(values.remove(first));
        REQUIRE(tracked_value::live == 1);

        WHEN("the map is cleared") {
            values.clear();

            THEN("all remaining values are destroyed and the map is logically empty") {
                REQUIRE(tracked_value::live == 0);
                REQUIRE(values.empty());
                REQUIRE(values.size() == 0);
            }

            THEN("new insertions work after the clear") {
                const auto key = values.emplace(tracked_value{3});
                REQUIRE(values.size() == 1);
                REQUIRE(values.get(key) != nullptr);
                values.get(key).apply_non_null([](tracked_value& stored) {
                    REQUIRE(stored.value == 3);
                });
            }
        }
    }
}

SCENARIO("value construction throws during insertion") {
    GIVEN("a map with a reusable free slot") {
        stdan::storage::generational_slot_map<throwing_copy_value> values;
        const auto removed = values.emplace(throwing_copy_value{1});
        REQUIRE(values.remove(removed));
        throwing_copy_value::throw_on_copy = true;

        WHEN("construction in the free slot throws") {
            throwing_copy_value source{2};
            REQUIRE_THROWS_AS(values.insert(source), std::runtime_error);
            REQUIRE(values.empty());
            REQUIRE(values.size() == 0);

            THEN("the map remains empty") {
                throwing_copy_value::throw_on_copy = false;
                REQUIRE(values.empty());
                REQUIRE(values.size() == 0);
            }

            THEN("the reserved slot remains reusable") {
                throwing_copy_value::throw_on_copy = false;
                const auto replacement = values.insert(source);
                REQUIRE(replacement.index == removed.index);
                REQUIRE(values.size() == 1);
            }
        }
    }
}

SCENARIO("a generational slot map is destroyed") {
    GIVEN("a map containing live tracked values") {
        tracked_value::reset();

        WHEN("the map leaves scope") {
            {
                stdan::storage::generational_slot_map<tracked_value> values;
                values.emplace(tracked_value{1});
                values.emplace(tracked_value{2});
                REQUIRE(tracked_value::live == 2);
            }

            THEN("every live value is destroyed") {
                REQUIRE(tracked_value::live == 0);
            }
        }
    }
}
