////////////////////////////////////////////////////////////////////////////////
//
// tests/muldiv_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/muldiv.hpp>
#include <amp/stddef.hpp>

#include <gtest/gtest.h>


using namespace ::amp;


TEST(muldiv_test, identity)
{
    EXPECT_EQ(muldiv(1, 100, 100), 1);
    EXPECT_EQ(muldiv(-1, 100, 100), -1);

    EXPECT_EQ(muldiv(1ULL, UINT32_MAX, UINT32_MAX), 1ULL);
    EXPECT_EQ(muldiv(1ULL, UINT64_MAX, UINT64_MAX), 1ULL);

    EXPECT_EQ(muldiv(-1LL, INT64_MIN, INT64_MAX), 1LL);
    EXPECT_EQ(muldiv(-1LL, INT64_MAX, INT64_MIN), 1LL);

    EXPECT_EQ(muldiv(1LL, INT64_MIN, INT64_MAX), -1LL);
    EXPECT_EQ(muldiv(1LL, INT64_MAX, INT64_MIN), -1LL);

    EXPECT_EQ(muldiv(INT64_MIN, INT64_MAX, INT64_MAX), INT64_MIN);
    EXPECT_EQ(muldiv(INT64_MAX, INT64_MIN, INT64_MIN), INT64_MAX);
}

TEST(muldiv_test, multiply_by_zero)
{
    EXPECT_EQ(muldiv(41, 0, 1), 0);
    EXPECT_EQ(muldiv(41LL, 0, INT64_MIN), 0);
}

TEST(muldiv_test, divide_by_zero)
{
    EXPECT_EQ(muldiv(+1LL, 1, 0), INT64_MAX);
    EXPECT_EQ(muldiv(-1LL, 1, 0), INT64_MIN);
    EXPECT_EQ(muldiv(1ULL, 1, 0), UINT64_MAX);
    EXPECT_EQ(muldiv(0ULL, 0, 0), UINT64_MAX);
}

TEST(muldiv_test, sign_extension)
{
    EXPECT_EQ(muldiv(+1, +7, +2), +4);
    EXPECT_EQ(muldiv(+1, -7, +2), -4);
    EXPECT_EQ(muldiv(+1, +7, -2), -4);
    EXPECT_EQ(muldiv(+1, -7, -2), +4);

    EXPECT_EQ(muldiv(-1, +7, +2), -4);
    EXPECT_EQ(muldiv(-1, -7, +2), +4);
    EXPECT_EQ(muldiv(-1, +7, -2), +4);
    EXPECT_EQ(muldiv(-1, -7, -2), -4);
}

TEST(muldiv_test, signed_intermediate_overflow)
{
    EXPECT_EQ(muldiv(int32{0x7fffffff}, 29, 30), 0x7bbbbbbb);
    EXPECT_EQ(muldiv(INT32_MIN, +2, +8), INT32_MIN / +4);
    EXPECT_EQ(muldiv(INT32_MIN, +2, -8), INT32_MIN / -4);
    EXPECT_EQ(muldiv(INT32_MIN, -2, +8), INT32_MIN / -4);
    EXPECT_EQ(muldiv(INT32_MIN, -2, -8), INT32_MIN / +4);
    EXPECT_EQ(muldiv(+44100, INT32_MAX, INT32_MIN), -44100);
    EXPECT_EQ(muldiv(+44100, INT32_MIN, INT32_MAX), -44100);
    EXPECT_EQ(muldiv(-44100, INT32_MAX, INT32_MIN), +44100);
    EXPECT_EQ(muldiv(-44100, INT32_MIN, INT32_MAX), +44100);


    EXPECT_EQ(muldiv(int64{0x7fffffffffffffff}, 29, 30), 0x7bbbbbbbbbbbbbbb);
    EXPECT_EQ(muldiv(INT64_MIN, +2, +8), INT64_MIN / +4);
    EXPECT_EQ(muldiv(INT64_MIN, +2, -8), INT64_MIN / -4);
    EXPECT_EQ(muldiv(INT64_MIN, -2, +8), INT64_MIN / -4);
    EXPECT_EQ(muldiv(INT64_MIN, -2, -8), INT64_MIN / +4);
    EXPECT_EQ(muldiv(+44100LL, INT64_MAX, INT64_MIN), -44100);
    EXPECT_EQ(muldiv(+44100LL, INT64_MIN, INT64_MAX), -44100);
    EXPECT_EQ(muldiv(-44100LL, INT64_MAX, INT64_MIN), +44100);
    EXPECT_EQ(muldiv(-44100LL, INT64_MIN, INT64_MAX), +44100);
}

TEST(muldiv_test, saturate_on_overflow)
{
    EXPECT_EQ(muldiv(INT32_MAX, INT32_MAX, 1), INT32_MAX);
    EXPECT_EQ(muldiv(INT32_MAX, INT32_MIN, 1), INT32_MIN);
    EXPECT_EQ(muldiv(INT32_MIN, INT32_MAX, 1), INT32_MIN);
    EXPECT_EQ(muldiv(INT32_MIN, INT32_MIN, 1), INT32_MAX);
    EXPECT_EQ(muldiv(INT32_MIN, INT32_MIN, -1), INT32_MIN);
    EXPECT_EQ(muldiv(UINT32_MAX, UINT32_MAX, 1), UINT32_MAX);

    EXPECT_EQ(muldiv(INT64_MAX, INT64_MAX, 1), INT64_MAX);
    EXPECT_EQ(muldiv(INT64_MAX, INT64_MIN, 1), INT64_MIN);
    EXPECT_EQ(muldiv(INT64_MIN, INT64_MAX, 1), INT64_MIN);
    EXPECT_EQ(muldiv(INT64_MIN, INT64_MIN, 1), INT64_MAX);
    EXPECT_EQ(muldiv(INT64_MIN, INT64_MIN, -1), INT64_MIN);
    EXPECT_EQ(muldiv(UINT64_MAX, UINT64_MAX, 1), UINT64_MAX);
}

