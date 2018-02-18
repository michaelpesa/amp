////////////////////////////////////////////////////////////////////////////////
//
// tests/bitops_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/bitops.hpp>
#include <amp/stddef.hpp>

#include <gtest/gtest.h>


using namespace ::amp;


TEST(bitops_test, shift)
{
    EXPECT_EQ(lsl(uint32{0x00000001}, 31), uint32{0x80000000});
    EXPECT_EQ(lsr(uint32{0x80000000}, 31), uint32{0x00000001});
    EXPECT_EQ(asr(uint32{0x80000000}, 31), uint32{0xffffffff});

    EXPECT_EQ(lsl(int32{1}, 31), INT32_MIN);
    EXPECT_EQ(lsr(INT32_MIN, 31), int32{1});
    EXPECT_EQ(asr(INT32_MIN, 31), int32{-1});
}

TEST(bitops_test, rotate)
{
    EXPECT_EQ(rol(uint32{0xff000000}, 0), uint32{0xff000000});
    EXPECT_EQ(ror(uint32{0x000000ff}, 0), uint32{0x000000ff});

    EXPECT_EQ(rol(uint16{0xff00}, 4), uint16{0xf00f});
    EXPECT_EQ(ror(uint16{0x00ff}, 4), uint16{0xf00f});

    EXPECT_EQ(rol(int8(0xf0), 2), int8(0xc3));
    EXPECT_EQ(ror(int8(0x0f), 2), int8(0xc3));
}

TEST(bitops_test, lzcnt)
{
    EXPECT_EQ(lzcnt(uint8{0x00}), 8);
    EXPECT_EQ(lzcnt(uint8{0x01}), 7);
    EXPECT_EQ(lzcnt(uint8{0x81}), 0);

    EXPECT_EQ(lzcnt(uint16{0x7fff}), 1);

    EXPECT_EQ(lzcnt(uint64{0x0fffffffffffffff}), 4);
    EXPECT_EQ(lzcnt(uint64{0x0f0f0f0f0f0f0f0f}), 4);
    EXPECT_EQ(lzcnt(uint64{0x0f00000000000000}), 4);
    EXPECT_EQ(lzcnt(uint64{0}), 64);
}

TEST(bitops_test, ceil_pow2)
{
    EXPECT_EQ(ceil_pow2(uint8{0xff}), 0x01);

    EXPECT_EQ(ceil_pow2(0), 0);
    EXPECT_EQ(ceil_pow2(1), 1);
    EXPECT_EQ(ceil_pow2(2), 2);
    EXPECT_EQ(ceil_pow2(3), 4);
    EXPECT_EQ(ceil_pow2(4), 4);

    EXPECT_EQ(ceil_pow2(0x7fffffff), 0x80000000);
    EXPECT_EQ(ceil_pow2(0x80000000), 0x80000000);
    EXPECT_EQ(ceil_pow2(0x80000001), 0x00000001);
    EXPECT_EQ(ceil_pow2(0xffffffff), 0x00000001);
}

TEST(bitops_test, floor_pow2)
{
    EXPECT_EQ(floor_pow2(uint8{0xff}), 0x80);

    EXPECT_EQ(floor_pow2(0), 0);
    EXPECT_EQ(floor_pow2(1), 1);
    EXPECT_EQ(floor_pow2(2), 2);
    EXPECT_EQ(floor_pow2(3), 2);
    EXPECT_EQ(floor_pow2(4), 4);

    EXPECT_EQ(floor_pow2(0x7fffffff), 0x40000000);
    EXPECT_EQ(floor_pow2(0x80000000), 0x80000000);
    EXPECT_EQ(floor_pow2(0x80000001), 0x80000000);
    EXPECT_EQ(floor_pow2(0xffffffff), 0x80000000);
}

TEST(bitops_test, ilog2)
{
    EXPECT_EQ(ilog2(1), 0);
    EXPECT_EQ(ilog2(2), 1);
    EXPECT_EQ(ilog2(3), 1);
    EXPECT_EQ(ilog2(4), 2);
    EXPECT_EQ(ilog2(5), 2);
    EXPECT_EQ(ilog2(6), 2);
    EXPECT_EQ(ilog2(7), 2);
    EXPECT_EQ(ilog2(8), 3);
}

