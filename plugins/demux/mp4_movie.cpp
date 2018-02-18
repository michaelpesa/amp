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
#include <amp/media/id3.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/net/endian.hpp>
#include <amp/string_view.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include "mp4_box.hpp"
#include "mp4_movie.hpp"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdio>
#include <limits>
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
        if (is_valid_utf8(item.str())) {
            tags.emplace(tags::map_common_key(item.name),
                         u8string::from_utf8_unchecked(item.str()));
        }
    }
}

void read_integer(u8string key, io::reader r, media::dictionary& tags)
{
    uint32 x;
    if (r.remain() >= 4) {
        x = r.read_unchecked<uint32,BE>();
    }
    else {
        x = r.read<uint16,BE>();
    }
    tags.emplace(std::move(key), to_u8string(x));
}

void read_pair(u8string key, io::reader r, media::dictionary& tags)
{
    uint16 part;
    r.gather<BE>(io::ignore<2>, part);

    u8string s;
    if (auto const total = r.try_read<uint16,BE>()) {
        s = u8format("%" PRIu16 "/%" PRIu16, part, *total);
    }
    else {
        s = u8format("%" PRIu16, part);
    }
    tags.emplace(std::move(key), std::move(s));
}

void read_boolean(u8string key, io::reader r, media::dictionary& tags)
{
    auto const value = (r.read<uint8>() != 0) ? "Yes" : "No";
    tags.emplace(std::move(key), u8string::from_utf8_unchecked(value));
}

void read_string(u8string key, io::reader r, media::dictionary& tags)
{
    auto const s = reinterpret_cast<char const*>(r.peek());
    tags.emplace(std::move(key), u8string::from_utf8(s, r.size()));
}

void read_genre(u8string key, io::reader r, media::dictionary& tags)
{
    auto index = r.read<uint16,BE>();
    if (--index <= std::numeric_limits<uint8>::max()) {
        auto value = id3v1::get_genre_name(static_cast<uint8>(index));
        tags.emplace(std::move(key), std::move(value));
    }
}

void read_rating(u8string key, io::reader r, media::dictionary& tags)
{
    auto const value = [&]() -> char const* {
        switch (r.read<uint8>()) {
        case 0: return "None";
        case 2: return "Clean";
        case 4: return "Explicit";
        }
        return nullptr;
    }();

    if (value != nullptr) {
        tags.emplace(std::move(key), u8string::from_utf8_unchecked(value));
    }
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
    string_view const key;
    item_type const type;

    void read(io::reader r, media::dictionary& tags) const
    {
        auto k = u8string::from_utf8_unchecked(key);
        switch (type) {
        case item_type::string:
            return read_string(std::move(k), r, tags);
        case item_type::integer:
            return read_integer(std::move(k), r, tags);
        case item_type::boolean:
            return read_boolean(std::move(k), r, tags);
        case item_type::pair:
            return read_pair(std::move(k), r, tags);
        case item_type::genre:
            return read_genre(std::move(k), r, tags);
        case item_type::rating:
            return read_rating(std::move(k), r, tags);
        }
    }
};


constexpr cxp::map<uint32, ilst_parser, 37> ilst_parsers {{
    { "aART"_4cc,    { tags::album_artist,      item_type::string  } },
    { "apID"_4cc,    { "iTunes Account ID",     item_type::string  } },
    { "cnID"_4cc,    { "iTunes Catalog ID",     item_type::integer } },
    { "cpil"_4cc,    { tags::compilation,       item_type::boolean } },
    { "cprt"_4cc,    { tags::copyright,         item_type::string  } },
    { "desc"_4cc,    { tags::description,       item_type::string  } },
    { "disk"_4cc,    { tags::disc_number,       item_type::pair    } },
    { "gnre"_4cc,    { tags::genre,             item_type::genre   } },
    { "pcst"_4cc,    { tags::podcast,           item_type::boolean } },
    { "pgap"_4cc,    { tags::gapless_album,     item_type::boolean } },
    { "purd"_4cc,    { "iTunes Purchase Date",  item_type::string  } },
    { "rtng"_4cc,    { "iTunes Content Rating", item_type::rating  } },
    { "soaa"_4cc,    { tags::album_artist_sort, item_type::string  } },
    { "soal"_4cc,    { tags::album_sort,        item_type::string  } },
    { "soar"_4cc,    { tags::artist_sort,       item_type::string  } },
    { "soco"_4cc,    { tags::composer_sort,     item_type::string  } },
    { "sonm"_4cc,    { tags::title_sort,        item_type::string  } },
    { "tmpo"_4cc,    { tags::bpm,               item_type::integer } },
    { "trkn"_4cc,    { tags::track_number,      item_type::pair    } },
    { "\251ART"_4cc, { tags::artist,            item_type::string  } },
    { "\251PRD"_4cc, { tags::producer,          item_type::string  } },
    { "\251alb"_4cc, { tags::album,             item_type::string  } },
    { "\251cmt"_4cc, { tags::comment,           item_type::string  } },
    { "\251com"_4cc, { tags::composer,          item_type::string  } },
    { "\251cpy"_4cc, { tags::copyright,         item_type::string  } },
    { "\251day"_4cc, { tags::date,              item_type::string  } },
    { "\251enc"_4cc, { tags::encoder,           item_type::string  } },
    { "\251gen"_4cc, { tags::genre,             item_type::string  } },
    { "\251grp"_4cc, { tags::group,             item_type::string  } },
    { "\251lyr"_4cc, { tags::lyrics,            item_type::string  } },
    { "\251nam"_4cc, { tags::title,             item_type::string  } },
    { "\251ope"_4cc, { tags::original_artist,   item_type::string  } },
    { "\251prd"_4cc, { tags::producer,          item_type::string  } },
    { "\251swr"_4cc, { tags::encoder,           item_type::string  } },
    { "\251too"_4cc, { tags::encoded_by,        item_type::string  } },
    { "\251wrt"_4cc, { tags::composer,          item_type::string  } },
    { "\251xyz"_4cc, { tags::location,          item_type::string  } },
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
            if (item.type == "----"_4cc) {
                read_freeform(item, tags);
            }
            else {
                auto found = ilst_parsers.find(item.type);
                if (found != ilst_parsers.end()) {
                    found->second.read(item.data, tags);
                }
            }
        }
    }
    return tags;
}

media::image movie::get_cover_art() const
{
    media::image image;
    if (!ilst) {
        return image;
    }

    auto const covr = std::find_if(
        ilst->cbegin(), ilst->cend(),
        [&](auto&& x) { return x.type == "covr"_4cc; });

    if (covr == ilst->cend()) {
        return image;
    }

    auto const mime_type = [&]() {
        switch (covr->data_type) {
        case 0x0d: return "image/jpeg";
        case 0x0e: return "image/png";
        case 0x1b: return "image/bmp";
        }
        raise(errc::invalid_data_format,
              "invalid MP4 'covr' data type: %#" PRIx32, covr->data_type);
    }();

    image.set_data(covr->data);
    image.set_mime_type(u8string::from_utf8_unchecked(mime_type));
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
        auto const ret = std::sscanf(u8string{item.str()}.c_str(),
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

