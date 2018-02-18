////////////////////////////////////////////////////////////////////////////////
//
// tests/media_dictionary_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/media/dictionary.hpp>
#include <amp/stddef.hpp>

#include <gtest/gtest.h>


using namespace ::amp;


TEST(media_dictionary_test, insert_or_assign)
{
    auto dict = media::dictionary {
        { "album",        "Album 1"        },
        { "album artist", "Album Artist 1" },
        { "album artist", "Album Artist 2" },
        { "artist",       "Artist 1"       },
    };

    ASSERT_EQ(dict.size(), 4);

    auto pos = dict.find("album artist");
    EXPECT_EQ((pos++)->second, "Album Artist 1");
    EXPECT_EQ((pos++)->second, "Album Artist 2");
    EXPECT_EQ(dict.count("album artist"), 2);

    pos = dict.insert_or_assign("album artist", "Album Artist 3");
    EXPECT_EQ(pos->second, "Album Artist 3");
    EXPECT_EQ(dict.count("album artist"), 1);

    pos = dict.find("artist");
    EXPECT_EQ(pos->second, "Artist 1");
    EXPECT_EQ(dict.count("artist"), 1);

    pos = dict.insert_or_assign("artist", "Artist 2");
    pos = dict.find("artist");
    EXPECT_EQ(pos->second, "Artist 2");
    EXPECT_EQ(dict.count("album artist"), 1);
}

TEST(media_dictionary_test, insert_hint)
{
    auto dict = media::dictionary {
        { "album", "Example Album 1" },
    };

    dict.insert(dict.cend(), { "album", "Example Album 2" });
    ASSERT_EQ(dict.size(), 2);

    auto pos = dict.cbegin();
    EXPECT_EQ((pos++)->second, "Example Album 1");
    EXPECT_EQ((pos++)->second, "Example Album 2");
    ASSERT_EQ(pos, dict.cend());
}

TEST(media_dictionary_test, erase)
{
    media::dictionary d1 {
        { "title",      "Example Title"     },
        { "artist",     "Example Artist 1"  },
        { "album",      "Example Album"     },
        { "genre",      "Progressive Rock"  },
        { "artist",     "Example Artist 2"  },
    };

    auto pos = d1.begin();

    ASSERT_EQ(d1.size(), 5);
    ASSERT_EQ((pos++)->second, "Example Album");
    ASSERT_EQ((pos++)->second, "Example Artist 1");
    ASSERT_EQ((pos++)->second, "Example Artist 2");
    ASSERT_EQ((pos++)->second, "Progressive Rock");
    ASSERT_EQ((pos++)->second, "Example Title");
    ASSERT_EQ(pos, d1.end());

    pos = d1.erase(d1.cbegin() + 1);
    ASSERT_EQ(d1.size(), 4);
    ASSERT_EQ(pos, d1.begin() + 1);
    ASSERT_EQ((pos++)->second, "Example Artist 2");
    ASSERT_EQ((pos++)->second, "Progressive Rock");
    ASSERT_EQ((pos++)->second, "Example Title");
    ASSERT_EQ(pos, d1.end());

    pos = d1.erase(d1.cbegin(), d1.cbegin() + 2);
    ASSERT_EQ(d1.size(), 2);
    ASSERT_EQ(pos, d1.begin());
    ASSERT_EQ((pos++)->second, "Progressive Rock");
    ASSERT_EQ((pos++)->second, "Example Title");
    ASSERT_EQ(pos, d1.end());

    pos = d1.erase(d1.cbegin(), d1.cbegin());
    ASSERT_EQ(d1.size(), 2);
    ASSERT_EQ(pos, d1.begin());
    ASSERT_EQ((pos++)->second, "Progressive Rock");
    ASSERT_EQ((pos++)->second, "Example Title");
    ASSERT_EQ(pos, d1.end());
}

TEST(media_dictionary_test, merge)
{
    auto x = media::dictionary {
        { "album",      "Example Album"    },
        { "artist",     "Example Artist 1" },
        { "artist",     "Example Artist 2" },
        { "artist",     "Example Artist 3" },
        { "title",      "Example Title"    },
    };
    auto y = media::dictionary {
        { "album",      "Another Album 1"  },
        { "album",      "Another Album 2"  },
        { "artist",     "Another Artist 1" },
        { "artist",     "Another Artist 2" },
        { "genre",      "Another Genre 1"  },
        { "genre",      "Another Genre 2"  },
        { "title",      "Another Title"    },
    };

    x.merge(y);
    ASSERT_EQ(y.size(), 7);
    ASSERT_EQ(x.size(), 7);

    auto pos = x.cbegin();
    EXPECT_EQ((pos++)->second, "Example Album");
    EXPECT_EQ((pos++)->second, "Example Artist 1");
    EXPECT_EQ((pos++)->second, "Example Artist 2");
    EXPECT_EQ((pos++)->second, "Example Artist 3");
    EXPECT_EQ((pos++)->second, "Another Genre 1");
    EXPECT_EQ((pos++)->second, "Another Genre 2");
    EXPECT_EQ((pos++)->second, "Example Title");
    ASSERT_EQ(pos, x.cend());

    x = {};
    y = {
        { "album",      "Example Album"    },
        { "artist",     "Example Artist 1" },
        { "artist",     "Example Artist 2" },
        { "title",      "Example Title"    },
    };

    ASSERT_EQ(x.size(), 0);
    ASSERT_EQ(y.size(), 4);

    x.merge(y);
    ASSERT_EQ(x.size(), 4);
    ASSERT_EQ(y.size(), 4);

    pos = x.cbegin();
    EXPECT_EQ((pos++)->second, "Example Album");
    EXPECT_EQ((pos++)->second, "Example Artist 1");
    EXPECT_EQ((pos++)->second, "Example Artist 2");
    EXPECT_EQ((pos++)->second, "Example Title");
    ASSERT_EQ(pos, x.cend());

    x = {
        { "album",      "Example Album"    },
        { "artist",     "Example Artist 1" },
        { "artist",     "Example Artist 2" },
        { "title",      "Example Title"    },
    };
    y = {};

    ASSERT_EQ(x.size(), 4);
    ASSERT_EQ(y.size(), 0);

    x.merge(y);
    ASSERT_EQ(x.size(), 4);
    ASSERT_EQ(y.size(), 0);

    pos = x.cbegin();
    EXPECT_EQ((pos++)->second, "Example Album");
    EXPECT_EQ((pos++)->second, "Example Artist 1");
    EXPECT_EQ((pos++)->second, "Example Artist 2");
    EXPECT_EQ((pos++)->second, "Example Title");
    ASSERT_EQ(pos, x.cend());
}

