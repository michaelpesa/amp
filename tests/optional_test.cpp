////////////////////////////////////////////////////////////////////////////////
//
// tests/optional_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/optional.hpp>
#include <amp/string_view.hpp>

#include <limits>
#include <string>
#include <tuple>
#include <utility>

#include <gtest/gtest.h>


using namespace ::amp;


static_assert(optional<int>(nullopt) < std::numeric_limits<int>::min(), "");
static_assert(optional<int>(0) > -1, "");
static_assert(optional<int>(1) == 1, "");
static_assert(optional<int>(nullopt) != 0, "");


TEST(optional_test, construct)
{
    optional<std::pair<string_view, int>> x {
        in_place,
        std::piecewise_construct,
        std::forward_as_tuple("abc"),
        std::forward_as_tuple(123),
    };

    ASSERT_TRUE(x.has_value());
    ASSERT_EQ(*x, std::make_pair("abc"_sv, 123));
}

