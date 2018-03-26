////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/mp4_movie.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/demuxer.hpp>
#include <amp/cxp/map.hpp>
#include <amp/error.hpp>
#include <amp/io/reader.hpp>
#include <amp/media/dictionary.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/net/endian.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include "mp4_box.hpp"
#include "mp4_movie.hpp"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdio>
#include <limits>
#include <string_view>
#include <utility>


namespace amp {
namespace mp4 {
namespace {

void read_freeform(ilst_entry const& item, media::dictionary& tags)
{
    // Freeform box keys follow the format '----:mean:name', where 'mean' is
    // typically "com.apple.iTunes" and "name" is a frame ID. Most values
    // contain UTF-8 encoded text, while some contain raw binary data.

    if (item.mean == "com.apple.iTunes" &&
        item.name != "Encoding Params" &&
        item.name != "iTunSMPB" &&
        item.name != "iTunNORM" &&
        item.name != "iTunMOVI") {
        auto const str = reinterpret_cast<char const*>(item.data.data());
        auto const len = item.data.size();
        if (is_valid_utf8(str, len)) {
            tags.emplace(tags::map_common_key(item.name),
                         u8string::from_utf8_unchecked(str, len));
        }
    }
}

u8string read_integer(io::reader r)
{
    uint32 x;
    if (r.remain() >= 4) {
        x = r.read_unchecked<uint32,BE>();
    }
    else {
        x = r.read<uint16,BE>();
    }
    return to_u8string(x);
}

u8string read_pair(io::reader r)
{
    uint16 part;
    r.gather<BE>(io::ignore<2>, part);

    char buf[16];
    if (auto const total = r.try_read<uint16,BE>()) {
        std::snprintf(buf, sizeof(buf), "%d/%d", part, *total);
    }
    else {
        std::snprintf(buf, sizeof(buf), "%d", part);
    }
    return u8string::from_utf8_unchecked(buf);
}

u8string read_boolean(io::reader r)
{
    auto const value = (r.read<uint8>() != 0) ? "Yes" : "No";
    return u8string::from_utf8_unchecked(value);
}

u8string read_string(io::reader r)
{
    return u8string{reinterpret_cast<char const*>(r.peek()), r.size()};
}

u8string read_genre(io::reader r)
{
    auto index = r.read<uint16,BE>();
    if (--index <= std::numeric_limits<uint8>::max()) {
        return id3v1::get_genre_name(static_cast<uint8>(index));
    }
    return {};
}

u8string read_rating(io::reader r)
{
    auto const value = [&]() -> char const* {
        switch (r.read<uint8>()) {
        case 0: return "None";
        case 2: return "Clean";
        case 4: return "Explicit";
        }
        return nullptr;
    }();
    return u8string::from_utf8_unchecked(value);
}


enum class item_type : uint8 {
    string,
    integer,
    boolean,
    pair,
    genre,
    rating,
};


struct ilst_parser
{
    std::string_view key;
    u8string (*func)(io::reader);

    void read(io::reader r, media::dictionary& tags) const
    {
        auto value = func(r);
        if (!value.empty()) {
            tags.emplace(u8string::from_utf8_unchecked(key), std::move(value));
        }
    }
};


constexpr cxp::map<uint32, ilst_parser, 37> ilst_parsers {{
    { "aART"_4cc,    { tags::album_artist,      read_string  } },
    { "apID"_4cc,    { "iTunes Account ID",     read_string  } },
    { "cnID"_4cc,    { "iTunes Catalog ID",     read_integer } },
    { "cpil"_4cc,    { tags::compilation,       read_boolean } },
    { "cprt"_4cc,    { tags::copyright,         read_string  } },
    { "desc"_4cc,    { tags::description,       read_string  } },
    { "disk"_4cc,    { tags::disc_number,       read_pair    } },
    { "gnre"_4cc,    { tags::genre,             read_genre   } },
    { "pcst"_4cc,    { tags::podcast,           read_boolean } },
    { "pgap"_4cc,    { tags::gapless_album,     read_boolean } },
    { "purd"_4cc,    { "iTunes Purchase Date",  read_string  } },
    { "rtng"_4cc,    { "iTunes Content Rating", read_rating  } },
    { "soaa"_4cc,    { tags::album_artist_sort, read_string  } },
    { "soal"_4cc,    { tags::album_sort,        read_string  } },
    { "soar"_4cc,    { tags::artist_sort,       read_string  } },
    { "soco"_4cc,    { tags::composer_sort,     read_string  } },
    { "sonm"_4cc,    { tags::title_sort,        read_string  } },
    { "tmpo"_4cc,    { tags::bpm,               read_integer } },
    { "trkn"_4cc,    { tags::track_number,      read_pair    } },
    { "\251ART"_4cc, { tags::artist,            read_string  } },
    { "\251PRD"_4cc, { tags::producer,          read_string  } },
    { "\251alb"_4cc, { tags::album,             read_string  } },
    { "\251cmt"_4cc, { tags::comment,           read_string  } },
    { "\251com"_4cc, { tags::composer,          read_string  } },
    { "\251cpy"_4cc, { tags::copyright,         read_string  } },
    { "\251day"_4cc, { tags::date,              read_string  } },
    { "\251enc"_4cc, { tags::encoder,           read_string  } },
    { "\251gen"_4cc, { tags::genre,             read_string  } },
    { "\251grp"_4cc, { tags::group,             read_string  } },
    { "\251lyr"_4cc, { tags::lyrics,            read_string  } },
    { "\251nam"_4cc, { tags::title,             read_string  } },
    { "\251ope"_4cc, { tags::original_artist,   read_string  } },
    { "\251prd"_4cc, { tags::producer,          read_string  } },
    { "\251swr"_4cc, { tags::encoder,           read_string  } },
    { "\251too"_4cc, { tags::encoded_by,        read_string  } },
    { "\251wrt"_4cc, { tags::composer,          read_string  } },
    { "\251xyz"_4cc, { tags::location,          read_string  } },
}};
static_assert(cxp::is_sorted(ilst_parsers), "");

}     // namespace <unnamed>


media::dictionary movie::get_tags() const
{
    media::dictionary tags;
    if (!ilst) {
        return tags;
    }

    tags.reserve(ilst->size());
    for (auto&& item : *ilst) {
        if (!item.data.empty()) {
            auto found = ilst_parsers.find(item.type);
            if (found != ilst_parsers.end()) {
                found->second.read(item.data, tags);
            }
            else if (item.type == "----"_4cc) {
                read_freeform(item, tags);
            }
        }
    }
    return tags;
}

media::image movie::get_cover_art() const
{
    if (!ilst) {
        return {};
    }

    auto const covr = std::find_if(
        ilst->cbegin(), ilst->cend(),
        [&](auto&& x) { return x.type == "covr"_4cc; });

    if (covr == ilst->cend()) {
        return {};
    }

    char const* mime_type;
    switch (covr->data_type) {
    case 0x0d:
        mime_type = "image/jpeg";
        break;
    case 0x0e:
        mime_type = "image/png";
        break;
    case 0x1b:
        mime_type = "image/bmp";
        break;
    default:
        std::fprintf(stderr, "[MP4] invalid 'covr' data type: %#x",
                     covr->data_type);
        return {};
    }

    media::image image;
    image.set_mime_type(u8string::from_utf8_unchecked(mime_type));
    image.set_data(covr->data);
    return image;
}

optional<iTunSMPB> movie::get_iTunSMPB() const
{
    if (!ilst) {
        return nullopt;
    }

    for (auto&& item : *ilst) {
        if (item.type != "----"_4cc         ||
            item.mean != "com.apple.iTunes" ||
            item.name != "iTunSMPB") {
            continue;
        }

        iTunSMPB out;
        auto const ret = std::sscanf(read_string(item.data).c_str(),
                                     "%*8" SCNx32 " %8"  SCNx32
                                     " %8" SCNx32 " %16" SCNx64,
                                     &out.priming, &out.padding, &out.frames);
        if (ret == 3) {
            return make_optional(out);
        }
    }
    return nullopt;
}

}}    // namespace amp::mp4

