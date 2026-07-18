#include <catch2/catch_test_macros.hpp>

#include "storage/ps_vector.hpp"

#include "test_base.hpp"

#include <atomic>
#include <memory>
#include <stdexcept>

namespace {
using test_support::tracked_value;
using test_support::throwing_copy_value;
using test_support::move_only_value;
} // namespace

SCENARIO("resize grows default-constructible values within capacity") {
    GIVEN("a vector with spare capacity") {
        stdan::storage::ps_vector<int> values(4);

        WHEN("it is resized up within capacity") {
            values.resize(2);

            THEN("the size grows to the requested value") {
                REQUIRE(values.size() == 2);
                REQUIRE(values[0]     == 0);
                REQUIRE(values[1]     == 0);
            }
        }

        WHEN("it is resized beyond capacity") {
            values.resize(5);

            THEN("the vector grows to the requested size") {
                REQUIRE(values.size() == 5);
                REQUIRE(values[0]     == 0);
                REQUIRE(values[1]     == 0);
                REQUIRE(values[2]     == 0);
                REQUIRE(values[3]     == 0);
                REQUIRE(values[4]     == 0);
            }
        }
    }
}

SCENARIO("append stores values until the vector is full") {
    GIVEN("a vector with spare capacity") {
        stdan::storage::ps_vector<int> values(2);

        WHEN("values are appended") {
            values.append(7);
            values.append(11);

            THEN("the values are stored in order") {
                REQUIRE(values.size() == 2);
                REQUIRE(values[0] == 7);
                REQUIRE(values[1] == 11);
            }
        }

        WHEN("an append is attempted after capacity is reached") {
            values.append(7);
            values.append(11);
            values.append(13);

            THEN("the vector remains full and unchanged") {
                REQUIRE(values.size() == 2);
                REQUIRE(values[0] == 7);
                REQUIRE(values[1] == 11);
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

            THEN("the last live value is moved into the vacated slot") {
                REQUIRE(values.size() == 2);
                REQUIRE(values[0] == 7);
                REQUIRE(values[1] == 13);
            }
        }
    }
}

SCENARIO("index_of finds live elements and reports missing ones") {
    GIVEN("a vector with known values") {
        stdan::storage::ps_vector<int> values(3);
        values.append(5);
        values.append(9);

        WHEN("a present value is queried") {
            auto index = values.index_of(9);

            THEN("the matching index is returned") {
                REQUIRE(index.has_value());
                REQUIRE(index.value() == 1);
            }
        }

        WHEN("a missing value is queried") {
            auto index = values.index_of(42);

            THEN("the lookup reports failure") {
                REQUIRE_FALSE(index.has_value());
            }
        }
    }
}

SCENARIO("destroy removes elements and destroys their storage") {
    GIVEN("a vector containing tracked values") {
        tracked_value::reset();
        stdan::storage::ps_vector<tracked_value> values(4);
        values.append(tracked_value{1});
        values.append(tracked_value{2});
        values.append(tracked_value{3});

        REQUIRE(values.size() == 3);

        WHEN("the middle element is destroyed") {
            const auto destroyed_before = tracked_value::destroyed;
            values.destroy(1);

            THEN("the destructor runs and the remaining elements stay live") {
                REQUIRE(values.size() == 2);
                REQUIRE(tracked_value::destroyed == destroyed_before + 1);
                REQUIRE(values[0].value == 1);
                REQUIRE(values[1].value == 3);
            }

            THEN("the vacated slot can be reused without leaks") {
                tracked_value replacement{99};
                const auto destroyed_before_append = tracked_value::destroyed;
                values.append(std::move(replacement));

                REQUIRE(values.size() == 3);
                REQUIRE(values[2].value == 99);
                REQUIRE(tracked_value::destroyed == destroyed_before_append);
            }
        }
    }
}

SCENARIO("move-only values survive pop-swap operations") {
    GIVEN("a vector storing move-only values") {
        stdan::storage::ps_vector<move_only_value> values(3);
        move_only_value first{7};
        move_only_value second{11};

        values.append(static_cast<move_only_value&&>(first));
        values.append(static_cast<move_only_value&&>(second));

        REQUIRE(first.payload == nullptr);
        REQUIRE(second.payload == nullptr);
        REQUIRE(values.size() == 2);

        WHEN("the first element is removed") {
            values.remove(0);

            THEN("the remaining value is preserved and compacted") {
                REQUIRE(values.size() == 1);
                REQUIRE(values[0].payload != nullptr);
                REQUIRE(*values[0].payload == 11);
            }

            THEN("a new move-only value can reuse the freed slot") {
                move_only_value third{13};
                values.append(static_cast<move_only_value&&>(third));

                REQUIRE(values.size() == 2);
                REQUIRE(values[1].payload != nullptr);
                REQUIRE(*values[1].payload == 13);
            }
        }
    }
}

SCENARIO("append propagates copy-assignment failures without corrupting state") {
    GIVEN("a vector with spare pre-allocated capacity") {
        stdan::storage::ps_vector<throwing_copy_value> values(3);
        values.resize(2);
        values[0].value = 42;
        values[1].value = 77;
        values.remove(1);
        REQUIRE(values.size() == 1);

        throwing_copy_value source{99};

        WHEN("copy assignment throws during append") {
            throwing_copy_value::throw_on_copy = true;

            THEN("the exception propagates and the vector remains unchanged") {
                REQUIRE_THROWS_AS(values.append(source), std::runtime_error);
                REQUIRE(values.size() == 1);
                REQUIRE(values[0].value == 42);

                throwing_copy_value replacement{19};
                replacement.throw_on_copy = false;
                REQUIRE_NOTHROW(values.append(replacement));
                REQUIRE(values.size() == 2);
                REQUIRE(values[1].value == 19);
            }
        }
    }
}

SCENARIO("for_each ignores padding beyond the live range") {
    GIVEN("a vector with extra reserved elements") {
        stdan::storage::ps_vector<int> values(6);
        values.resize(4);
        values[0] = 7;
        values[1] = 11;
        values[2] = 13;
        values[3] = 17;
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
                REQUIRE(values[0] == 14);
                REQUIRE(values[1] == 22);
            }
        }
    }
}
