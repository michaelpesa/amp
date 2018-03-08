////////////////////////////////////////////////////////////////////////////////
//
// media/id3v2.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/cxp/map.hpp>
#include <amp/cxp/string.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/reader.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/dictionary.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/net/endian.hpp>
#include <amp/optional.hpp>
#include <amp/stddef.hpp>
#include <amp/string.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include <cinttypes>
#include <cstdio>
#include <string_view>
#include <type_traits>


namespace amp {
namespace id3v2 {
namespace {

constexpr cxp::map<uint32, uint32, 64> frame_id_to_v24_map {{
    { "\0BUF"_4cc, "RBUF"_4cc },
    { "\0CNT"_4cc, "PCNT"_4cc },
    { "\0COM"_4cc, "COMM"_4cc },
    { "\0CRA"_4cc, "AENC"_4cc },
    { "\0ETC"_4cc, "ETCO"_4cc },
    { "\0GEO"_4cc, "GEOB"_4cc },
    { "\0IPL"_4cc, "TIPL"_4cc },
    { "\0MCI"_4cc, "MCDI"_4cc },
    { "\0MLL"_4cc, "MLLT"_4cc },
    { "\0PIC"_4cc, "APIC"_4cc },
    { "\0POP"_4cc, "POPM"_4cc },
    { "\0REV"_4cc, "RVRB"_4cc },
    { "\0SLT"_4cc, "SYLT"_4cc },
    { "\0STC"_4cc, "SYTC"_4cc },
    { "\0TAL"_4cc, "TALB"_4cc },
    { "\0TBP"_4cc, "TBPM"_4cc },
    { "\0TCM"_4cc, "TCOM"_4cc },
    { "\0TCO"_4cc, "TCON"_4cc },
    { "\0TCP"_4cc, "TCMP"_4cc },
    { "\0TCR"_4cc, "TCOP"_4cc },
    { "\0TDY"_4cc, "TDLY"_4cc },
    { "\0TEN"_4cc, "TENC"_4cc },
    { "\0TFT"_4cc, "TFLT"_4cc },
    { "\0TKE"_4cc, "TKEY"_4cc },
    { "\0TLA"_4cc, "TLAN"_4cc },
    { "\0TMT"_4cc, "TMED"_4cc },
    { "\0TOA"_4cc, "TOAL"_4cc },
    { "\0TOF"_4cc, "TOFN"_4cc },
    { "\0TOL"_4cc, "TOLY"_4cc },
    { "\0TOR"_4cc, "TDOR"_4cc },
    { "\0TOT"_4cc, "TOAL"_4cc },
    { "\0TP1"_4cc, "TPE1"_4cc },
    { "\0TP2"_4cc, "TPE2"_4cc },
    { "\0TP3"_4cc, "TPE3"_4cc },
    { "\0TP4"_4cc, "TPE4"_4cc },
    { "\0TPA"_4cc, "TPOS"_4cc },
    { "\0TPB"_4cc, "TPUB"_4cc },
    { "\0TRC"_4cc, "TSRC"_4cc },
    { "\0TRD"_4cc, "TDRC"_4cc },
    { "\0TRK"_4cc, "TRCK"_4cc },
    { "\0TS2"_4cc, "TSO2"_4cc },
    { "\0TSA"_4cc, "TSOA"_4cc },
    { "\0TSC"_4cc, "TSOC"_4cc },
    { "\0TSP"_4cc, "TSOP"_4cc },
    { "\0TSS"_4cc, "TSSE"_4cc },
    { "\0TST"_4cc, "TSOT"_4cc },
    { "\0TT1"_4cc, "TIT1"_4cc },
    { "\0TT2"_4cc, "TIT2"_4cc },
    { "\0TT3"_4cc, "TIT3"_4cc },
    { "\0TXT"_4cc, "TOLY"_4cc },
    { "\0TXX"_4cc, "TXXX"_4cc },
    { "\0TYE"_4cc, "TDRC"_4cc },
    { "\0UFI"_4cc, "UFID"_4cc },
    { "\0ULT"_4cc, "USLT"_4cc },
    { "\0WAF"_4cc, "WOAF"_4cc },
    { "\0WAR"_4cc, "WOAR"_4cc },
    { "\0WAS"_4cc, "WOAS"_4cc },
    { "\0WCM"_4cc, "WCOM"_4cc },
    { "\0WCP"_4cc, "WCOP"_4cc },
    { "\0WPB"_4cc, "WPUB"_4cc },
    { "\0WXX"_4cc, "WXXX"_4cc },
    {  "IPLS"_4cc, "TIPL"_4cc },
    {  "TORY"_4cc, "TDOR"_4cc },
    {  "TYER"_4cc, "TDRC"_4cc },
}};
static_assert(cxp::is_sorted(frame_id_to_v24_map), "");

constexpr cxp::map<uint32, std::string_view, 44> text_frame_map {{
    { "TALB"_4cc, tags::album               },
    { "TBPM"_4cc, tags::bpm                 },
    { "TCMP"_4cc, tags::compilation         },
    { "TCOM"_4cc, tags::composer            },
    { "TCON"_4cc, tags::genre               },
    { "TCOP"_4cc, tags::copyright           },
    { "TDEN"_4cc, tags::encoding_time       },
    { "TDLY"_4cc, tags::playlist_delay      },
    { "TDOR"_4cc, tags::original_date       },
    { "TDRC"_4cc, tags::date                },
    { "TDRL"_4cc, tags::date                },
    { "TDTG"_4cc, tags::tagging_date        },
    { "TENC"_4cc, tags::encoded_by          },
    { "TEXT"_4cc, tags::lyricist            },
    { "TFLT"_4cc, tags::file_type           },
    { "TIT1"_4cc, tags::group               },
    { "TIT2"_4cc, tags::title               },
    { "TIT3"_4cc, tags::subtitle            },
    { "TKEY"_4cc, tags::initial_key         },
    { "TLAN"_4cc, tags::language            },
    { "TMED"_4cc, tags::media_type          },
    { "TMOO"_4cc, tags::mood                },
    { "TOAL"_4cc, tags::original_album      },
    { "TOFN"_4cc, tags::original_filename   },
    { "TOLY"_4cc, tags::original_lyricist   },
    { "TOPE"_4cc, tags::original_artist     },
    { "TOWN"_4cc, tags::owner               },
    { "TPE1"_4cc, tags::artist              },
    { "TPE2"_4cc, tags::album_artist        },
    { "TPE3"_4cc, tags::conductor           },
    { "TPE4"_4cc, tags::remixer             },
    { "TPOS"_4cc, tags::disc_number         },
    { "TPRO"_4cc, tags::produced_notice     },
    { "TPUB"_4cc, tags::label               },
    { "TRCK"_4cc, tags::track_number        },
    { "TRSN"_4cc, tags::radio_station       },
    { "TRSO"_4cc, tags::radio_station_owner },
    { "TSO2"_4cc, tags::album_artist_sort   },
    { "TSOA"_4cc, tags::album_sort          },
    { "TSOC"_4cc, tags::composer_sort       },
    { "TSOP"_4cc, tags::artist_sort         },
    { "TSOT"_4cc, tags::title_sort          },
    { "TSRC"_4cc, tags::isrc                },
    { "TSSE"_4cc, tags::encoding_settings   },
}};
static_assert(cxp::is_sorted(text_frame_map), "");

constexpr cxp::map<uint32, std::string_view, 9> url_frame_map {{
    { "WCOM"_4cc, tags::commercial_information },
    { "WCOP"_4cc, tags::copyright_information  },
    { "WOAF"_4cc, tags::file_web_page          },
    { "WOAR"_4cc, tags::artist_web_page        },
    { "WOAS"_4cc, tags::audio_source_web_page  },
    { "WORS"_4cc, tags::radio_station_web_page },
    { "WPAY"_4cc, tags::payment_web_page       },
    { "WPUB"_4cc, tags::publisher_web_page     },
    { "WXXX"_4cc, tags::user_web_page          },
}};
static_assert(cxp::is_sorted(url_frame_map), "");


enum : uint8 {
    header_flag_unsynchronization = 0x80,
    header_flag_extended_header   = 0x40,
    header_flag_experimental      = 0x20,
    header_flag_footer_present    = 0x10,
};

enum : uint16 {
    frame_flag_frame_alter_preserve  = 0x4000,
    frame_flag_file_alter_preserve   = 0x2000,
    frame_flag_read_only             = 0x1000,
    frame_flag_grouping_id           = 0x0040,
    frame_flag_compression           = 0x0008,
    frame_flag_encryption            = 0x0004,
    frame_flag_unsynchronization     = 0x0002,
    frame_flag_data_length_indicator = 0x0001,
};


constexpr uint32 unsynchsafe(uint32 const x) noexcept
{
    return ((x & 0x0000007f) >> 0)
         | ((x & 0x00007f00) >> 1)
         | ((x & 0x007f0000) >> 2)
         | ((x & 0x7f000000) >> 3);
}

constexpr uint32 load_uint24BE(uint8 const* const p) noexcept
{
    return (uint32{p[0]} << 16) | (uint32{p[1]} << 8) | p[2];
}

inline bool is_valid_frame_id(uint32 const id, uint8 const version) noexcept
{
    for (auto i = ((version > 2) ? 4 : 3); i-- != 0; ) {
        auto const c = static_cast<char>(id >> (i * 8));
        if ((c < 'A' || c > 'Z') && (c < '0' && c > '9')) {
            return false;
        }
    }
    return true;
}


struct header
{
    static optional<id3v2::header> read(io::stream& file)
    {
        char id[3];
        id3v2::header h;
        file.gather<BE>(id, h.version, h.revision, h.flags, h.size);

        if (id[0] == 'I' && id[1] == 'D' && id[2] == '3' && h.version <= 4) {
            if (!(h.size & 0x80808080)) {
                h.size = id3v2::unsynchsafe(h.size);
                return make_optional(h);
            }
        }
        return nullopt;
    }

    uint8 version;
    uint8 revision;
    uint8 flags;
    uint32 size;
};

struct frame_header
{
    explicit frame_header(io::reader& r, uint8 const version)
    {
        switch (version) {
        case 4:
            r.gather<BE>(id, size, flags);
            if (size & 0x80808080) {
                raise(errc::failure, "ID3v2: invalid synchsafe integer");
            }
            size = id3v2::unsynchsafe(size);
            break;
        case 3:
            r.gather<BE>(id, size, flags);
            flags = static_cast<uint16>(((flags & 0xe000) >> 1) |
                                        ((flags & 0x00c0) >> 4) |
                                        ((flags & 0x0020) << 1));
            break;
        case 2:
        case 1:
        case 0:
            uint8 buf[6];
            r.read(buf);
            id = load_uint24BE(&buf[0]);
            size = load_uint24BE(&buf[3]);
            flags = 0;
            break;
        default:
            AMP_UNREACHABLE();
        }

        if (!id3v2::is_valid_frame_id(id, version)) {
            raise(errc::failure, "invalid ID3v2 frame ID");
        }

        if (version < 4) {
            auto found = frame_id_to_v24_map.find(id);
            if (found != frame_id_to_v24_map.end()) {
                id = found->second;
            }
        }
    }

    uint32 id;
    uint32 size;
    uint16 flags;
};


AMP_NOINLINE
auto read_string(io::reader& r,
                 string_encoding const enc = string_encoding::latin1)
{
    auto str = reinterpret_cast<char const*>(r.peek());
    auto len = 0_sz;

    if (enc == string_encoding::utf8 || enc == string_encoding::latin1) {
        auto const limit = r.remain();
        len = cxp::strlen(str, limit);
        r.skip_unchecked(len + (len != limit));
    }
    else {
        auto const limit = r.remain() / 2;
        for (; len != limit; ++len) {
            if (io::load<char16>(&str[len * 2]) == u'\0') {
                break;
            }
        }
        r.skip_unchecked(2 * (len + (len != limit)));
    }
    return u8string::from_encoding_lossy(str, len, enc);
}

inline auto read_string_encoding(io::reader& r)
{
    switch (r.read<uint8>()) {
    case 0: return string_encoding::latin1;
    case 1: return string_encoding::utf16;
    case 2: return string_encoding::utf16be;
    case 3: return string_encoding::utf8;
    }
    raise(errc::out_of_bounds, "illegal ID3v2 text encoding");
}


class frame_parser
{
public:
    explicit frame_parser(io::stream& file, id3v2::header const& h) :
        header(h),
        tag_buf(file, header.size),
        r(tag_buf),
        frame_header_size(header.version >= 3 ? 10 : 6)
    {
        if (header.flags & header_flag_unsynchronization) {
            if (header.version <= 3) {
                header.flags &= ~header_flag_unsynchronization;
                reverse_unsynchronization(tag_buf);
                r = io::reader{tag_buf};
            }
        }
        if (header.flags & header_flag_extended_header) {
            read_extended_header();
        }
    }

    uint32 operator()(io::buffer& out)
    {
        if (r.remain() >= frame_header_size) {
            return read_frame_header(out);
        }
        return 0;
    }

private:
    uint32 read_frame_header(io::buffer& out)
    {
        auto const frame = id3v2::frame_header(r, header.version);
        if (frame.id != 0) {
            if ((frame.flags & frame_flag_encryption) ||
                (frame.flags & frame_flag_compression)) {
                raise(errc::not_implemented,
                      "ID3v2 encrypted and/or compressed frames are "
                      "currently not supported");
            }

            auto frame_data_length = frame.size;
            if (frame.flags & frame_flag_data_length_indicator) {
                if (frame_data_length < 4) {
                    raise(errc::out_of_bounds,
                          "ID3v2 frame data is too small");
                }
                r.skip(4);
                frame_data_length -= 4;
            }
            if (frame.flags & frame_flag_grouping_id) {
                if (frame_data_length < 1) {
                    raise(errc::out_of_bounds,
                          "ID3v2 frame data is too small");
                }
                r.skip(1);
                frame_data_length -= 1;
            }
            out.assign(r.read_n(frame_data_length), frame_data_length);

            if ((frame.flags & frame_flag_unsynchronization) ||
                (header.flags & header_flag_unsynchronization)) {
                reverse_unsynchronization(out);
            }
        }
        return frame.id;
    }

    void read_extended_header()
    {
        auto extended_header_length = r.read<uint32,BE>();
        if (header.version == 4) {
            if (extended_header_length < 6) {
                raise(errc::out_of_bounds,
                      "ID3v2.4 extended header is too small");
            }
            extended_header_length -= 4;
        }
        if (extended_header_length > header.size) {
            raise(errc::out_of_bounds,
                  "ID3v2 extended header is too large");
        }
        r.skip(extended_header_length);
    }

    static void reverse_unsynchronization(io::buffer& buf) noexcept
    {
        auto i = 0_sz;
        auto j = 0_sz;

        while ((i + 1) < buf.size()) {
            auto const byte = buf[i++];
            buf[j++] = byte;
            if (AMP_UNLIKELY(byte == 0xff && buf[i] == 0x00)) {
                ++i;
            }
        }

        if (buf.size() > i) {
            buf[j++] = buf[i];
        }
        buf.pop_back(buf.size() - j);
    }

    id3v2::header header;
    io::buffer tag_buf;
    io::reader r;
    uint8 frame_header_size;
};


void read_tipl_frame(uint32 const id, io::reader r, media::dictionary& tags)
{
    auto const enc = read_string_encoding(r);
    while (auto key = read_string(r, enc)) {
        if (id == "TMCL"_4cc) {
            key = u8format("performer:%s", key.c_str());
        }
        else if (stricmpeq(key, "engineer")) {
            key = u8string::from_utf8_unchecked(tags::engineer);
        }
        else if (stricmpeq(key, "producer")) {
            key = u8string::from_utf8_unchecked(tags::producer);
        }
        else if (stricmpeq(key, "mix")) {
            key = u8string::from_utf8_unchecked(tags::mixer);
        }
        else {
            key = u8format("involved:%s", key.c_str());
        }
        tags.emplace(std::move(key), read_string(r, enc));
    }
}

void read_text_frame(uint32 const id, io::reader r, media::dictionary& tags)
{
    auto const enc = read_string_encoding(r);
    u8string key;

    if (id == "TXXX"_4cc) {
        key = tags::map_common_key(read_string(r, enc));
    }
    else {
        auto found = id3v2::text_frame_map.find(id);
        if (found != id3v2::text_frame_map.cend()) {
            key = u8string::from_utf8_unchecked(found->second);
        }
        else {
            return;
        }
    }

    while (auto v = read_string(r, enc)) {
        if (id == "TCON"_4cc) {
            auto const s = v.c_str() + (v[0] == '(');
            uint8 index;
            if (std::sscanf(s, "%" SCNu8, &index) == 1) {
                v = id3v1::get_genre_name(index);
            }
        }
        tags.emplace(key, std::move(v));
    }
}

void read_url_frame(uint32 const id, io::reader r, media::dictionary& tags)
{
    auto found = id3v2::url_frame_map.find(id);
    if (found != id3v2::url_frame_map.cend()) {
        tags.emplace(found->second, read_string(r));
    }
}

void read_comm_frame(io::reader r, media::dictionary& tags)
{
    auto const enc = read_string_encoding(r);
    r.skip(3);
    auto key = read_string(r, enc);

    if (key.empty() || stricmpeq(key, "comment")) {
        key = u8string::from_utf8_unchecked(tags::comment);
    }
    else {
        key = u8format("comment:%s", key.c_str());
    }
    tags.emplace(std::move(key), read_string(r, enc));
}

void read_uslt_frame(io::reader r, media::dictionary& tags)
{
    auto const enc = read_string_encoding(r);
    r.skip(3);
    auto key = read_string(r, enc);

    if (key.empty() || stricmpeq(key, "lyrics")) {
        key = u8string::from_utf8_unchecked(tags::lyrics);
    }
    else {
        key = u8format("lyrics:%s", key.c_str());
    }
    tags.emplace(std::move(key), read_string(r, enc));
}

bool read_apic_frame(id3v2::header const& header, io::buffer& data,
                     media::image::type const type, media::image& dest)
{
    io::reader r{data};
    auto const enc = read_string_encoding(r);
    if (header.version >= 3) {
        dest.set_mime_type(read_string(r));
    }
    else {
        auto const image_format = load_uint24BE(r.read_n(3));
        auto const mime_type = (image_format == "\0JPG"_4cc) ? "image/jpeg"
                             : (image_format == "\0PNG"_4cc) ? "image/png"
                             : nullptr;
        if (mime_type) {
            dest.set_mime_type(u8string::from_utf8_unchecked(mime_type));
        }
    }

    auto const apic_type = r.read<uint8>();
    if (apic_type != as_underlying(type)) {
        if (type != media::image::type::front_cover || apic_type != 0) {
            return false;
        }
    }

    dest.set_description(read_string(r, enc));
    if (r.remain() == 0) {
        return false;
    }

    data.pop_front(r.tell());
    dest.set_data(std::move(data));
    return true;
}

}     // namespace <unnamed>


bool skip(io::stream& file)
{
    if (auto const header = id3v2::header::read(file)) {
        file.skip(header->size);
        return true;
    }

    file.seek(-10, io::seekdir::cur);
    return false;
}

void read(io::stream& file, media::dictionary& dict)
{
    if (auto const header = id3v2::header::read(file)) {
        id3v2::frame_parser parse{file, *header};
        io::buffer data;

        while (auto id = parse(data)) {
            switch (id) {
            case "COMM"_4cc:
                id3v2::read_comm_frame(data, dict);
                break;
            case "USLT"_4cc:
                id3v2::read_uslt_frame(data, dict);
                break;
            case "TIPL"_4cc:
            case "TMCL"_4cc:
                id3v2::read_tipl_frame(id, data, dict);
                break;
            default:
                if ((id >> 24) == 'T') {
                    id3v2::read_text_frame(id, data, dict);
                }
                else if ((id >> 24) == 'W') {
                    id3v2::read_url_frame(id, data, dict);
                }
            }
        }

        dict.emplace(tags::tag_type, u8format("ID3v2.%u", header->version));
    }
}

media::image find_image(io::stream& file, media::image::type const type)
{
    media::image image;

    if (auto const header = id3v2::header::read(file)) {
        id3v2::frame_parser parse{file, *header};
        io::buffer data;

        while (auto id = parse(data)) {
            if (id == "APIC"_4cc) {
                if (id3v2::read_apic_frame(*header, data, type, image)) {
                    break;
                }
            }
        }
    }
    return image;
}

}}    // namespace amp::id3v2

