////////////////////////////////////////////////////////////////////////////////
//
// tests/io_buffer_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/io/buffer.hpp>

#include <algorithm>
#include <array>
#include <numeric>

#include <gtest/gtest.h>


using namespace ::amp;


TEST(io_buffer, resize)
{
    io::buffer buf;
    ASSERT_EQ(buf.size(), 0);
    ASSERT_EQ(buf.capacity(), 0);
    ASSERT_TRUE(buf.empty());

    buf.resize(128);
    ASSERT_EQ(buf.size(), 128);
    ASSERT_EQ(buf.capacity(), 128);
    ASSERT_FALSE(buf.empty());

    std::array<uint8, 128> tmp{};
    ASSERT_TRUE(std::equal(buf.begin(), buf.end(), tmp.begin()));

    std::iota(buf.begin(), buf.end(), uint8{0});
    std::iota(tmp.begin(), tmp.end(), uint8{0});
    ASSERT_TRUE(std::equal(buf.begin(), buf.end(), tmp.begin()));

    buf.resize(64, uninitialized);
    ASSERT_EQ(buf.size(), 64);
    ASSERT_EQ(buf.capacity(), 128);
    ASSERT_TRUE(std::equal(buf.begin(), buf.end(), tmp.begin()));

    buf.resize(128, uninitialized);
    ASSERT_EQ(buf.size(), 128);
    ASSERT_EQ(buf.capacity(), 128);
    ASSERT_TRUE(std::equal(buf.begin(), buf.end(), tmp.begin()));

    buf.resize(64);
    ASSERT_EQ(buf.size(), 64);
    ASSERT_EQ(buf.capacity(), 128);
    ASSERT_TRUE(std::equal(buf.begin(), buf.end(), tmp.begin()));

    buf.resize(128);
    ASSERT_EQ(buf.size(), 128);
    ASSERT_EQ(buf.capacity(), 128);
    ASSERT_FALSE(std::equal(buf.begin(), buf.end(), tmp.begin()));
}

TEST(io_buffer, insert)
{
    io::buffer buf;
    buf.assign("2678", 4);
    ASSERT_EQ(buf.size(), 4);

    auto ret = buf.insert(buf.cbegin(), "0", 1);
    ASSERT_EQ(*ret, '0');
    ASSERT_EQ(buf.size(), 5);

    ret = buf.insert(buf.cbegin() + 1, "1", 1);
    ASSERT_EQ(*ret, '1');
    ASSERT_EQ(buf.size(), 6);

    ret = buf.insert(buf.cbegin() + 3, "345", 3);
    ASSERT_EQ(*ret, '3');
    ASSERT_EQ(buf.size(), 9);

    ret = buf.insert(buf.end(), "9", 1);
    ASSERT_EQ(*ret, '9');
    ASSERT_EQ(buf.size(), 10);

    ASSERT_EQ(buf, io::buffer("0123456789", 10));
}

