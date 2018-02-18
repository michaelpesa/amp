////////////////////////////////////////////////////////////////////////////////
//
// tests/crc_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/crc.hpp>

#include <algorithm>
#include <array>
#include <numeric>

#include <gtest/gtest.h>


using namespace ::amp;


TEST(crc32_test, compute)
{
    {
        char const s[] = "1234567890A";
        EXPECT_EQ(crc32::compute(s, sizeof(s) - 3), 0xcbf43926);
        EXPECT_EQ(crc32::compute(s, sizeof(s) - 2), 0x261daee5);
        EXPECT_EQ(crc32::compute(s, sizeof(s) - 1), 0x039f95d2);
    }

    {
        char const s[] = "The quick brown fox jumps over the lazy dog";
        EXPECT_EQ(crc32::compute(s, sizeof(s) - 1), 0x414fa339);
    }
}

TEST(crc32_test, update)
{
    auto rem = ~uint32{0};

    rem = crc32::update("123", 3, rem);
    rem = crc32::update("456", 3, rem);
    rem = crc32::update("789", 3, rem);
    EXPECT_EQ(~rem, 0xcbf43926);

    rem = crc32::update("0", 1, rem);
    EXPECT_EQ(~rem, 0x261daee5);

    rem = crc32::update("A", 1, rem);
    EXPECT_EQ(~rem, 0x039f95d2);

    rem = ~uint32{0};
    rem = crc32::update("The quick brown fox ju", 22, rem);
    rem = crc32::update("mps over the lazy dog",  21, rem);
    EXPECT_EQ(~rem, 0x414fa339);
}


TEST(crc32c_test, compute)
{
    {
        char const s[] = "1234567890A";
        EXPECT_EQ(crc32c::compute(s, sizeof(s) - 3), 0xe3069283);
        EXPECT_EQ(crc32c::compute(s, sizeof(s) - 2), 0xf3dbd4fe);
        EXPECT_EQ(crc32c::compute(s, sizeof(s) - 1), 0xbe88c668);
    }

    {
        char const s[] = "The quick brown fox jumps over the lazy dog";
        EXPECT_EQ(crc32c::compute(s, sizeof(s) - 1), 0x22620404);
    }
}

TEST(crc32c_test, update)
{
    auto rem = ~uint32{0};

    rem = crc32c::update("123", 3, rem);
    rem = crc32c::update("456", 3, rem);
    rem = crc32c::update("789", 3, rem);
    EXPECT_EQ(~rem, 0xe3069283);

    rem = crc32c::update("0", 1, rem);
    EXPECT_EQ(~rem, 0xf3dbd4fe);

    rem = crc32c::update("A", 1, rem);
    EXPECT_EQ(~rem, 0xbe88c668);

    rem = ~uint32{0};
    rem = crc32c::update("The quick brown fox ju", 22, rem);
    rem = crc32c::update("mps over the lazy dog",  21, rem);
    EXPECT_EQ(~rem, 0x22620404);
}

TEST(crc32_test, rfc3720_examples)
{
    std::array<uint8, 32> buf{};
    EXPECT_EQ(crc32c::compute(buf.data(), buf.size()), 0x8a9136aa);

    std::fill(buf.begin(), buf.end(), uint8{0xff});
    EXPECT_EQ(crc32c::compute(buf.data(), buf.size()), 0x62a8ab43);

    std::iota(buf.begin(), buf.end(), uint8{0x00});
    EXPECT_EQ(crc32c::compute(buf.data(), buf.size()), 0x46dd794e);

    std::iota(buf.rbegin(), buf.rend(), uint8{0x00});
    EXPECT_EQ(crc32c::compute(buf.data(), buf.size()), 0x113fdb5c);
}

