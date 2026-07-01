#include <catch2/catch_test_macros.hpp>

#include "PopSwapVector.hpp"

#ifndef STDAN_DEBUG
#error "STDAN_DEBUG must be defined by the build."
#endif

#ifndef EXPECTED_STDAN_DEBUG
#error "EXPECTED_STDAN_DEBUG must be defined by the test target."
#endif

struct NonDefaultConstructible
{
    int value;

    NonDefaultConstructible() = delete;

    explicit NonDefaultConstructible(int aValue)
        : value(aValue)
    {
    }
};

SCENARIO("STDAN_DEBUG matches the configured build type")
{
    GIVEN("a test target compiled by CMake")
    {
        THEN("STDAN_DEBUG matches the expected value for the configuration")
        {
            static_assert(STDAN_DEBUG == EXPECTED_STDAN_DEBUG);
            REQUIRE(STDAN_DEBUG == EXPECTED_STDAN_DEBUG);
        }
    }
}

SCENARIO("Resize grows default-constructible values within capacity")
{
    GIVEN("a vector with spare capacity")
    {
        PopSwapVector<int> values(4);

        WHEN("it is resized up within capacity")
        {
            values.Resize(2);

            THEN("the size grows to the requested value")
            {
                REQUIRE(values.Size() == 2);
                REQUIRE(values[0] == 0);
                REQUIRE(values[1] == 0);
            }
        }

        WHEN("it is resized beyond capacity")
        {
            values.Resize(5);

            THEN("the vector remains unchanged")
            {
                REQUIRE(values.Size() == 0);
            }
        }
    }
}

SCENARIO("Resize shrinks non-default-constructible values without requiring default construction")
{
    GIVEN("a vector with existing non-default-constructible elements")
    {
        PopSwapVector<NonDefaultConstructible> values(4);
        REQUIRE(values.Emplace(7));
        REQUIRE(values.Emplace(11));

        WHEN("it is resized smaller")
        {
            values.Resize(1);

            THEN("the trailing element is removed and the first element is preserved")
            {
                REQUIRE(values.Size() == 1);
                REQUIRE(values[0].value == 7);
            }
        }

        WHEN("it is resized to the current size")
        {
            values.Resize(2);

            THEN("the stored values remain unchanged")
            {
                REQUIRE(values.Size() == 2);
                REQUIRE(values[0].value == 7);
                REQUIRE(values[1].value == 11);
            }
        }
    }
}

SCENARIO("Resize growth on non-default-constructible values is non-mutating outside Debug")
{
#if defined(STDAN_DEBUG) && STDAN_DEBUG
    GIVEN("a Debug build")
    {
        THEN("the abort behavior is covered by the dedicated helper executable")
        {
            SUCCEED("PopSwapVectorResizeGrowthAbort validates the Debug assertion path.");
        }
    }
#else
    GIVEN("a vector with one non-default-constructible element")
    {
        PopSwapVector<NonDefaultConstructible> values(4);
        REQUIRE(values.Emplace(5));

        WHEN("it is resized larger")
        {
            values.Resize(2);

            THEN("the size and stored value remain unchanged")
            {
                REQUIRE(values.Size() == 1);
                REQUIRE(values[0].value == 5);
            }
        }
    }
#endif
}
