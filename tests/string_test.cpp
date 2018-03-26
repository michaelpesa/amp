////////////////////////////////////////////////////////////////////////////////
//
// tests/string_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/string.hpp>

#include <gtest/gtest.h>


using namespace ::amp;


TEST(tokenize_test, empty_string)
{
    {
        auto const r = tokenize("", '\n');
        ASSERT_EQ(r.begin(), r.end());
    }

    {
        auto const r = tokenize("", " \r\n\t");
        ASSERT_EQ(r.begin(), r.end());
    }
}

TEST(tokenize_test, no_delimiters_in_string)
{
    {
        auto const r = tokenize("no line breaks", '\n');
        auto it = r.begin();

        ASSERT_EQ(*it++, "no line breaks");
        ASSERT_EQ(it, r.end());
    }

    {
        auto const r = tokenize("no line breaks", "\r\n");
        auto it = r.begin();

        ASSERT_EQ(*it++, "no line breaks");
        ASSERT_EQ(it, r.end());
    }
}

TEST(tokenize_test, delimiters_in_string)
{
    {
        auto const r = tokenize("more\nthan\none\nline", '\n');
        auto it = r.begin();

        ASSERT_EQ(*it++, "more");
        ASSERT_EQ(*it++, "than");
        ASSERT_EQ(*it++, "one");
        ASSERT_EQ(*it++, "line");
        ASSERT_EQ(it, r.end());
    }

    {
        auto const r = tokenize("more\nthan\none\nline", "\r\n");
        auto it = r.begin();

        ASSERT_EQ(*it++, "more");
        ASSERT_EQ(*it++, "than");
        ASSERT_EQ(*it++, "one");
        ASSERT_EQ(*it++, "line");
        ASSERT_EQ(it, r.end());
    }
}

TEST(tokenize_test, ends_with_delimiter)
{
    {
        auto const r = tokenize("ends\nwith\nline\nbreak\n", '\n');
        auto it = r.begin();

        ASSERT_EQ(*it++, "ends");
        ASSERT_EQ(*it++, "with");
        ASSERT_EQ(*it++, "line");
        ASSERT_EQ(*it++, "break");
        ASSERT_EQ(it, r.end());
    }

    {
        auto const r = tokenize("ends\nwith\nline\nbreak\n", "\r\n");
        auto it = r.begin();

        ASSERT_EQ(*it++, "ends");
        ASSERT_EQ(*it++, "with");
        ASSERT_EQ(*it++, "line");
        ASSERT_EQ(*it++, "break");
        ASSERT_EQ(it, r.end());
    }
}

TEST(tokenize_test, adjacent_delimiters)
{
    {
        auto const r = tokenize("adjacent\n\nline\nbreaks\n\n", '\n');
        auto it = r.begin();

        ASSERT_EQ(*it++, "adjacent");
        ASSERT_EQ(*it++, "line");
        ASSERT_EQ(*it++, "breaks");
        ASSERT_EQ(it, r.end());
    }

    {
        auto const r = tokenize("adjacent\r\nline\r\n\nbreaks\r\n", "\r\n");
        auto it = r.begin();

        ASSERT_EQ(*it++, "adjacent");
        ASSERT_EQ(*it++, "line");
        ASSERT_EQ(*it++, "breaks");
        ASSERT_EQ(it, r.end());
    }
}

TEST(tokenize_test, cue_sheet)
{
    char const s[] = "TITLE\t\"Paranoid\"\n"
                     "FILE\t\"cdimage.wav\"\tWAVE\n"
                     "TRACK\t01\t\tAUDIO\n"
                     "\tTITLE\t\"War Pigs\"\n"
                     "\tPERFORMER\t\"Black Sabbath\"\n"
                     "\t\tINDEX\t01\t00:00:00\n";

    auto const r = tokenize(s, "\r\n\t");
    auto it = r.begin();
    ASSERT_EQ(*it++, "TITLE");
    ASSERT_EQ(*it++, "\"Paranoid\"");
    ASSERT_EQ(*it++, "FILE");
    ASSERT_EQ(*it++, "\"cdimage.wav\"");
    ASSERT_EQ(*it++, "WAVE");
    ASSERT_EQ(*it++, "TRACK");
    ASSERT_EQ(*it++, "01");
    ASSERT_EQ(*it++, "AUDIO");
    ASSERT_EQ(*it++, "TITLE");
    ASSERT_EQ(*it++, "\"War Pigs\"");
    ASSERT_EQ(*it++, "PERFORMER");
    ASSERT_EQ(*it++, "\"Black Sabbath\"");
    ASSERT_EQ(*it++, "INDEX");
    ASSERT_EQ(*it++, "01");
    ASSERT_EQ(*it++, "00:00:00");
    ASSERT_EQ(it, r.end());
}

