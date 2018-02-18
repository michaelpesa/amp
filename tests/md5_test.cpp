////////////////////////////////////////////////////////////////////////////////
//
// tests/md5_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/md5.hpp>
#include <amp/memory.hpp>
#include <amp/string_view.hpp>

#include <gtest/gtest.h>


using namespace ::amp;


TEST(md5_test, sum)
{
    {
        uint8 const expected_digest[] {
            0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04,
            0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e,
        };

        uint8 digest[16];
        md5::sum(nullptr, 0, digest);
        EXPECT_TRUE(mem::equal(digest, expected_digest));
    }

    {
        auto const msg = "The quick brown fox jumps over the lazy dog"_sv;

        uint8 const expected_digest[] {
            0x9e, 0x10, 0x7d, 0x9d, 0x37, 0x2b, 0xb6, 0x82,
            0x6b, 0xd8, 0x1d, 0x35, 0x42, 0xa4, 0x19, 0xd6,
        };

        uint8 digest[16];
        md5::sum(msg.data(), msg.size(), digest);
        EXPECT_TRUE(mem::equal(digest, expected_digest));
    }

    {
        auto const msg = "The quick brown fox jumps over the lazy dog."_sv;

        uint8 const expected_digest[] {
            0xe4, 0xd9, 0x09, 0xc2, 0x90, 0xd0, 0xfb, 0x1c,
            0xa0, 0x68, 0xff, 0xad, 0xdf, 0x22, 0xcb, 0xd0,
        };

        uint8 digest[16];
        md5::sum(msg.data(), msg.size(), digest);
        EXPECT_TRUE(mem::equal(digest, expected_digest));
    }
}

TEST(md5_test, update_and_finish)
{
    uint8 const expected_digest[] {
        0xe4, 0xd9, 0x09, 0xc2, 0x90, 0xd0, 0xfb, 0x1c,
        0xa0, 0x68, 0xff, 0xad, 0xdf, 0x22, 0xcb, 0xd0,
    };

    uint8 digest[16];

    md5 md5;
    md5.update("The quick brown fox ju", 22);
    md5.update("mps over the lazy dog.", 22);
    md5.finish(digest);
    EXPECT_TRUE(mem::equal(digest, expected_digest));
}

