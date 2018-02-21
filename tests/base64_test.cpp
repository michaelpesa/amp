////////////////////////////////////////////////////////////////////////////////
//
// tests/base64_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/base64.hpp>

#include <stdexcept>
#include <string_view>

#include <gtest/gtest.h>


using namespace ::amp;
using namespace ::std::literals;


TEST(base64_test, encode_and_decode)
{
    static constexpr struct {
        std::string_view decoded;
        std::string_view encoded;
    }
    const table[] {
        { ""sv,        ""sv             },
        { "1"sv,       "MQ=="sv         },
        { "22"sv,      "MjI="sv         },
        { "333"sv,     "MzMz"sv         },
        { "4444"sv,    "NDQ0NA=="sv     },
        { "55555"sv,   "NTU1NTU="sv     },
        { "666666"sv,  "NjY2NjY2"sv     },
        { "abc:def"sv, "YWJjOmRlZg=="sv },
    };

    char buf[128];
    std::size_t len;

    for (auto&& entry : table) {
        len = base64::encode(entry.decoded.data(), entry.decoded.size(), buf);
        ASSERT_EQ(std::string_view(buf, len), entry.encoded);

        len = base64::decode(entry.encoded.data(), entry.encoded.size(), buf);
        ASSERT_EQ(std::string_view(buf, len), entry.decoded);
    }
}

TEST(base64_test, stream_decode)
{
    char buf[128];

    {
        base64::stream s;
        auto dst = &buf[0];

        dst += s.decode("MQ", 2, dst);
        dst += s.decode("==", 2, dst);
        *dst++ = '\0';

        ASSERT_EQ(buf, "1"sv);
    }

    {
        base64::stream s;
        auto dst = &buf[0];

        dst += s.decode("Mj", 2, dst);
        dst += s.decode("I=", 2, dst);
        *dst++ = '\0';

        ASSERT_EQ(buf, "22"sv);
    }

    {
        base64::stream s;
        auto dst = &buf[0];

        dst += s.decode("NDQ", 3, dst);
        dst += s.decode("0N",  2, dst);
        dst += s.decode("A==", 3, dst);
        *dst++ = '\0';

        ASSERT_EQ(buf, "4444"sv);
    }

    {
        base64::stream s;
        auto dst = &buf[0];

        dst += s.decode("NjY", 3, dst);
        dst += s.decode("2Nj", 3, dst);
        dst += s.decode("Y",   1, dst);
        dst += s.decode("2",   1, dst);
        *dst++ = '\0';

        ASSERT_EQ(buf, "666666"sv);
    }
}

TEST(base64_test, stream_encode)
{
    char buf[128];

    {
        base64::stream s;
        auto dst = &buf[0];

        dst += s.encode("1", 1, dst);
        dst += s.encode_finish(dst);
        *dst++ = '\0';

        ASSERT_EQ(buf, "MQ=="sv);
    }

    {
        base64::stream s;
        auto dst = &buf[0];

        dst += s.encode("2", 1, dst);
        dst += s.encode("2", 1, dst);
        dst += s.encode_finish(dst);
        *dst++ = '\0';

        ASSERT_EQ(buf, "MjI="sv);
    }

    {
        base64::stream s;
        auto dst = &buf[0];

        dst += s.encode("44", 2, dst);
        dst += s.encode("4",  1, dst);
        dst += s.encode("4",  1, dst);
        dst += s.encode_finish(dst);
        *dst++ = '\0';

        ASSERT_EQ(buf, "NDQ0NA=="sv);
    }

    {
        base64::stream s;
        auto dst = &buf[0];

        dst += s.encode("666", 3, dst);
        dst += s.encode("6",   1, dst);
        dst += s.encode("66",  2, dst);
        dst += s.encode_finish(dst);
        *dst++ = '\0';

        ASSERT_EQ(buf, "NjY2NjY2"sv);
    }
}

