////////////////////////////////////////////////////////////////////////////////
//
// tests/u8string_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/crc.hpp>
#include <amp/u8string.hpp>

#include <algorithm>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>


using namespace ::amp;
using namespace ::std::literals;


inline u8string_rep* rep_ptr(u8string_buffer const& s) noexcept
{ return *reinterpret_cast<u8string_rep* const*>(&s); }

inline u8string_rep const* rep_ptr(u8string const& s) noexcept
{ return *reinterpret_cast<u8string_rep const* const*>(&s); }


TEST(u8string_test, detach_unique)
{
    auto s1 = u8string{"string"};
    ASSERT_EQ(s1.use_count(), 1);

    auto s2 = s1.detach();
    ASSERT_NE(rep_ptr(s1), rep_ptr(s2));

    auto const old_ptr = rep_ptr(s1);
    auto s3 = std::move(s1).detach();
    ASSERT_EQ(rep_ptr(s1), nullptr);
    ASSERT_EQ(rep_ptr(s3), old_ptr);
}

TEST(u8string_test, detach_shared)
{
    auto s1 = u8string{"string"};
    ASSERT_EQ(s1.use_count(), 1);

    auto s2 = s1;
    ASSERT_EQ(s1.use_count(), 2);
    ASSERT_EQ(rep_ptr(s1), rep_ptr(s2));

    auto s3 = std::move(s1).detach();
    ASSERT_EQ(s1.use_count(), 2);
    ASSERT_EQ(rep_ptr(s1), rep_ptr(s2));
    ASSERT_NE(rep_ptr(s1), rep_ptr(s3));
    ASSERT_TRUE(std::equal(s1.begin(), s1.end(), s3.begin(), s3.end()));
}

TEST(u8string_test, detach_interned)
{
    auto s1 = u8string{"testing"};
    ASSERT_EQ(s1.use_count(), 1);

    s1.intern();
    ASSERT_EQ(s1.use_count(), 1);

    auto s2 = std::move(s1).detach();
    ASSERT_EQ(s1.use_count(), 1);
    ASSERT_NE(rep_ptr(s1), rep_ptr(s2));
    ASSERT_TRUE(std::equal(s1.begin(), s1.end(), s2.begin(), s2.end()));
}

TEST(u8string_test, interned_reference_counting)
{
    {
        auto s1 = u8string{};
        s1.intern();
        ASSERT_EQ(s1.use_count(), 0);
    }

    {
        auto s1 = u8string{"testing"};
        s1.intern();
        ASSERT_EQ(s1.use_count(), 1);
    }

    auto s1 = u8string{"testing"};
    ASSERT_EQ(s1.use_count(), 1);

    s1.intern();
    ASSERT_EQ(s1.use_count(), 1);

    auto s2 = s1;
    ASSERT_EQ(s1, s2);
    ASSERT_EQ(rep_ptr(s1), rep_ptr(s2));
    ASSERT_EQ(s1.use_count(), 2);

    s1 = "another";
    s1.intern();
    ASSERT_NE(s1, s2);
    ASSERT_NE(rep_ptr(s1), rep_ptr(s2));
    ASSERT_EQ(s1.use_count(), 1);
    ASSERT_EQ(s2.use_count(), 1);

    s2 = "another";
    ASSERT_EQ(s1, s2);
    ASSERT_NE(rep_ptr(s1), rep_ptr(s2));
    ASSERT_EQ(s1.use_count(), 1);
    ASSERT_EQ(s2.use_count(), 1);

    s2.intern();
    ASSERT_EQ(s1, s2);
    ASSERT_EQ(rep_ptr(s1), rep_ptr(s2));
    ASSERT_EQ(s1.use_count(), 2);

    {
        auto s3 = u8string::intern("another");
        ASSERT_EQ(s1, s2);
        ASSERT_EQ(s1, s3);
        ASSERT_EQ(rep_ptr(s1), rep_ptr(s2));
        ASSERT_EQ(rep_ptr(s1), rep_ptr(s3));
        ASSERT_EQ(s1.use_count(), 3);
    }

    ASSERT_EQ(s1, s2);
    ASSERT_EQ(rep_ptr(s1), rep_ptr(s2));
    ASSERT_EQ(s1.use_count(), 2);
}

TEST(u8string_test, construct_with_size_uninitialized)
{
    auto const s = u8string_buffer(12, uninitialized);
    ASSERT_EQ(s.size(), 12);
    ASSERT_EQ(s[s.size()], '\0');
}

TEST(u8string_test, construct_with_size)
{
    auto const s = u8string_buffer(3);
    ASSERT_EQ(s.size(), 3);
    ASSERT_EQ(s[s.size()], '\0');
    ASSERT_EQ(s.compare("\0\0\0", 3), 0);
}

TEST(u8string_test, construct_with_size_and_fill_character)
{
    auto const s = u8string_buffer(4, '@');
    ASSERT_EQ(s.size(), 4);
    ASSERT_EQ(s[s.size()], '\0');
    ASSERT_EQ(s.compare("@@@@", 4), 0);
}

TEST(u8string_test, compare)
{
    ASSERT_EQ(u8string{"abcde"}.compare("abcdefg", 5), 0);
    ASSERT_LT(u8string{"abcde"}.compare("abcdefg", 6), 0);
    ASSERT_GT(u8string{"abcde"}.compare("abcdefg", 4), 0);

    ASSERT_GT(u8string{"ABCDE"}.compare("12345"sv), 0);
    ASSERT_LT(u8string{"ABCDE"}.compare("abcde"sv), 0);
}

TEST(u8string_test, resize)
{
    {
        auto s = u8string_buffer{"testing..."};
        s.resize(sizeof("testing") - 1);
        ASSERT_EQ(s.size(), (sizeof("testing") - 1));
        ASSERT_EQ(std::strcmp(s.c_str(), "testing"), 0);

        s.resize(s.size() + 128);
        ASSERT_EQ(s.size(), (sizeof("testing") - 1) + 128);
        ASSERT_EQ(std::strcmp(s.c_str(), "testing"), 0);
    }

    {
        auto s = u8string_buffer{};
        ASSERT_TRUE(s.empty());

        s.resize(sizeof("empty") - 1);
        ASSERT_EQ(s.size(), (sizeof("empty") - 1));

        std::copy_n("empty", sizeof("empty") - 1, s.begin());
        ASSERT_EQ(s.size(), (sizeof("empty") - 1));
        ASSERT_EQ(std::strcmp(s.c_str(), "empty"), 0);
    }
}

TEST(u8string_test, resize_failure_preserves_string)
{
    auto s = u8string_buffer{"asdf"};
    ASSERT_THROW(s.resize((SIZE_MAX / 2) - 1), std::bad_alloc);
    ASSERT_EQ(s.size(), 4);
    ASSERT_EQ(s.compare("asdf", 4), 0);
}

TEST(u8string_test, promote)
{
    {
        auto buf = u8string_buffer{"testing"};
        buf.append(' ');
        buf.append("\xf0\x9f\x8d\x8c");

        auto s = buf.promote();
        ASSERT_EQ(s, "testing 🍌");
        ASSERT_TRUE(buf.empty());
    }

    {
        auto buf = u8string_buffer{"testing"};
        buf.append(' ');
        buf.append("\xf0\x9f\x8d");     // incomplete UTF-8 byte sequence

        ASSERT_THROW(buf.promote(), std::runtime_error);
        ASSERT_TRUE(buf.empty());
    }
}

TEST(u8string_test, erase_iterator_range)
{
    auto s = u8string_buffer{"hello, world!"};
    s.erase(s.cbegin(), s.cbegin());

    ASSERT_EQ(s, "hello, world!"sv);
    ASSERT_EQ(s.size(), 13);
    ASSERT_EQ(s[13], '\0');

    s.erase(s.cbegin() + 5, s.cbegin() + 6);
    ASSERT_EQ(s, "hello world!"sv);
    ASSERT_EQ(s.size(), 12);
    ASSERT_EQ(s[12], '\0');

    s.erase(s.cbegin(), s.cbegin() + 6);
    ASSERT_EQ(s, "world!"sv);
    ASSERT_EQ(s.size(), 6);
    ASSERT_EQ(s[6], '\0');

    s.erase(s.cbegin(), s.cend());
    ASSERT_EQ(s, ""sv);
    ASSERT_EQ(s.size(), 0);
    ASSERT_EQ(rep_ptr(s), nullptr);
}

TEST(u8string_test, appendf)
{
    {
        u8string_buffer s{"abc"};
        s.appendf("%d", 123);
        ASSERT_EQ(s, "abc123");
    }

    {
        u8string_buffer s;
        s.appendf("%s, %s!", "hello", "world");
        ASSERT_EQ(s, "hello, world!");
    }
}

TEST(u8string_test, concatenation)
{
    auto a = u8string{"path"};
    auto b = u8string{"to"};
    auto c = u8string{"file"};
    EXPECT_EQ("/" + a + "/" + b + "/" + c, "/path/to/file");

    auto d = u8string_buffer{"/"};
    d.append(a);
    d.append('/');
    d += b;
    d += '/';
    d += c;
    EXPECT_EQ(d, "/path/to/file");
}


TEST(u8string_test, substr)
{
    {
        auto const s = u8string{"hello, world"};

        EXPECT_EQ(s.substr(0),    "hello, world");
        EXPECT_EQ(s.substr(0, 5), "hello");
        EXPECT_EQ(s.substr(5, 2), ", ");
        EXPECT_EQ(s.substr(7),    "world");
        EXPECT_EQ(s.substr(0, 0), "");

        EXPECT_THROW(s.substr(13, 0), std::runtime_error);
    }

    {
        auto const s = u8string{"contains \U0001f34c UTF-8 byte sequence"};

        EXPECT_EQ(s.substr(0, 13), "contains \U0001f34c");
        EXPECT_EQ(s.substr(14),    "UTF-8 byte sequence");

        EXPECT_THROW(s.substr(10), std::runtime_error);
        EXPECT_THROW(s.substr(11), std::runtime_error);
        EXPECT_THROW(s.substr(12), std::runtime_error);

        EXPECT_THROW(s.substr(0, 10), std::runtime_error);
        EXPECT_THROW(s.substr(0, 11), std::runtime_error);
        EXPECT_THROW(s.substr(0, 12), std::runtime_error);
    }
}

TEST(u8string_test, from_utf8)
{
    EXPECT_EQ(u8string::from_utf8(u8"\u1e11\U0001f34c"), u8"ḑ🍌");

    // Overlong UTF-8 encoding.
    EXPECT_EQ(u8string::from_utf8_lossy("\xf0\x82\x82\xac"), u8"�");

    // Encoded UTF-16 surrogate.
    EXPECT_EQ(u8string::from_utf8_lossy("\x41\xed\xa0\x80\x5a"), u8"A�Z");
    EXPECT_EQ(u8string::from_utf8_lossy("\x61\xed\xbf\xbf\x7a"), u8"a�z");

    // Illegal code point U+00110000.
    EXPECT_EQ(u8string::from_utf8_lossy("\xf4\x90\x80\x80"), u8"�");

    // Unexpected continuation byte.
    EXPECT_EQ(u8string::from_utf8_lossy("\x61\x80\x62"), u8"a�b");

    // Truncated byte sequence.
    EXPECT_EQ(u8string::from_utf8_lossy("\x61\xe2\x9a\x7a"), u8"a�z");
    EXPECT_EQ(u8string::from_utf8_lossy("\xe2\x9a"), u8"�");
}

TEST(u8string_test, from_utf16)
{
    EXPECT_EQ(u8string::from_utf16(u"\u1e11\U0001f34c"), u8"ḑ🍌");

    // Unpaired lead surrogate.
    EXPECT_EQ(u8string::from_utf16_lossy(u"\x1e11\xd834\x007a"), u8"ḑ�z");
    EXPECT_EQ(u8string::from_utf16_lossy(u"\x1e11\xd834"), u8"ḑ�");

    // Unpaired trail surrogate.
    EXPECT_EQ(u8string::from_utf16_lossy(u"\x1e11\xdd0b\x007a"), u8"ḑ�z");
    EXPECT_EQ(u8string::from_utf16_lossy(u"\x1e11\xdd0b"), u8"ḑ�");
}

TEST(u8string_test, from_utf32)
{
    EXPECT_EQ(u8string::from_utf32(U"\u1e11\U0001f34c"), u8"ḑ🍌");

    // Illegal code point U+00110000.
    EXPECT_EQ(u8string::from_utf32_lossy(U"a\x00110000z"), u8"a�z");

    // Encoded UTF-16 surrogate.
    EXPECT_EQ(u8string::from_utf32_lossy(U"A\xd834\xdd0bZ"), u8"A��Z");
}

TEST(u8string_test, from_cp1252)
{
    EXPECT_EQ(u8string::from_cp1252("\xd0\xdf\xfe"), u8"Ðßþ");
    EXPECT_EQ(u8string::from_cp1252("\x80\x96\xdf"), u8"€–ß");

    // Contains invalid character '\x81'.
    EXPECT_EQ(u8string::from_cp1252_lossy("\x80\x81\x9a"), u8"€�š");
}


TEST(u8string_test, is_valid_utf8)
{
    // Empty string.
    EXPECT_TRUE(is_valid_utf8(u8""));

    // String containing only ASCII characters.
    EXPECT_TRUE(is_valid_utf8(u8"ASCII"));

    // String with Unicode literals.
    EXPECT_TRUE(is_valid_utf8(u8"grüßen"));

    // Another string with Unicode literals.
    EXPECT_TRUE(is_valid_utf8(u8"GRÜSSEN"));

    // String with Unicode code points.
    EXPECT_TRUE(is_valid_utf8(u8"\U00013030\U00013084"));

    // Another string with Unicode code points.
    EXPECT_TRUE(is_valid_utf8(u8"\uffff\U0010ffff"));

    // 4-byte UTF-8 sequence with a non-continuation byte at the end.
    EXPECT_FALSE(is_valid_utf8(u8"\xf4\x8f\xbf\x3f"));

    // Contains UTF-16 surrogate pair.
    EXPECT_FALSE(is_valid_utf8(u8"\xed\xa0\x80\xed\xb0\x80"));

    // First byte in a UTF-8 sequence cannot exceed 0xf4.
    EXPECT_FALSE(is_valid_utf8(u8"\xf5\x8f\xbf\xbf"));

    // Contains overlong UTF-8 byte sequence.
    EXPECT_FALSE(is_valid_utf8(u8"\xc0\x83"));
}

TEST(u8string_test, is_valid_utf8_until)
{
    // Illegal initial sequence byte 0xf5.
    {
        auto const s = "\xe2\x98\xad\xf5\x8f\xbf\xbf"sv;
        EXPECT_EQ(is_valid_utf8_until(s.begin(), s.end()), s.begin() + 3);
    }

    // Unicode string with embedded null characters.
    {
        auto const s = u8"\0Ðßþ\0爆\0発"sv;
        EXPECT_EQ(is_valid_utf8_until(s.begin(), s.end()), s.end());
    }
}


TEST(u8string_test, find)
{
    static constexpr auto npos = u8string::npos;

    EXPECT_EQ(u8string{""}.find('c', 0), npos);
    EXPECT_EQ(u8string{""}.find('c', 1), npos);
    EXPECT_EQ(u8string{"abcde"}.find('c', 0), 2);
    EXPECT_EQ(u8string{"abcde"}.find('c', 1), 2);
    EXPECT_EQ(u8string{"abcde"}.find('c', 2), 2);
    EXPECT_EQ(u8string{"abcde"}.find('c', 4), npos);
    EXPECT_EQ(u8string{"abcde"}.find('c', 5), npos);
    EXPECT_EQ(u8string{"abcde"}.find('c', 6), npos);
    EXPECT_EQ(u8string{"abcdeabcde"}.find('c', 0), 2);
    EXPECT_EQ(u8string{"abcdeabcde"}.find('c', 1), 2);
    EXPECT_EQ(u8string{"abcdeabcde"}.find('c', 5), 7);
    EXPECT_EQ(u8string{"abcdeabcde"}.find('c', 9), npos);
    EXPECT_EQ(u8string{"abcdeabcde"}.find('c', 11), npos);

    EXPECT_EQ(u8string{""}.find("", 0), 0);
    EXPECT_EQ(u8string{""}.find("abcde", 0), npos);
    EXPECT_EQ(u8string{""}.find("", 1), npos);
    EXPECT_EQ(u8string{"abcde"}.find("", 0), 0);
    EXPECT_EQ(u8string{"abcde"}.find("abcde", 0), 0);
    EXPECT_EQ(u8string{"abcde"}.find("abcdeabcde", 0), npos);
    EXPECT_EQ(u8string{"abcde"}.find("", 1), 1);
    EXPECT_EQ(u8string{"abcde"}.find("abcde", 1), npos);
}

TEST(u8string_test, rfind)
{
    static constexpr auto npos = u8string::npos;

    EXPECT_EQ(u8string{""}.rfind('b', 0), npos);
    EXPECT_EQ(u8string{""}.rfind('b', 1), npos);
    EXPECT_EQ(u8string{"abcde"}.rfind('b', 0), npos);
    EXPECT_EQ(u8string{"abcde"}.rfind('b', 1), 1);
    EXPECT_EQ(u8string{"abcde"}.rfind('b', 2), 1);
    EXPECT_EQ(u8string{"abcde"}.rfind('b', 4), 1);
    EXPECT_EQ(u8string{"abcde"}.rfind('b', 5), 1);
    EXPECT_EQ(u8string{"abcde"}.rfind('b', 6), 1);
    EXPECT_EQ(u8string{"abcdeabcde"}.rfind('b', 0), npos);
    EXPECT_EQ(u8string{"abcdeabcde"}.rfind('b', 1), 1);
    EXPECT_EQ(u8string{"abcdeabcde"}.rfind('b', 5), 1);
    EXPECT_EQ(u8string{"abcdeabcde"}.rfind('b', 9), 6);
    EXPECT_EQ(u8string{"abcdeabcde"}.rfind('b', 10), 6);
    EXPECT_EQ(u8string{"abcdeabcde"}.rfind('b', 11), 6);

    EXPECT_EQ(u8string{""}.rfind("", 0), 0);
    EXPECT_EQ(u8string{""}.rfind("", 1), 0);
    EXPECT_EQ(u8string{""}.rfind("abcde", 0), npos);
    EXPECT_EQ(u8string{""}.rfind("abcde", 1), npos);
    EXPECT_EQ(u8string{""}.rfind("abcdeabcde", 0), npos);
    EXPECT_EQ(u8string{""}.rfind("abcdeabcde", 1), npos);
    EXPECT_EQ(u8string{"abcde"}.rfind("", 0), 0);
    EXPECT_EQ(u8string{"abcde"}.rfind("", 1), 1);
    EXPECT_EQ(u8string{"abcde"}.rfind("abcde", 0), 0);
    EXPECT_EQ(u8string{"abcde"}.rfind("abcde", 1), 0);
    EXPECT_EQ(u8string{"abcde"}.rfind("abcde", 5), 0);
}

TEST(u8string_test, find_first_of)
{
    static constexpr auto npos = u8string::npos;
    static constexpr auto digit = "0123456789"sv;
    static constexpr auto lower = "abcdefghijklmnopqrstuvwxyz"sv;

    EXPECT_EQ(u8string{""}.find_first_of('e', 0), npos);
    EXPECT_EQ(u8string{""}.find_first_of('e', 1), npos);
    EXPECT_EQ(u8string{"kitcj"}.find_first_of('e', 0), npos);
    EXPECT_EQ(u8string{"qkamf"}.find_first_of('e', 1), npos);
    EXPECT_EQ(u8string{"abcde"}.find_first_of('e', 0), 4);
    EXPECT_EQ(u8string{"abcde"}.find_first_of('e', 1), 4);
    EXPECT_EQ(u8string{"abcde"}.find_first_of('e', 4), 4);
    EXPECT_EQ(u8string{"abcde"}.find_first_of('e', 5), npos);

    EXPECT_EQ(u8string{"abc123"}.find_first_of(digit, 0), 3);
    EXPECT_EQ(u8string{"abc123"}.find_first_of(digit, 1), 3);
    EXPECT_EQ(u8string{"abc123"}.find_first_of(digit, 3), 3);
    EXPECT_EQ(u8string{"abc123"}.find_first_of(digit, 6), npos);
    EXPECT_EQ(u8string{"abc123"}.find_first_of(lower, 0), 0);
    EXPECT_EQ(u8string{"abc123"}.find_first_of(lower, 1), 1);
    EXPECT_EQ(u8string{"abc123"}.find_first_of(lower, 3), npos);
    EXPECT_EQ(u8string{"abc123"}.find_first_of(lower, 6), npos);
}

TEST(u8string_test, find_last_of)
{
    static constexpr auto npos = u8string::npos;
    static constexpr auto digit = "0123456789"sv;
    static constexpr auto lower = "abcdefghijklmnopqrstuvwxyz"sv;

    EXPECT_EQ(u8string{""}.find_last_of('m', 0), npos);
    EXPECT_EQ(u8string{""}.find_last_of('m', 1), npos);
    EXPECT_EQ(u8string{"kitcj"}.find_last_of('m', 0), npos);
    EXPECT_EQ(u8string{"qkamf"}.find_last_of('m', 1), npos);
    EXPECT_EQ(u8string{"nhmko"}.find_last_of('m', 2), 2);
    EXPECT_EQ(u8string{"tpsaf"}.find_last_of('m', 4), npos);
    EXPECT_EQ(u8string{"lahfb"}.find_last_of('m', 5), npos);
    EXPECT_EQ(u8string{"irkhs"}.find_last_of('m', 6), npos);

    EXPECT_EQ(u8string{"abc123"}.find_last_of(digit, 5), 5);
    EXPECT_EQ(u8string{"abc123"}.find_last_of(digit, 4), 4);
    EXPECT_EQ(u8string{"abc123"}.find_last_of(digit, 3), 3);
    EXPECT_EQ(u8string{"abc123"}.find_last_of(digit, 2), npos);
    EXPECT_EQ(u8string{"abc123"}.find_last_of(lower, 5), 2);
    EXPECT_EQ(u8string{"abc123"}.find_last_of(lower, 3), 2);
    EXPECT_EQ(u8string{"abc123"}.find_last_of(lower, 2), 2);
    EXPECT_EQ(u8string{"abc123"}.find_last_of(lower, 1), 1);
    EXPECT_EQ(u8string{"abc123"}.find_last_of(lower, 0), 0);
}

TEST(u8string_test, find_first_not_of)
{
    static constexpr auto npos = u8string::npos;
    static constexpr auto digit = "0123456789"sv;
    static constexpr auto lower = "abcdefghijklmnopqrstuvwxyz"sv;

    EXPECT_EQ(u8string{""}.find_first_not_of('q', 0), npos);
    EXPECT_EQ(u8string{""}.find_first_not_of('q', 1), npos);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of('q', 0), 0);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of('q', 1), 1);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of('q', 2), 2);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of('q', 6), npos);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of('q', 7), npos);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of('3', 0), 0);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of('3', 1), 1);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of('3', 5), npos);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of('3', 6), npos);

    EXPECT_EQ(u8string{""}.find_first_not_of("", 0), npos);
    EXPECT_EQ(u8string{""}.find_first_not_of("", 1), npos);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of(digit, 0), 0);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of(digit, 1), 1);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of(digit, 2), 2);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of(digit, 3), npos);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of(lower, 0), 3);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of(lower, 1), 3);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of(lower, 2), 3);
    EXPECT_EQ(u8string{"abc123"}.find_first_not_of(lower, 3), 3);
}

TEST(u8string_test, find_last_not_of)
{
    static constexpr auto npos = u8string::npos;
    static constexpr auto digit = "0123456789"sv;
    static constexpr auto lower = "abcdefghijklmnopqrstuvwxyz"sv;

    EXPECT_EQ(u8string{""}.find_last_not_of('q', 0), npos);
    EXPECT_EQ(u8string{""}.find_last_not_of('q', 1), npos);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of('q', 0), 0);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of('q', 1), 1);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of('q', 2), 2);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of('q', 6), 5);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of('3', 0), 0);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of('3', 5), 4);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of('3', 4), 4);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of('3', 0), 0);
    EXPECT_EQ(u8string{"333333"}.find_last_not_of('3', 5), npos);

    EXPECT_EQ(u8string{""}.find_last_not_of("", 0), npos);
    EXPECT_EQ(u8string{""}.find_last_not_of("", 1), npos);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of("", 5), 5);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of("", 2), 2);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of(digit, 5), 2);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of(digit, 3), 2);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of(digit, 0), 0);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of(lower, 5), 5);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of(lower, 3), 3);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of(lower, 2), npos);
    EXPECT_EQ(u8string{"abc123"}.find_last_not_of(lower, 0), npos);
}

