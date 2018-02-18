////////////////////////////////////////////////////////////////////////////////
//
// tests/numeric_cast_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/numeric.hpp>
#include <amp/stddef.hpp>

#include <gtest/gtest.h>


using namespace ::amp;


namespace {

static_assert(noexcept(numeric_cast<uint32>(uint8{})), "");
static_assert(noexcept(numeric_cast<uint32>(uint16{})), "");
static_assert(noexcept(numeric_cast<uint32>(uint32{})), "");

static_assert(noexcept(numeric_cast<int32>(int8{})), "");
static_assert(noexcept(numeric_cast<int32>(int16{})), "");
static_assert(noexcept(numeric_cast<int32>(int32{})), "");
static_assert(noexcept(numeric_cast<int32>(uint8{})), "");
static_assert(noexcept(numeric_cast<int32>(uint16{})), "");

static_assert(!noexcept(numeric_cast<uint32>(int8{})), "");
static_assert(!noexcept(numeric_cast<uint32>(int16{})), "");
static_assert(!noexcept(numeric_cast<uint32>(int32{})), "");
static_assert(!noexcept(numeric_cast<uint32>(int64{})), "");
static_assert(!noexcept(numeric_cast<uint32>(uint64{})), "");

static_assert(!noexcept(numeric_cast<int32>(uint32{})), "");
static_assert(!noexcept(numeric_cast<int32>(uint64{})), "");
static_assert(!noexcept(numeric_cast<int32>(int64{})), "");

static_assert(noexcept(numeric_cast<double>(double{})), "");

static_assert(!noexcept(numeric_cast<int16>(double{})), "");
static_assert(!noexcept(numeric_cast<int32>(double{})), "");
static_assert(!noexcept(numeric_cast<int64>(double{})), "");

static_assert(!noexcept(numeric_cast<double>(int16{})), "");
static_assert(!noexcept(numeric_cast<double>(int32{})), "");
static_assert(!noexcept(numeric_cast<double>(int64{})), "");

}     // namespace <unnamed>


TEST(numeric_cast_test, int_to_float)
{
    ASSERT_EQ(numeric_cast<float>(0x00ffffff), 0x00ffffff);
    ASSERT_EQ(numeric_cast<float>(0x01000000), 0x01000000);
    ASSERT_FALSE(numeric_try_cast<float>(0x1000001));
}

TEST(numeric_cast_test, float_to_int)
{
    ASSERT_EQ(numeric_cast<int16>(float{INT16_MAX}), INT16_MAX);
    ASSERT_FALSE(numeric_try_cast<int16>(float{INT16_MAX} + 1.f));

    ASSERT_EQ(numeric_cast<int16>(float{INT16_MIN}), INT16_MIN);
    ASSERT_FALSE(numeric_try_cast<int16>(float{INT16_MIN} - 1.f));
}

TEST(numeric_cast_test, signed_overflow)
{
    ASSERT_EQ(numeric_cast<int16>(int32{INT16_MAX}), INT16_MAX);
    ASSERT_FALSE(numeric_try_cast<int16>(int32{INT16_MAX} + 1));

    ASSERT_EQ(numeric_cast<int16>(int32{INT16_MIN}), INT16_MIN);
    ASSERT_FALSE(numeric_try_cast<int16>(int32{INT16_MIN} - 1));

    ASSERT_EQ(numeric_cast<int32>(uint32{INT32_MAX}), INT32_MAX);
    ASSERT_FALSE(numeric_try_cast<int32>(uint32{INT32_MAX} + 1));
}

TEST(numeric_cast_test, unsigned_overflow)
{
    ASSERT_EQ(numeric_cast<uint8>(int32{UINT8_MAX}), UINT8_MAX);
    ASSERT_FALSE(numeric_try_cast<uint8>(int32{UINT8_MAX} + 1));
    ASSERT_FALSE(numeric_try_cast<uint8>(int32{INT32_MIN}));
    ASSERT_FALSE(numeric_try_cast<uint8>(int8{-1}));
}

