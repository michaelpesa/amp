////////////////////////////////////////////////////////////////////////////////
//
// tests/string_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/string.hpp>
#include <amp/string_view.hpp>

#include <gtest/gtest.h>


using namespace ::amp;


TEST(string_test, tokenize)
{
    {
        auto const r = tokenize("", '\n');

        ASSERT_EQ(r.begin(), r.end());
    }

    {
        auto const r = tokenize("no line breaks", '\n');
        auto it = r.begin();

        ASSERT_EQ("no line breaks", *it++);
        ASSERT_EQ(r.end(), it);
    }

    {
        auto const r = tokenize("more\nthan\none\nline", '\n');
        auto it = r.begin();

        ASSERT_EQ("more", *it++);
        ASSERT_EQ("than", *it++);
        ASSERT_EQ("one",  *it++);
        ASSERT_EQ("line", *it++);
        ASSERT_EQ(r.end(), it);
    }

    {
        auto const s = "ends\nwith\nline\nbreak\n"_sv;
        auto it = tokenize(s, '\n').begin();

        ASSERT_EQ("ends",  *it++);
        ASSERT_EQ("with",  *it++);
        ASSERT_EQ("line",  *it++);
        ASSERT_EQ("break", *it++);
    }

    {
        auto const s = "adjacent\n\nline\nbreaks\n\n"_sv;
        auto it = tokenize(s, '\n').begin();

        ASSERT_EQ("adjacent", *it++);
        ASSERT_EQ("line",     *it++);
        ASSERT_EQ("breaks",   *it++);
    }

    {
        auto const s = "    TRACK\t01 AUDIO  \r\n"_sv;
        auto const r = tokenize(s, " \t\r\n");
        auto it = r.begin();

        ASSERT_EQ("TRACK", *it++);
        ASSERT_EQ("01",    *it++);
        ASSERT_EQ("AUDIO", *it++);
        ASSERT_EQ(r.end(), it);
    }

    {
        char const s[] = "TITLE\t\"Paranoid\"\n"
                         "FILE\t\"cdimage.wav\"\tWAVE\n"
                         "TRACK\t01\t\tAUDIO\n"
                         "\tTITLE\t\"War Pigs\"\n"
                         "\tPERFORMER\t\"Black Sabbath\"\n"
                         "\t\tINDEX\t01\t00:00:00\n";

        auto const r = tokenize(s, "\r\n\t");
        auto it = r.begin();
        ASSERT_EQ("TITLE", *it++);
        ASSERT_EQ("\"Paranoid\"", *it++);
        ASSERT_EQ("FILE", *it++);
        ASSERT_EQ("\"cdimage.wav\"", *it++);
        ASSERT_EQ("WAVE", *it++);
        ASSERT_EQ("TRACK", *it++);
        ASSERT_EQ("01", *it++);
        ASSERT_EQ("AUDIO", *it++);
        ASSERT_EQ("TITLE", *it++);
        ASSERT_EQ("\"War Pigs\"", *it++);
        ASSERT_EQ("PERFORMER", *it++);
        ASSERT_EQ("\"Black Sabbath\"", *it++);
        ASSERT_EQ("INDEX", *it++);
        ASSERT_EQ("01", *it++);
        ASSERT_EQ("00:00:00", *it++);
        ASSERT_EQ(r.end(), it);
    }
}

