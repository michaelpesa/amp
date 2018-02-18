////////////////////////////////////////////////////////////////////////////////
//
// tests/cue_sheet_test.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/media/tags.hpp>
#include <amp/stddef.hpp>
#include <amp/string_view.hpp>
#include <amp/u8string.hpp>

#include "media/cue_sheet.hpp"

#include <gtest/gtest.h>


using namespace ::amp;


namespace {

inline auto get_meta(cue::track const& track, string_view const key)
{
    auto found = track.tags.find(key);
    return found != track.tags.end() ? found->second : u8string{};
}

}     // namespace <unnamed>


TEST(cue_sheet_test, performer_per_track)
{
    char const text[] = R"(
TITLE "Paranoid"
FILE "cdimage.wav" WAVE
TRACK 01 AUDIO
    TITLE "War Pigs"
    PERFORMER "Black Sabbath"
    INDEX 01 00:00:00
TRACK 02 AUDIO
    TITLE "Paranoid"
    PERFORMER "Black Sabbath"
    INDEX 00 07:54:42
    INDEX 01 07:57:65)";

    auto tracks = cue::parse(text);
    ASSERT_EQ(tracks.size(), 2);
    ASSERT_EQ(tracks[0].tags.size(), 6);
    ASSERT_EQ(tracks[1].tags.size(), 6);

    ASSERT_EQ(get_meta(tracks[0], tags::artist), "Black Sabbath");
    ASSERT_EQ(get_meta(tracks[1], tags::artist), "Black Sabbath");

    ASSERT_EQ(get_meta(tracks[0], tags::album_artist), "");
    ASSERT_EQ(get_meta(tracks[1], tags::album_artist), "");

    ASSERT_EQ(get_meta(tracks[0], tags::album), "Paranoid");
    ASSERT_EQ(get_meta(tracks[1], tags::album), "Paranoid");

    ASSERT_EQ(get_meta(tracks[0], tags::title), "War Pigs");
    ASSERT_EQ(get_meta(tracks[1], tags::title), "Paranoid");
}

TEST(cue_sheet_test, performer_for_sheet)
{
    char const text[] = R"(
TITLE "Paranoid"
PERFORMER "Black Sabbath"
FILE "cdimage.wav" WAVE
TRACK 01 AUDIO
    TITLE "War Pigs"
    INDEX 01 00:00:00
TRACK 02 AUDIO
    TITLE "Paranoid"
    INDEX 00 07:54:42
    INDEX 01 07:57:65)";

    auto tracks = cue::parse(text);
    ASSERT_EQ(tracks.size(), 2);
    ASSERT_EQ(tracks[0].tags.size(), 6);
    ASSERT_EQ(tracks[1].tags.size(), 6);

    ASSERT_EQ(get_meta(tracks[0], tags::artist), "Black Sabbath");
    ASSERT_EQ(get_meta(tracks[1], tags::artist), "Black Sabbath");

    ASSERT_EQ(get_meta(tracks[0], tags::album_artist), "");
    ASSERT_EQ(get_meta(tracks[1], tags::album_artist), "");

    ASSERT_EQ(get_meta(tracks[0], tags::album), "Paranoid");
    ASSERT_EQ(get_meta(tracks[1], tags::album), "Paranoid");

    ASSERT_EQ(get_meta(tracks[0], tags::title), "War Pigs");
    ASSERT_EQ(get_meta(tracks[1], tags::title), "Paranoid");
}

TEST(cue_sheet_test, equivalent_performers)
{
    char const text[] = R"(
TITLE "Paranoid"
PERFORMER "Black Sabbath"
FILE "cdimage.wav" WAVE
TRACK 01 AUDIO
    TITLE "War Pigs"
    PERFORMER "Black Sabbath"
    INDEX 01 00:00:00
TRACK 02 AUDIO
    TITLE "Paranoid"
    PERFORMER "Black Sabbath"
    INDEX 00 07:54:42
    INDEX 01 07:57:65)";

    auto tracks = cue::parse(text);
    ASSERT_EQ(tracks.size(), 2);
    ASSERT_EQ(tracks[0].tags.size(), 6);
    ASSERT_EQ(tracks[1].tags.size(), 6);

    ASSERT_EQ(get_meta(tracks[0], tags::artist), "Black Sabbath");
    ASSERT_EQ(get_meta(tracks[1], tags::artist), "Black Sabbath");

    ASSERT_EQ(get_meta(tracks[0], tags::album_artist), "");
    ASSERT_EQ(get_meta(tracks[1], tags::album_artist), "");

    ASSERT_EQ(get_meta(tracks[0], tags::album), "Paranoid");
    ASSERT_EQ(get_meta(tracks[1], tags::album), "Paranoid");

    ASSERT_EQ(get_meta(tracks[0], tags::title), "War Pigs");
    ASSERT_EQ(get_meta(tracks[1], tags::title), "Paranoid");
}

TEST(cue_sheet_test, different_performers)
{
    char const text[] = R"(
TITLE "Paranoid"
PERFORMER "Various Artists"
FILE "cdimage.wav" WAVE
TRACK 01 AUDIO
    TITLE "War Pigs"
    PERFORMER "Black Sabbath"
    INDEX 01 00:00:00
TRACK 02 AUDIO
    TITLE "Paranoid"
    PERFORMER "Black Sabbath"
    INDEX 00 07:54:42
    INDEX 01 07:57:65)";

    auto tracks = cue::parse(text);
    ASSERT_EQ(tracks.size(), 2);
    ASSERT_EQ(tracks[0].tags.size(), 7);
    ASSERT_EQ(tracks[1].tags.size(), 7);

    ASSERT_EQ(get_meta(tracks[0], tags::artist), "Black Sabbath");
    ASSERT_EQ(get_meta(tracks[1], tags::artist), "Black Sabbath");

    ASSERT_EQ(get_meta(tracks[0], tags::album_artist), "Various Artists");
    ASSERT_EQ(get_meta(tracks[1], tags::album_artist), "Various Artists");

    ASSERT_EQ(get_meta(tracks[0], tags::album), "Paranoid");
    ASSERT_EQ(get_meta(tracks[1], tags::album), "Paranoid");

    ASSERT_EQ(get_meta(tracks[0], tags::title), "War Pigs");
    ASSERT_EQ(get_meta(tracks[1], tags::title), "Paranoid");
}

