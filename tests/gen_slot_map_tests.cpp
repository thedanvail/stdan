#include <catch2/catch_test_macros.hpp>

#include "storage/gen_slot_map.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>

namespace {
using int_map = stdan::storage::generational_slot_map<int>;

struct tracked_value {
    inline static int live = 0;
    inline static int destroyed = 0;

    int value;

    explicit tracked_value(int initial) noexcept
        : value(initial) {
        ++live;
    }

    tracked_value(const tracked_value& other)
        : value(other.value) {
        ++live;
    }

    tracked_value(tracked_value&& other) noexcept
        : value(other.value) {
        ++live;
        other.value = -1;
    }

    ~tracked_value() {
        --live;
        ++destroyed;
    }

    static void reset_counts() noexcept {
        live = 0;
        destroyed = 0;
    }
};

struct move_only_value {
    std::unique_ptr<int> value;

    explicit move_only_value(int initial)
        : value(std::make_unique<int>(initial)) {}

    move_only_value(move_only_value&&) noexcept = default;
    move_only_value& operator=(move_only_value&&) noexcept = default;
    move_only_value(const move_only_value&) = delete;
    move_only_value& operator=(const move_only_value&) = delete;
};

struct throwing_copy_value {
    inline static bool throw_on_copy = false;

    int value;

    explicit throwing_copy_value(int initial) noexcept
        : value(initial) {}

    throwing_copy_value(const throwing_copy_value& other)
        : value(other.value) {
        if(throw_on_copy) { throw std::runtime_error("copy failed"); }
    }

    throwing_copy_value(throwing_copy_value&&) noexcept = default;
};
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
                REQUIRE(*values.get(key) == 7);
            }
        }
    }

    GIVEN("an empty map and a move-only value") {
        stdan::storage::generational_slot_map<move_only_value> values;
        move_only_value source{11};

        WHEN("the value is inserted as an rvalue") {
            const auto key = values.emplace_back(static_cast<move_only_value&&>(source));

            THEN("ownership is transferred into the map") {
                REQUIRE(source.value == nullptr);
                REQUIRE(values.size() == 1);
                REQUIRE(values.get(key) != nullptr);
                REQUIRE(*values.get(key)->value == 11);
            }
        }
    }
}

SCENARIO("a generational slot map grows beyond its initial allocation") {
    GIVEN("a map containing non-trivial values") {
        tracked_value::reset_counts();
        stdan::storage::generational_slot_map<tracked_value> values;

        WHEN("enough values are inserted to force repeated growth") {
            for(int value = 0; value < 128; ++value) {
                values.emplace_back(tracked_value{value});
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
        tracked_value::reset_counts();
        stdan::storage::generational_slot_map<tracked_value> values;
        const auto key = values.emplace_back(tracked_value{7});
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
                REQUIRE(*values.get(current_key) == 11);
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

SCENARIO("a generational slot map is accessed through const qualification") {
    GIVEN("a const view of a map containing a value") {
        int_map values;
        const auto key = values.insert(29);
        const int_map& const_values = values;

        WHEN("the value is looked up") {
            auto pointer = const_values.get(key);

            THEN("a readable const transient pointer is returned") {
                REQUIRE(pointer != nullptr);
                const int observed = static_cast<decltype(pointer)&&>(pointer).apply_non_null([](const int& value) { return value; });
                REQUIRE(observed == 29);
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
        tracked_value::reset_counts();
        stdan::storage::generational_slot_map<tracked_value> values;
        const auto first = values.emplace_back(tracked_value{1});
        values.emplace_back(tracked_value{2});
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
                const auto key = values.emplace_back(tracked_value{3});
                REQUIRE(values.size() == 1);
                REQUIRE(values.get(key) != nullptr);
                REQUIRE(values.get(key)->value == 3);
            }
        }
    }
}

SCENARIO("value construction throws during insertion") {
    GIVEN("a map with a reusable free slot") {
        stdan::storage::generational_slot_map<throwing_copy_value> values;
        const auto removed = values.emplace_back(throwing_copy_value{1});
        REQUIRE(values.remove(removed));
        throwing_copy_value source{2};
        throwing_copy_value::throw_on_copy = true;

        WHEN("construction in the free slot throws") {
            REQUIRE_THROWS_AS(values.insert(source), std::runtime_error);
            throwing_copy_value::throw_on_copy = false;

            THEN("the map remains empty") {
                REQUIRE(values.empty());
                REQUIRE(values.size() == 0);
            }

            THEN("the reserved slot remains reusable") {
                const auto replacement = values.insert(source);
                REQUIRE(replacement.index == removed.index);
                REQUIRE(values.size() == 1);
            }
        }
    }
}

SCENARIO("a generational slot map is destroyed") {
    GIVEN("a map containing live tracked values") {
        tracked_value::reset_counts();

        WHEN("the map leaves scope") {
            {
                stdan::storage::generational_slot_map<tracked_value> values;
                values.emplace_back(tracked_value{1});
                values.emplace_back(tracked_value{2});
                REQUIRE(tracked_value::live == 2);
            }

            THEN("every live value is destroyed") {
                REQUIRE(tracked_value::live == 0);
            }
        }
    }
}
