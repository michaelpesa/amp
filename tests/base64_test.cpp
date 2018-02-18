////////////////////////////////////////////////////////////////////////////////
//
// tests/base64_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/base64.hpp>
#include <amp/string_view.hpp>

#include <stdexcept>

#include <gtest/gtest.h>


using namespace ::amp;


TEST(base64_test, encode_and_decode)
{
    static constexpr struct {
        string_view decoded;
        string_view encoded;
    }
    const table[] {
        { "",        ""             },
        { "1",       "MQ=="         },
        { "22",      "MjI="         },
        { "333",     "MzMz"         },
        { "4444",    "NDQ0NA=="     },
        { "55555",   "NTU1NTU="     },
        { "666666",  "NjY2NjY2"     },
        { "abc:def", "YWJjOmRlZg==" },
    };

    char buf[128];
    std::size_t len;

    for (auto&& entry : table) {
        len = base64::encode(entry.decoded.data(), entry.decoded.size(), buf);
        ASSERT_EQ(string_view(buf, len), entry.encoded);

        len = base64::decode(entry.encoded.data(), entry.encoded.size(), buf);
        ASSERT_EQ(string_view(buf, len), entry.decoded);
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

        ASSERT_EQ(buf, "1"_sv);
    }

    {
        base64::stream s;
        auto dst = &buf[0];

        dst += s.decode("Mj", 2, dst);
        dst += s.decode("I=", 2, dst);
        *dst++ = '\0';

        ASSERT_EQ(buf, "22"_sv);
    }

    {
        base64::stream s;
        auto dst = &buf[0];

        dst += s.decode("NDQ", 3, dst);
        dst += s.decode("0N",  2, dst);
        dst += s.decode("A==", 3, dst);
        *dst++ = '\0';

        ASSERT_EQ(buf, "4444"_sv);
    }

    {
        base64::stream s;
        auto dst = &buf[0];

        dst += s.decode("NjY", 3, dst);
        dst += s.decode("2Nj", 3, dst);
        dst += s.decode("Y",   1, dst);
        dst += s.decode("2",   1, dst);
        *dst++ = '\0';

        ASSERT_EQ(buf, "666666"_sv);
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

        ASSERT_EQ(buf, "MQ=="_sv);
    }

    {
        base64::stream s;
        auto dst = &buf[0];

        dst += s.encode("2", 1, dst);
        dst += s.encode("2", 1, dst);
        dst += s.encode_finish(dst);
        *dst++ = '\0';

        ASSERT_EQ(buf, "MjI="_sv);
    }

    {
        base64::stream s;
        auto dst = &buf[0];

        dst += s.encode("44", 2, dst);
        dst += s.encode("4",  1, dst);
        dst += s.encode("4",  1, dst);
        dst += s.encode_finish(dst);
        *dst++ = '\0';

        ASSERT_EQ(buf, "NDQ0NA=="_sv);
    }

    {
        base64::stream s;
        auto dst = &buf[0];

        dst += s.encode("666", 3, dst);
        dst += s.encode("6",   1, dst);
        dst += s.encode("66",  2, dst);
        dst += s.encode_finish(dst);
        *dst++ = '\0';

        ASSERT_EQ(buf, "NjY2NjY2"_sv);
    }
}

