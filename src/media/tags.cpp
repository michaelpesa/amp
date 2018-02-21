////////////////////////////////////////////////////////////////////////////////
//
// media/tags.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/cxp/map.hpp>
#include <amp/media/tags.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/string.hpp>

#include "core/filesystem.hpp"
#include "media/tags.hpp"
#include "media/track.hpp"

#include <chrono>
#include <cinttypes>
#include <cstdlib>
#include <iterator>
#include <string_view>
#include <utility>


namespace amp {
namespace tags {

using ::std::string_view;


AMP_NOINLINE
char* format_hms(std::chrono::seconds const t, char* dst) noexcept
{
    // Compute sign of 't'.
    auto const sign = static_cast<uint64>(t.count() >> 63);

    // Compute |t| without the possibility of signed overflow.
    auto s = (static_cast<uint64>(t.count()) ^ sign) - sign;
    auto m = s / 60;
    auto h = m / 60;

    s %= 60;
    m %= 60;

    *(--dst) = static_cast<char>((s % 10) + '0');
    *(--dst) = static_cast<char>((s / 10) + '0');
    *(--dst) = ':';
    *(--dst) = static_cast<char>((m % 10) + '0');

    if (m >= 10 || h != 0) {
        *(--dst) = static_cast<char>((m / 10) + '0');

        if (h != 0) {
            *(--dst) = ':';
            do {
                *(--dst) = static_cast<char>((h % 10) + '0');
                h /= 10;
            }
            while (h != 0);
        }
    }

    if (sign != 0) {
        *(--dst) = '-';
    }
    return dst;
}


namespace {

struct disc_and_track_number
{
    uint8 disc;
    uint8 track;
};

auto get_disc_and_track_number(media::track const& x) noexcept
{
    auto get_number = [&](string_view const key) noexcept {
        auto found = x.tags.find(key);
        if (found != x.tags.end()) {
            char* end;
            auto const s = found->second.c_str();
            auto const v = std::strtoul(s, &end, 10);
            if (end != s && v < 100) {
                return static_cast<uint8>(v);
            }
        }
        return uint8{0};
    };

    disc_and_track_number n;
    n.disc  = get_number(tags::disc_number);
    n.track = get_number(tags::track_number);
    return n;
}

AMP_NOINLINE
u8string first_of(media::track const& x,
                  iterator_range<string_view const*> const keys) noexcept
{
    for (auto&& key : keys) {
        auto found = x.tags.find(key);
        if (found != x.tags.end()) {
            return found->second;
        }
    }
    return {};
}


constexpr string_view const album_artist_keys[] {
    tags::album_artist,
    tags::artist,
    tags::composer,
    tags::performer,
};
constexpr string_view const artist_keys[] {
    tags::artist,
    tags::album_artist,
    tags::composer,
    tags::performer,
};
constexpr string_view const artist_sort_keys[] {
    tags::artist_sort,
    tags::artist,
    tags::album_artist_sort,
    tags::album_artist,
    tags::composer_sort,
    tags::composer,
    tags::performer,
};
constexpr string_view const album_artist_sort_keys[] {
    tags::album_artist_sort,
    tags::album_artist,
    tags::artist_sort,
    tags::artist,
    tags::composer_sort,
    tags::composer,
    tags::performer,
};
constexpr string_view const composer_sort_keys[] {
    tags::composer_sort,
    tags::composer,
};
constexpr string_view const title_sort_keys[] {
    tags::title_sort,
    tags::title,
};

}     // namespace <unnamed>


AMP_NOINLINE
u8string find(media::track const& x,
              string_view const key,
              tags::scope const scope)
{
    if (scope == tags::scope::smart) {
        if (key == tags::artist) {
            return first_of(x, artist_keys);
        }
        else if (key == tags::album_artist) {
            return first_of(x, album_artist_keys);
        }
        else if (key == tags::track_artist) {
            auto const end = x.tags.end();
            auto const t = x.tags.find(tags::artist);
            auto const a = x.tags.find(tags::album_artist);

            if (t != end && a != end && t != a) {
                return t->second;
            }
            return {};
        }
        else if (key == tags::disc_track) {
            auto const n = tags::get_disc_and_track_number(x);
            if (n.track != 0) {
                if (n.disc != 0) {
                    return u8format("%" PRIu8 ".%02" PRIu8, n.disc, n.track);
                }
                return u8format("%02" PRIu8, n.track);
            }
            return {};
        }
        else if (key == tags::length) {
            auto const len = x.length<std::chrono::seconds>();
            char buf[tags::max_hms_length];
            auto s = tags::format_hms(len, std::end(buf));
            auto const size = static_cast<std::size_t>(std::end(buf) - s);
            return u8string::from_utf8_unchecked(s, size);
        }
        else if (key == tags::file_name) {
            return fs::filename(x.location.get_file_path());
        }
        else if (key == tags::directory) {
            return fs::parent_path(x.location.get_file_path());
        }
    }

    auto found = x.tags.find(key);
    if (found != x.tags.end()) {
        return found->second;
    }
    if (scope == tags::scope::smart && key == tags::title) {
        return fs::filename(x.location.get_file_path());
    }
    return {};
}

AMP_NOINLINE
bool contains(media::track const& x,
              string_view const key) noexcept
{
    if (key == tags::artist) {
        return !first_of(x, artist_keys).empty();
    }
    else if (key == tags::album_artist) {
        return !first_of(x, album_artist_keys).empty();
    }
    else if (key == tags::track_artist) {
        auto const end = x.tags.end();
        auto const t = x.tags.find(tags::artist);
        auto const a = x.tags.find(tags::album_artist);

        return (t != end && a != end && t != a);
    }
    else if (key == tags::disc_track) {
        auto const n = tags::get_disc_and_track_number(x);
        return (n.track != 0);
    }
    else if (key == tags::length) {
        return (x.length() != x.length().zero());
    }

    if (x.tags.contains(key)) {
        return true;
    }
    return (key == tags::title && !x.location.empty());
}

AMP_NOINLINE
int compare(media::track const& x,
            media::track const& y,
            string_view const key)
{
    if (key == tags::disc_track) {
        auto const n1 = tags::get_disc_and_track_number(x);
        auto const n2 = tags::get_disc_and_track_number(y);
        return ((n1.disc << 8) | n1.track) - ((n2.disc << 8) | n2.track);
    }
    else if (key == tags::length) {
        auto const n1 = x.length();
        auto const n2 = y.length();
        return int{n1 > n2} - int{n1 < n2};
    }

    iterator_range<string_view const*> sort_keys;
    if (key == tags::title) {
        sort_keys = make_range(title_sort_keys);
    }
    else if (key == tags::artist) {
        sort_keys = make_range(artist_sort_keys);
    }
    else if (key == tags::album_artist) {
        sort_keys = make_range(album_artist_sort_keys);
    }
    else if (key == tags::composer) {
        sort_keys = make_range(composer_sort_keys);
    }
    else {
        sort_keys = make_range(&key, 1);
    }

    auto s1 = first_of(x, sort_keys);
    auto s2 = first_of(y, sort_keys);
    if (key == tags::title) {
        if (s1.empty()) {
            s1 = x.location.get_file_path();
        }
        if (s2.empty()) {
            s2 = y.location.get_file_path();
        }
    }
    return s1.compare(s2);
}


namespace {

constexpr cxp::map<string_view, char const*, 66> display_names {{
    { tags::album,             "Album"                 },
    { tags::album_artist,      "Album artist"          },
    { tags::album_artist_sort, "Album artist (sort)"   },
    { tags::album_sort,        "Album (sort)"          },
    { tags::artist,            "Artist"                },
    { tags::artist_sort,       "Artist (sort)"         },
    { tags::bit_rate,          "Bit rate"              },
    { tags::bits_per_sample,   "Bits per sample"       },
    { tags::bpm,               "Beats per minute"      },
    { tags::catalog_number,    "Catalog number"        },
    { tags::channels,          "Channels"              },
    { tags::codec,             "Codec"                 },
    { tags::codec_profile,     "Codec profile"         },
    { tags::comment,           "Comment"               },
    { tags::compilation,       "Compilation"           },
    { tags::composer,          "Composer"              },
    { tags::composer_sort,     "Composer (sort)"       },
    { tags::conductor,         "Conductor"             },
    { tags::contact,           "Contact"               },
    { tags::container,         "Container"             },
    { tags::copyright,         "Copyright"             },
    { tags::creation_date,     "Creation date"         },
    { tags::date,              "Date"                  },
    { tags::description,       "Description"           },
    { tags::disc_id,           "Disc ID"               },
    { tags::disc_number,       "Disc number"           },
    { tags::disc_total,        "Disc total"            },
    { tags::disc_track,        "Track number"          },
    { tags::encoded_by,        "Encoded by"            },
    { tags::encoder,           "Encoder"               },
    { tags::encoding_settings, "Encoding settings"     },
    { tags::encoding_time,     "Encoding time"         },
    { tags::engineer,          "Engineer"              },
    { tags::gapless_album,     "Gapless album"         },
    { tags::genre,             "Genre"                 },
    { tags::isrc,              "ISRC"                  },
    { tags::label,             "Label"                 },
    { tags::length,            "Length"                },
    { tags::license,           "License"               },
    { tags::location,          "Location"              },
    { tags::lyricist,          "Lyricist"              },
    { tags::lyrics,            "Lyrics"                },
    { tags::mixer,             "Mixer"                 },
    { tags::mood,              "Mood"                  },
    { tags::orchestra,         "Orchestra"             },
    { tags::original_album,    "Original album"        },
    { tags::original_artist,   "Original artist"       },
    { tags::original_date,     "Original date"         },
    { tags::original_filename, "Original filename"     },
    { tags::original_lyricist, "Original lyricist"     },
    { tags::performer,         "Performer"             },
    { tags::producer,          "Producer"              },
    { tags::radio_station,     "Radio station"         },
    { tags::rating,            "Rating"                },
    { tags::remixer,           "Remixer"               },
    { tags::rg_album_gain,     "ReplayGain album gain" },
    { tags::rg_album_peak,     "ReplayGain album peak" },
    { tags::rg_track_gain,     "ReplayGain track gain" },
    { tags::rg_track_peak,     "ReplayGain track peak" },
    { tags::tag_type,          "Tag type"              },
    { tags::title,             "Title"                 },
    { tags::title_sort,        "Title (sort)"          },
    { tags::track_number,      "Track number"          },
    { tags::track_total,       "Track total"           },
    { tags::upc,               "UPC"                   },
    { tags::writer,            "Writer"                },
}};
static_assert(cxp::is_sorted(display_names), "");

constexpr cxp::map<string_view, string_view, 68, stricmp_less> common_keys {{
    { "acoustid fingerprint",         tags::acoustid_fingerprint },
    { "acoustid id",                  tags::acoustid_id          },
    { "album",                        tags::album                },
    { "album artist",                 tags::album_artist         },
    { "albumartist",                  tags::album_artist         },
    { "albumartistsort",              tags::album_artist_sort    },
    { "albumsort",                    tags::album_sort           },
    { "artist",                       tags::artist               },
    { "artistsort",                   tags::artist_sort          },
    { "asin",                         tags::asin                 },
    { "author",                       tags::author               },
    { "barcode",                      tags::barcode              },
    { "bpm",                          tags::bpm                  },
    { "catalog",                      tags::catalog_number       },
    { "catalognumber",                tags::catalog_number       },
    { "comment",                      tags::comment              },
    { "compilation",                  tags::compilation          },
    { "composer",                     tags::composer             },
    { "composersort",                 tags::composer_sort        },
    { "conductor",                    tags::conductor            },
    { "copyright",                    tags::copyright            },
    { "cuesheet",                     tags::cue_sheet            },
    { "date",                         tags::date                 },
    { "description",                  tags::description          },
    { "disc",                         tags::disc_number          },
    { "discid",                       tags::disc_id              },
    { "discnumber",                   tags::disc_number          },
    { "disctotal",                    tags::disc_total           },
    { "encoded-by",                   tags::encoded_by           },
    { "encodedby",                    tags::encoded_by           },
    { "encoder",                      tags::encoder              },
    { "engineer",                     tags::engineer             },
    { "genre",                        tags::genre                },
    { "isrc",                         tags::isrc                 },
    { "label",                        tags::label                },
    { "license",                      tags::license              },
    { "lyricist",                     tags::lyricist             },
    { "lyrics",                       tags::lyrics               },
    { "mixer",                        tags::mixer                },
    { "mood",                         tags::mood                 },
    { "musicbrainz album artist id",  tags::mb_album_artist_id   },
    { "musicbrainz album id",         tags::mb_album_id          },
    { "musicbrainz artist id",        tags::mb_artist_id         },
    { "musicbrainz release country",  tags::mb_release_country   },
    { "musicbrainz release group id", tags::mb_artist_id         },
    { "musicbrainz track id",         tags::mb_track_id          },
    { "musicbrainz work id",          tags::mb_work_id           },
    { "musicip puid",                 tags::musicip_puid         },
    { "organization",                 tags::label                },
    { "originaldate",                 tags::original_date        },
    { "performer",                    tags::performer            },
    { "producer",                     tags::producer             },
    { "replaygain_album_gain",        tags::rg_album_gain        },
    { "replaygain_album_peak",        tags::rg_album_peak        },
    { "replaygain_track_gain",        tags::rg_track_gain        },
    { "replaygain_track_peak",        tags::rg_track_peak        },
    { "songwriter",                   tags::writer               },
    { "tempo",                        tags::bpm                  },
    { "title",                        tags::title                },
    { "titlesort",                    tags::title_sort           },
    { "totaldiscs",                   tags::disc_total           },
    { "totaltracks",                  tags::track_total          },
    { "track",                        tags::track_number         },
    { "tracknumber",                  tags::track_number         },
    { "tracktotal",                   tags::track_total          },
    { "upc",                          tags::upc                  },
    { "writer",                       tags::writer               },
    { "year",                         tags::date                 },
}};
static_assert(cxp::is_sorted(common_keys), "");

}     // namespace <unnamed>

char const* display_name(string_view const key) noexcept
{
    auto found = display_names.find(key);
    if (found != display_names.cend()) {
        return found->second;
    }
    return nullptr;
}

u8string map_common_key(string_view const key)
{
    auto found = common_keys.find(key);
    if (found != common_keys.end()) {
        return u8string::from_utf8_unchecked(found->second);
    }
    return u8string::from_utf8(key);
}

}}    // namespace amp::tags

