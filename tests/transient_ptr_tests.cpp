#include <catch2/catch_test_macros.hpp>

#include "storage/transient_ptr.hpp"

#include <memory>
#include <type_traits>

namespace {
struct item {
    int value;

    void increment() noexcept { ++value; }
};

using item_ptr = stdan::storage::transient_ptr<item>;

static_assert(!std::is_copy_constructible_v<item_ptr>);
static_assert(!std::is_copy_assignable_v<item_ptr>);
static_assert(!std::is_move_constructible_v<item_ptr>);
static_assert(!std::is_move_assignable_v<item_ptr>);
} // namespace

SCENARIO("a transient pointer is created from an object") {
    GIVEN("an object and transient pointers created from its address and reference") {
        item value{7};
        item_ptr from_pointer{std::addressof(value)};
        item_ptr from_reference{value};

        THEN("both pointers report that they contain an object") {
            REQUIRE(static_cast<bool>(from_pointer));
            REQUIRE(static_cast<bool>(from_reference));
            REQUIRE_FALSE(from_pointer == nullptr);
            REQUIRE_FALSE(from_reference == nullptr);
        }
    }
}

SCENARIO("a transient pointer is null") {
    GIVEN("a transient pointer constructed from nullptr") {
        item_ptr pointer{nullptr};

        THEN("it reports that no object is present") {
            REQUIRE_FALSE(static_cast<bool>(pointer));
            REQUIRE(pointer == nullptr);
        }

        WHEN("apply is invoked") {
            item* observed = reinterpret_cast<item*>(1);
            static_cast<item_ptr&&>(pointer).apply([&observed](item* value) { observed = value; });

            THEN("the callback receives nullptr") {
                REQUIRE(observed == nullptr);
            }
        }

        WHEN("apply_if_non_null is invoked") {
            bool invoked = false;
            const bool applied = static_cast<item_ptr&&>(pointer).apply_if_non_null([&invoked](item&) { invoked = true; });

            THEN("the callback is skipped") {
                REQUIRE_FALSE(applied);
                REQUIRE_FALSE(invoked);
            }
        }
    }
}

SCENARIO("a transient pointer is consumed through direct access") {
    GIVEN("a transient pointer to a mutable object") {
        item value{7};
        item_ptr pointer{value};

        WHEN("the rvalue pointer is dereferenced") {
            *static_cast<item_ptr&&>(pointer) = item{11};

            THEN("the referenced object is modified") {
                REQUIRE(value.value == 11);
            }
        }
    }

    GIVEN("a transient pointer to an object with a member function") {
        item value{7};
        item_ptr pointer{value};

        WHEN("the rvalue arrow operator is used") {
            static_cast<item_ptr&&>(pointer)->increment();

            THEN("the member function is invoked on the referenced object") {
                REQUIRE(value.value == 8);
            }
        }
    }
}

SCENARIO("a transient pointer is consumed through callbacks") {
    GIVEN("a transient pointer to a mutable object") {
        item value{7};
        item_ptr pointer{value};

        WHEN("apply is invoked") {
            item* observed = nullptr;
            static_cast<item_ptr&&>(pointer).apply([&observed](item* candidate) { observed = candidate; });

            THEN("the callback receives the stored address") {
                REQUIRE(observed == std::addressof(value));
            }
        }
    }

    GIVEN("a transient pointer to a mutable object") {
        item value{7};
        item_ptr pointer{value};

        WHEN("apply_non_null is invoked") {
            const int result = static_cast<item_ptr&&>(pointer).apply_non_null([](item& candidate) {
                candidate.value = 13;
                return candidate.value;
            });

            THEN("the callback receives the object and its result is returned") {
                REQUIRE(result == 13);
                REQUIRE(value.value == 13);
            }
        }
    }

    GIVEN("a non-null transient pointer") {
        item value{7};
        item_ptr pointer{value};

        WHEN("apply_if_non_null is invoked") {
            bool invoked = false;
            const bool applied = static_cast<item_ptr&&>(pointer).apply_if_non_null([&invoked](item& candidate) {
                invoked = true;
                candidate.value = 17;
            });

            THEN("the callback runs and success is reported") {
                REQUIRE(applied);
                REQUIRE(invoked);
                REQUIRE(value.value == 17);
            }
        }
    }
}

SCENARIO("a transient pointer provides const access") {
    GIVEN("a transient pointer to a const object") {
        const item value{23};
        stdan::storage::transient_ptr<const item> pointer{value};

        WHEN("apply_non_null is invoked") {
            const int observed = static_cast<stdan::storage::transient_ptr<const item>&&>(pointer).apply_non_null([](const item& candidate) {
                return candidate.value;
            });

            THEN("the callback can read the object") {
                REQUIRE(observed == 23);
            }
        }
    }
}
