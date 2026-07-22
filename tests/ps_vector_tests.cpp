#include <catch2/catch_test_macros.hpp>

#include "storage/ps_vector.hpp"

#include "test_base.hpp"

#include <atomic>
#include <cstddef>
#include <stdexcept>

namespace {
using test_support::move_only_value;
using test_support::throwing_copy_value;
using test_support::tracked_value;

template<typename T>
T& live_value_at(stdan::storage::ps_vector<T>& values, std::size_t index) {
    return *values.get(index);
}
} // namespace

SCENARIO("resize preconstructs reusable storage without creating logical elements") {
    GIVEN("an empty vector with reserved capacity") {
        stdan::storage::ps_vector<int> values(4);

        WHEN("storage is preconstructed within the current allocation") {
            values.resize(2);

            THEN("the vector remains logically empty") {
                REQUIRE(values.empty());
                REQUIRE(values.size() == 0);
                REQUIRE(values.capacity() >= 4);
                REQUIRE_THROWS_AS(values.get(0), std::out_of_range);
            }

            THEN("appends reuse the preconstructed storage") {
                values.append(7);
                values.append(11);

                REQUIRE(values.size() == 2);
                REQUIRE(live_value_at(values, 0) == 7);
                REQUIRE(live_value_at(values, 1) == 11);
            }
        }

        WHEN("storage is preconstructed beyond the current allocation") {
            values.resize(5);

            THEN("the allocation grows but the vector remains logically empty") {
                REQUIRE(values.empty());
                REQUIRE(values.capacity() >= 5);
                REQUIRE_THROWS_AS(values.get(0), std::out_of_range);
            }
        }
    }
}

SCENARIO("resize truncates logical elements when physical storage shrinks") {
    GIVEN("a vector containing several values") {
        stdan::storage::ps_vector<int> values(4);
        values.append(7);
        values.append(11);
        values.append(13);

        WHEN("the physical storage is resized below the logical size") {
            values.resize(2);

            THEN("only elements that still have backing storage remain live") {
                REQUIRE(values.size() == 2);
                REQUIRE(live_value_at(values, 0) == 7);
                REQUIRE(live_value_at(values, 1) == 11);
                REQUIRE_THROWS_AS(values.get(2), std::out_of_range);
            }
        }
    }
}

SCENARIO("append grows a vector beyond its current capacity") {
    GIVEN("a vector filled to its reported capacity") {
        stdan::storage::ps_vector<int> values(2);
        const auto initial_capacity = values.capacity();
        REQUIRE(initial_capacity >= 2);

        for(std::size_t index = 0; index < initial_capacity; ++index) {
            values.append(static_cast<int>(index + 1));
        }
        REQUIRE(values.full());

        WHEN("an rvalue is appended") {
            values.append(999);

            THEN("the vector grows without losing existing values") {
                REQUIRE(values.size() == initial_capacity + 1);
                REQUIRE(values.capacity() >= values.size());
                for(std::size_t index = 0; index < initial_capacity; ++index) {
                    REQUIRE(live_value_at(values, index) == static_cast<int>(index + 1));
                }
                REQUIRE(live_value_at(values, initial_capacity) == 999);
            }
        }

        WHEN("an lvalue is appended") {
            const int appended = 999;
            values.append(appended);

            THEN("the vector grows without losing existing values") {
                REQUIRE(values.size() == initial_capacity + 1);
                REQUIRE(values.capacity() >= values.size());
                for(std::size_t index = 0; index < initial_capacity; ++index) {
                    REQUIRE(live_value_at(values, index) == static_cast<int>(index + 1));
                }
                REQUIRE(live_value_at(values, initial_capacity) == appended);
            }
        }
    }
}

SCENARIO("remove swaps the last live element into the removed slot") {
    GIVEN("a vector with multiple values") {
        stdan::storage::ps_vector<int> values(4);
        values.append(7);
        values.append(11);
        values.append(13);

        WHEN("the middle element is removed") {
            values.remove(1);

            THEN("the last live value occupies the vacated logical slot") {
                REQUIRE(values.size() == 2);
                REQUIRE(live_value_at(values, 0) == 7);
                REQUIRE(live_value_at(values, 1) == 13);
                REQUIRE_THROWS_AS(values.get(2), std::out_of_range);
            }
        }

        WHEN("the last element is removed") {
            values.remove(2);

            THEN("the preceding elements retain their order") {
                REQUIRE(values.size() == 2);
                REQUIRE(live_value_at(values, 0) == 7);
                REQUIRE(live_value_at(values, 1) == 11);
            }
        }
    }
}

SCENARIO("index_of searches only the live range") {
    GIVEN("a vector with a removed value in reusable storage") {
        stdan::storage::ps_vector<int> values(3);
        values.append(5);
        values.append(9);
        values.append(42);
        values.remove(2);

        WHEN("a live value is queried") {
            auto index = values.index_of(9);

            THEN("the matching logical index is returned") {
                REQUIRE(index.has_value());
                REQUIRE(index.value() == 1);
            }
        }

        WHEN("the removed value is queried") {
            auto index = values.index_of(42);

            THEN("the inactive backing slot is ignored") {
                REQUIRE_FALSE(index.has_value());
            }
        }
    }
}

SCENARIO("remove retains constructed backing objects for reuse") {
    GIVEN("a vector containing tracked values") {
        tracked_value::reset();
        stdan::storage::ps_vector<tracked_value> values(4);
        values.append(tracked_value{1});
        values.append(tracked_value{2});
        values.append(tracked_value{3});

        REQUIRE(values.size() == 3);
        REQUIRE(tracked_value::live == 3);

        WHEN("the middle value is removed") {
            const auto live_before_remove = tracked_value::live;
            values.remove(1);

            THEN("logical compaction does not end a backing object's lifetime") {
                REQUIRE(values.size() == 2);
                REQUIRE(tracked_value::live == live_before_remove);
                REQUIRE(live_value_at(values, 0).value == 1);
                REQUIRE(live_value_at(values, 1).value == 3);
                REQUIRE_THROWS_AS(values.get(2), std::out_of_range);
            }

            THEN("the inactive backing object is reused by a later append") {
                tracked_value replacement{99};
                const auto live_before_append = tracked_value::live;
                values.append(std::move(replacement));

                REQUIRE(values.size() == 3);
                REQUIRE(tracked_value::live == live_before_append);
                REQUIRE(live_value_at(values, 2).value == 99);
            }
        }
    }
}

SCENARIO("inactive backing objects are destroyed with the vector") {
    tracked_value::reset();

    WHEN("a vector containing a removed value leaves scope") {
        {
            stdan::storage::ps_vector<tracked_value> values(3);
            values.append(tracked_value{1});
            values.append(tracked_value{2});
            values.append(tracked_value{3});
            values.remove(1);

            REQUIRE(values.size() == 2);
            REQUIRE(tracked_value::live == 3);
        }

        THEN("all constructed backing objects are destroyed") {
            REQUIRE(tracked_value::live == 0);
        }
    }
}

SCENARIO("move-only values survive pop-swap operations") {
    GIVEN("a vector storing move-only values") {
        stdan::storage::ps_vector<move_only_value> values(3);
        move_only_value first{7};
        move_only_value second{11};

        values.append(std::move(first));
        values.append(std::move(second));

        REQUIRE(first.value == nullptr);
        REQUIRE(second.value == nullptr);
        REQUIRE(values.size() == 2);

        WHEN("the first element is removed") {
            values.remove(0);

            THEN("the remaining value is preserved and compacted") {
                REQUIRE(values.size() == 1);
                REQUIRE(live_value_at(values, 0).value != nullptr);
                REQUIRE(*live_value_at(values, 0).value == 11);
            }

            THEN("a new move-only value reuses the inactive backing slot") {
                move_only_value third{13};
                values.append(std::move(third));

                REQUIRE(values.size() == 2);
                REQUIRE(third.value == nullptr);
                REQUIRE(live_value_at(values, 1).value != nullptr);
                REQUIRE(*live_value_at(values, 1).value == 13);
            }
        }
    }
}

SCENARIO("append propagates copy-assignment failures without changing logical size") {
    GIVEN("a vector with reusable preconstructed storage") {
        throwing_copy_value::throw_on_copy = false;
        stdan::storage::ps_vector<throwing_copy_value> values(3);
        values.resize(2);
        values.append(throwing_copy_value{42});
        values.append(throwing_copy_value{77});
        values.remove(1);
        REQUIRE(values.size() == 1);

        throwing_copy_value source{99};

        WHEN("copy assignment into the reusable slot throws") {
            throwing_copy_value::throw_on_copy = true;

            THEN("the exception propagates and the logical range remains unchanged") {
                REQUIRE_THROWS_AS(values.append(source), std::runtime_error);
                REQUIRE(values.size() == 1);
                REQUIRE(live_value_at(values, 0).value == 42);

                throwing_copy_value::throw_on_copy = false;
                throwing_copy_value replacement{19};
                REQUIRE_NOTHROW(values.append(replacement));
                REQUIRE(values.size() == 2);
                REQUIRE(live_value_at(values, 1).value == 19);
            }
        }
    }
}

SCENARIO("for_each ignores constructed storage beyond the live range") {
    GIVEN("a vector with inactive preconstructed elements") {
        stdan::storage::ps_vector<int> values(6);
        values.resize(4);
        values.append(7);
        values.append(11);
        values.append(13);
        values.append(17);
        values.remove(3);
        values.remove(2);
        REQUIRE(values.size() == 2);

        std::atomic<int> total{0};

        WHEN("par-unseq for_each is invoked") {
            values.for_each_par_unseq([&total](int& value) {
                total.fetch_add(value, std::memory_order_relaxed);
                value *= 2;
            });

            THEN("only live elements are processed") {
                REQUIRE(total.load(std::memory_order_relaxed) == 18);
                REQUIRE(values.size() == 2);
                REQUIRE(live_value_at(values, 0) == 14);
                REQUIRE(live_value_at(values, 1) == 22);
            }
        }
    }
}

SCENARIO("moving a ps_vector transfers its logical range") {
    GIVEN("a vector containing live values") {
        stdan::storage::ps_vector<int> source(3);
        source.append(7);
        source.append(11);

        WHEN("the vector is move-constructed") {
            stdan::storage::ps_vector<int> destination(std::move(source));

            THEN("the destination owns the values and the source is logically empty") {
                REQUIRE(destination.size() == 2);
                REQUIRE(live_value_at(destination, 0) == 7);
                REQUIRE(live_value_at(destination, 1) == 11);
                REQUIRE(source.empty());
                REQUIRE(source.begin() == source.end());
            }
        }
    }
}
