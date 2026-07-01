#include <catch2/catch_test_macros.hpp>

#include "storage/ps_vector.hpp"

#ifndef STDAN_DEBUG
#error "STDAN_DEBUG must be defined by the build."
#endif

#ifndef EXPECTED_STDAN_DEBUG
#error "EXPECTED_STDAN_DEBUG must be defined by the test target."
#endif

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

SCENARIO("resize grows default-constructible values within capacity")
{
    GIVEN("a vector with spare capacity")
    {
        stdan::ps_vector<int> values(4);

        WHEN("it is resized up within capacity")
        {
            values.resize(2);

            THEN("the size grows to the requested value")
            {
                REQUIRE(values.size() == 2);
                REQUIRE(values[0] == 0);
                REQUIRE(values[1] == 0);
            }
        }

        WHEN("it is resized beyond capacity")
        {
            values.resize(5);

            THEN("the vector grows to the requested size")
            {
                REQUIRE(values.size() == 5);
                REQUIRE(values[0] == 0);
                REQUIRE(values[1] == 0);
                REQUIRE(values[2] == 0);
                REQUIRE(values[3] == 0);
                REQUIRE(values[4] == 0);
            }
        }
    }
}

SCENARIO("append stores values until the vector is full")
{
    GIVEN("a vector with spare capacity")
    {
        stdan::ps_vector<int> values(2);

        WHEN("values are appended")
        {
            values.append(7);
            values.append(11);

            THEN("the values are stored in order")
            {
                REQUIRE(values.size() == 2);
                REQUIRE(values[0] == 7);
                REQUIRE(values[1] == 11);
            }
        }

        WHEN("an append is attempted after capacity is reached")
        {
            values.append(7);
            values.append(11);
            values.append(13);

            THEN("the vector remains full and unchanged")
            {
                REQUIRE(values.size() == 2);
                REQUIRE(values[0] == 7);
                REQUIRE(values[1] == 11);
            }
        }
    }
}

SCENARIO("remove swaps the last live element into the removed slot")
{
    GIVEN("a vector with multiple values")
    {
        stdan::ps_vector<int> values(4);
        values.append(7);
        values.append(11);
        values.append(13);

        WHEN("the middle element is removed")
        {
            values.remove(1);

            THEN("the last live value is moved into the vacated slot")
            {
                REQUIRE(values.size() == 2);
                REQUIRE(values[0] == 7);
                REQUIRE(values[1] == 13);
            }
        }
    }
}

SCENARIO("index_of finds live elements and reports missing ones")
{
    GIVEN("a vector with known values")
    {
        stdan::ps_vector<int> values(3);
        values.append(5);
        values.append(9);

        WHEN("a present value is queried")
        {
            auto index = values.index_of(9);

            THEN("the matching index is returned")
            {
                REQUIRE(index.has_value());
                REQUIRE(index.value() == 1);
            }
        }

        WHEN("a missing value is queried")
        {
            auto index = values.index_of(42);

            THEN("the lookup reports failure")
            {
                REQUIRE_FALSE(index.has_value());
            }
        }
    }
}
