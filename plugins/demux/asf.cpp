////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/asf.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/decoder.hpp>
#include <amp/audio/demuxer.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/input.hpp>
#include <amp/bitops.hpp>
#include <amp/cxp/map.hpp>
#include <amp/cxp/string.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/memory.hpp>
#include <amp/io/reader.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/numeric.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/string.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include "wave.hpp"

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <utility>
#include <vector>


namespace amp {
namespace asf {
namespace {

using namespace wave::literals;
using wave::guid;

// ---------------------------------------------------------------------------
// Top-level object GUIDs
// ---------------------------------------------------------------------------
constexpr auto guid_header_object
    = "3026b275-8e66-cf11-a6d9-00aa0062ce6c"_guid;
constexpr auto guid_data_object
    = "3626b275-8e66-cf11-a6d9-00aa0062ce6c"_guid;
//constexpr auto guid_simple_index_object
//    = "90080033-b1e5-cf11-89f4-00a0c90349cb"_guid;
//constexpr auto guid_index_object
//    = "d329e2d6-da35-d111-9034-00a0c90349be"_guid;
//constexpr auto guid_media_object_index_object
//    = "f803b1fe-ad12-644c-840f-2a1d2f7ad48c"_guid;
//constexpr auto guid_timecode_index_object
//    = "d03fb73c-4a0c-0348-953d-edf7b6228f0c"_guid;

// ---------------------------------------------------------------------------
// Header object GUIDs
// ---------------------------------------------------------------------------
constexpr auto guid_file_properties_object
    = "a1dcab8c-47a9-cf11-8ee4-00c00c205365"_guid;
constexpr auto guid_stream_properties_object
    = "9107dcb7-b7a9-cf11-8ee6-00c00c205365"_guid;
constexpr auto guid_header_extension_object
    = "b503bf5f-2ea9-cf11-8ee3-00c00c205365"_guid;
//constexpr auto guid_codec_list_object
//    = "4052d186-1d31-d011-a3a4-00a0c90348f6"_guid;
constexpr auto guid_marker_object
    = "01cd87f4-51a9-cf11-8ee6-00c00c205365"_guid;
constexpr auto guid_content_description_object
    = "3326b275-8e66-cf11-a6d9-00aa0062ce6c"_guid;
constexpr auto guid_extended_content_description_object
    = "40a4d0d2-07e3-d211-97f0-00a0c95ea850"_guid;
constexpr auto guid_content_encryption_object
    = "fbb31122-23bd-d211-b4b7-00a0c955fc6e"_guid;
constexpr auto guid_extended_content_encryption_object
    = "14e68a29-2226-174c-b935-dae07ee9289c"_guid;
constexpr auto guid_advanced_content_encryption_object
    = "b69b077a-a4da-124e-a5ca-91d38dc11a8d"_guid;

// ---------------------------------------------------------------------------
// Header extension object GUIDs
// ---------------------------------------------------------------------------
constexpr auto guid_extended_stream_properties_object
    = "cba5e614-72c6-3243-8399-a96952065b5a"_guid;
constexpr auto guid_metadata_object
    = "eacbf8c5-af5b-7748-8467-aa8c44fa4cca"_guid;
constexpr auto guid_metadata_library_object
    = "941c2344-9894-d149-a141-1d134e457054"_guid;

// ---------------------------------------------------------------------------
// Stream properties object GUIDs
// ---------------------------------------------------------------------------
constexpr auto guid_audio_media
    = "409e69f8-4d5b-cf11-a8fd-00805f5c442b"_guid;
//constexpr auto guid_video_media
//    = "c0ef19bc-4d5b-cf11-a8fd-00805f5c442b"_guid;
//constexpr auto guid_no_error_correction
//    = "0057fb20-555b-cf11-a8fd-00805f5c442b"_guid;
constexpr auto guid_audio_spread
    = "50cdc3bf-8f61-cf11-8bb2-00aa00b4e220"_guid;


using std::string_view;

constexpr cxp::map<string_view, string_view, 45, stricmp_less> key_mapping {{
    { "MusicBrainz/Album Artist Id",  tags::mb_album_artist_id  },
    { "MusicBrainz/Album Id",         tags::mb_album_id         },
    { "MusicBrainz/Artist Id",        tags::mb_artist_id        },
    { "MusicBrainz/Disc Id",          tags::mb_disc_id          },
    { "MusicBrainz/Release Country",  tags::mb_release_country  },
    { "MusicBrainz/Release Group Id", tags::mb_release_group_id },
    { "MusicBrainz/Track Id",         tags::mb_track_id         },
    { "WM/AlbumArtist",               tags::album_artist        },
    { "WM/AlbumArtistSortOrder",      tags::album_artist_sort   },
    { "WM/AlbumSortOrder",            tags::album_sort          },
    { "WM/AlbumTitle",                tags::album               },
    { "WM/ArtistSortOrder",           tags::artist_sort         },
    { "WM/Barcode",                   tags::barcode             },
    { "WM/BeatsPerMinute",            tags::bpm                 },
    { "WM/CatalogNo",                 tags::catalog_number      },
    { "WM/Comments",                  tags::comment             },
    { "WM/Compilation",               tags::compilation         },
    { "WM/Composer",                  tags::composer            },
    { "WM/ComposerSortOrder",         tags::composer_sort       },
    { "WM/Conductor",                 tags::conductor           },
    { "WM/ContentGroupDescription",   tags::group               },
    { "WM/Copyright",                 tags::copyright           },
    { "WM/EncodedBy",                 tags::encoded_by          },
    { "WM/EncodingSettings",          tags::encoding_settings   },
    { "WM/EncodingTime",              tags::encoding_time       },
    { "WM/Genre",                     tags::genre               },
    { "WM/ISRC",                      tags::isrc                },
    { "WM/Lyrics",                    tags::lyrics              },
    { "WM/Mixer",                     tags::mixer               },
    { "WM/ModifiedBy",                tags::remixer             },
    { "WM/Mood",                      tags::mood                },
    { "WM/OriginalAlbumTitle",        tags::original_album      },
    { "WM/OriginalArtist",            tags::original_artist     },
    { "WM/OriginalFilename",          tags::original_filename   },
    { "WM/OriginalLyricist",          tags::original_lyricist   },
    { "WM/PartOfSet",                 tags::disc_number         },
    { "WM/Producer",                  tags::producer            },
    { "WM/Publisher",                 tags::label               },
    { "WM/RadioStationName",          tags::radio_station       },
    { "WM/RadioStationOwner",         tags::radio_station_owner },
    { "WM/SharedUserRating",          tags::rating              },
    { "WM/TitleSortOrder",            tags::title_sort          },
    { "WM/TrackNumber",               tags::track_number        },
    { "WM/Writer",                    tags::lyricist            },
    { "WM/Year",                      tags::date                },
}};
static_assert(cxp::is_sorted(asf::key_mapping), "");

inline u8string to_media_key(u8string key)
{
    auto found = asf::key_mapping.find(key);
    if (found != asf::key_mapping.end()) {
        key = u8string::from_utf8_unchecked(found->second);
    }
    else {
        key = tags::map_common_key(key);
    }
    return key;
}


u8string load_string(void const* const buf, std::size_t const bytes)
{
    auto const str = static_cast<char16 const*>(buf);
    return u8string::from_utf16le(str, cxp::strlen(str, bytes / 2));
}

u8string read_string(io::stream& file, std::size_t bytes, io::buffer& tmp)
{
    bytes = align_up(bytes, 2);
    if (bytes != 0) {
        tmp.assign(file, bytes);
        return asf::load_string(tmp.data(), bytes);
    }
    return {};
}


struct object
{
    guid   id;
    uint64 size;

    static asf::object read(io::stream& file)
    {
        asf::object object;
        file.gather<LE>(object.id, object.size);

        if (AMP_UNLIKELY(object.size < 24)) {
            raise(errc::invalid_data_format,
                  "ASF object size must be at least 24 bytes");
        }
        return object;
    }
};

struct header_object
{
    explicit header_object(io::stream& file)
    {
        asf::object object;
        uint8 reserved[2];
        file.gather<LE>(object.id, object.size, subobject_count, reserved);

        if (object.id != asf::guid_header_object || reserved[1] != 0x02) {
            raise(errc::invalid_data_format, "invalid ASF header object");
        }
    }

    uint32 subobject_count;
};

struct file_properties_object
{
    guid   file_id;
    uint64 file_size;
    uint64 creation_date;
    uint64 packet_count;
    uint64 play_duration;
    uint64 send_duration;
    uint64 preroll;
    uint32 flags;
    uint32 min_packet_size;
    uint32 max_packet_size;
    uint32 max_bit_rate;

    void read(io::stream& file, uint64 const object_size)
    {
        if (object_size < 104) {
            raise(errc::invalid_data_format,
                  "ASF File Properties Object is too small");
        }

        file.gather<LE>(file_id,
                        file_size,
                        creation_date,
                        packet_count,
                        play_duration,
                        send_duration,
                        preroll,
                        flags,
                        min_packet_size,
                        max_packet_size,
                        max_bit_rate);
    }
};

struct content_description_object
{
    void read(io::stream& file)
    {
        uint16 lengths[5];
        file.gather<LE>(lengths);

        io::buffer tmp;
        title       = asf::read_string(file, lengths[0], tmp);
        artist      = asf::read_string(file, lengths[1], tmp);
        copyright   = asf::read_string(file, lengths[2], tmp);
        description = asf::read_string(file, lengths[3], tmp);
        rating      = asf::read_string(file, lengths[4], tmp);
    }

    u8string title;
    u8string artist;
    u8string copyright;
    u8string description;
    u8string rating;
};

struct marker_object
{
    struct entry_type
    {
        uint64   byte_offset;
        uint64   presentation_time;
        uint32   send_time;
        uint32   flags;
        u8string description;
    };

    void parse(io::stream& file)
    {
        uint32 entry_count;
        uint16 name_length;

        file.gather<LE>(io::ignore<16>,         // reserved1
                        entry_count,
                        io::ignore<2>,          // reserved2
                        name_length);

        io::buffer tmp;
        name = asf::read_string(file, name_length, tmp);

        entries.resize(entry_count);
        for (auto&& entry : entries) {
            uint16 entry_length;
            uint32 descr_length;

            file.gather<LE>(entry.byte_offset,
                            entry.presentation_time,
                            entry_length,
                            entry.send_time,
                            entry.flags,
                            descr_length);

            entry.description = asf::read_string(file, descr_length * 2, tmp);
        }
    }

    u8string                name;
    std::vector<entry_type> entries;
};


struct attribute
{
    static constexpr auto unicode = uint16{0};
    static constexpr auto bytes   = uint16{1};
    static constexpr auto boolean = uint16{2};
    static constexpr auto dword   = uint16{3};
    static constexpr auto qword   = uint16{4};
    static constexpr auto word    = uint16{5};
    static constexpr auto guid    = uint16{6};

    u8string   name;
    io::buffer data;
    uint16     type;
    uint16     stream_number;

    u8string to_string() const
    {
        if (type == attribute::unicode) {
            return asf::load_string(data.data(), data.size());
        }
        if (type == attribute::boolean) {
            auto const b = !data.empty() && (data[0] != 0);
            return u8string::from_utf8_unchecked(b ? "Yes" : "No");
        }
        if (type >= attribute::dword && type <= attribute::word) {
            io::reader r(data);
            auto const x = (type == attribute::qword) ? r.read<uint64,LE>()
                         : (type == attribute::dword) ? r.read<uint32,LE>()
                         :                              r.read<uint16,LE>();
            return to_u8string(x);
        }
        if (type == attribute::guid || type == attribute::bytes) {
            return {};
        }
        AMP_UNREACHABLE();
    }
};


void read_metadata_object(io::stream& file,
                          std::vector<attribute>& dest)
{
    auto n = file.read<uint16,LE>();
    dest.reserve(dest.size() + n);

    io::buffer tmp;
    while (n-- != 0) {
        attribute attr;
        uint16 name_length;
        uint32 data_length;

        file.gather<LE>(io::ignore<2>,      // language_list_index
                        attr.stream_number,
                        name_length,
                        attr.type,
                        data_length);

        attr.name = asf::read_string(file, name_length, tmp);
        attr.data.assign(file, data_length);
        dest.push_back(std::move(attr));
    }
}

void read_extended_content_description_object(io::stream& file,
                                              std::vector<attribute>& dest)
{
    auto n = file.read<uint16,LE>();
    dest.reserve(dest.size() + n);

    io::buffer tmp;
    while (n-- != 0) {
        attribute attr;
        attr.stream_number = 0;
        attr.name = asf::read_string(file, file.read<uint16,LE>(), tmp);

        uint16 data_length;
        file.gather<LE>(attr.type, data_length);
        attr.data.assign(file, data_length);
        dest.push_back(std::move(attr));
    }
}


class descrambler
{
public:
    void parse(io::reader r)
    {
        r.gather<LE>(io::alias<uint8>(span),
                     io::alias<uint16>(virtual_packet_length),
                     io::alias<uint16>(virtual_chunk_length));

        if (span > 1) {
            if ((virtual_chunk_length == 0)                         ||
                (virtual_packet_length / virtual_chunk_length <= 1) ||
                (virtual_packet_length % virtual_chunk_length != 0)) {
                span = 0;
            }
            else {
                tmp.resize(virtual_packet_length * span);
            }
        }
    }

    void operator()(io::buffer& pkt)
    {
        if (span <= 1) {
            return;
        }

        if (AMP_UNLIKELY(pkt.size() != tmp.size())) {
            raise(errc::invalid_argument, "invalid packet size");
        }

        auto const n = virtual_chunk_length;
        auto const h = virtual_packet_length / n;
        auto const w = span;

        for (auto const i : xrange(h)) {
            for (auto const j : xrange(w)) {
                std::copy_n(&pkt[n * (i + j*h)], n,
                            &tmp[n * (j + i*w)]);
            }
        }
        tmp.swap(pkt);
    }

private:
    io::buffer tmp;
    uint32     virtual_packet_length{};
    uint32     virtual_chunk_length{};
    uint32     span{};
};


struct payload_parsing_info
{
    uint8  length_type_flags;
    uint8  property_flags;
    uint32 packet_length;
    uint32 sequence;
    uint32 padding_length;
    uint32 send_time;
    uint16 duration;
    uint8  payload_flags;
};

struct payload_data_header
{
    uint8  stream_number;
    uint32 media_object_number;
    uint32 offset_into_media_object;
    uint32 replicated_data_length;
};


constexpr uint32 coded_size(uint32 const flags, uint32 const offset) noexcept
{
    return (uint32{1} << ((flags >> offset) & 0x3)) >> 1;
}

inline uint32 read_coded(io::reader& r, uint32 const size) noexcept
{
    AMP_ASSERT(r.remain() >= size);
    AMP_ASSERT(size <= 4);
    AMP_ASSUME(size <= 4);

    auto const v = io::load<uint32,LE>(r.read_n_unchecked(size));
    auto const m = uint64{0xffffffff} >> ((4 - size) * 8);
    return static_cast<uint32>(v & m);
}


struct stream
{
    io::buffer ts_data;
    io::buffer ec_data;
    uint64     start_time{};
    uint64     end_time{};
    uint32     bit_rate{};
    bool       is_audio{};
    bool       has_spread_ec{};
};


class demuxer final :
    public audio::basic_demuxer<asf::demuxer>
{
    using Base = audio::basic_demuxer<asf::demuxer>;
    friend class audio::basic_demuxer<asf::demuxer>;

public:
    explicit demuxer(ref_ptr<io::stream>, audio::open_mode);

    void seek(uint64);

    auto get_info(uint32);
    auto get_image(media::image::type);
    auto get_chapter_count() const noexcept;

private:
    void find_first_audio_stream(asf::stream(&)[128]);
    void parse_stream_properties(asf::stream(&)[128]);
    void parse_extended_stream_properties(asf::stream(&)[128]);

    bool feed(io::buffer&);

    auto read_payload_parsing_info();
    auto read_payload_data_header(asf::payload_parsing_info const&);
    auto read_payload_length(asf::payload_parsing_info const&);

    void demux_payloads(asf::payload_parsing_info const&, uint64);
    bool is_beginning_of_packet(asf::payload_parsing_info const&);

    ref_ptr<io::stream>             file;
    asf::file_properties_object     file_properties;
    asf::content_description_object content_description;
    asf::marker_object              marker;
    asf::descrambler                descramble;
    uint64                          data_start_offset{};
    uint64                          data_length{};
    std::vector<asf::attribute>     attributes;
    std::vector<io::buffer>         packet_queue;
    io::buffer                      packet_buffer;
    uint64                          packet_number{};
    uint32                          packet_offset{};
    uint8                           audio_stream_number{};
};


void demuxer::find_first_audio_stream(asf::stream(&streams)[128])
{
    AMP_ASSERT(audio_stream_number == 0);

    for (auto const i : xrange<uint8>(1, 128)) {
        auto&& stream = streams[i];
        if (stream.is_audio &&
            Base::try_resolve_decoder(wave::parse_format(stream.ts_data))) {
            audio_stream_number = i;
            goto found;
        }
    }
    raise(errc::failure, "no audio stream(s) found in ASF file");

found:
    auto&& stream = streams[audio_stream_number];
    if (stream.has_spread_ec) {
        descramble.parse(stream.ec_data);
    }

    auto start_time = stream.start_time;
    auto total_time = stream.end_time;
    if (total_time == 0) {
        total_time  = file_properties.play_duration;
        total_time -= file_properties.preroll * (10000000 / 1000);
    }
    total_time -= start_time;

    start_time = muldiv(start_time, format.sample_rate, 10000000);
    total_time = muldiv(total_time, format.sample_rate, 10000000);

    Base::set_total_frames(total_time);
    Base::set_encoder_delay(numeric_cast<uint32>(start_time));

    average_bit_rate = stream.bit_rate;
    if (average_bit_rate == 0) {
        average_bit_rate = format.bit_rate;
    }
    if (average_bit_rate == 0) {
        average_bit_rate = static_cast<uint32>(muldiv(data_length,
                                                      format.sample_rate * 8,
                                                      total_time));
    }
}

void demuxer::parse_stream_properties(asf::stream(&streams)[128])
{
    guid stream_type, ec_type;
    uint32 ts_data_length, ec_data_length;
    uint16 flags;

    file->gather<LE>(stream_type,
                     ec_type,
                     io::ignore<8>,     // time_offset
                     ts_data_length,
                     ec_data_length,
                     flags,
                     io::ignore<4>);    // reserved

    auto&& s = streams[flags & 0x7f];
    s.ts_data.assign(*file, ts_data_length);
    s.ec_data.assign(*file, ec_data_length);
    s.is_audio = (stream_type == asf::guid_audio_media);
    s.has_spread_ec = (ec_type == asf::guid_audio_spread);
}

void demuxer::parse_extended_stream_properties(asf::stream(&streams)[128])
{
    uint64 start_time, end_time;
    uint32 data_bit_rate;
    uint16 stream_number;

    file->gather<LE>(start_time,
                     end_time,
                     data_bit_rate,
                     io::ignore<4>,     // buffer_size
                     io::ignore<4>,     // initial_buffer_fullness
                     io::ignore<4>,     // alternate_data_bit_rate
                     io::ignore<4>,     // alternate_buffer_size
                     io::ignore<4>,     // alternate_initial_buffer_fullness
                     io::ignore<4>,     // maximum_object_size
                     io::ignore<4>,     // flags
                     stream_number,
                     io::ignore<2>,     // stream_language_id_index
                     io::ignore<8>,     // average_time_per_frame
                     io::ignore<2>,     // stream_name_count
                     io::ignore<2>);    // payload_extension_system_count

    auto&& s = streams[stream_number & 0x7f];
    s.start_time = start_time;
    s.end_time   = end_time;
    s.bit_rate   = data_bit_rate;
}

demuxer::demuxer(ref_ptr<io::stream> s, audio::open_mode const mode) :
    file{std::move(s)}
{
    asf::header_object header{*file};
    asf::stream streams[128];

    auto const file_length = file->size();
    auto       file_offset = file->tell();

    while ((file_offset + 24) < file_length) {
        auto const object = asf::object::read(*file);
        if (object.id == asf::guid_header_extension_object) {
            file_offset += 46;
            file->seek(file_offset);
            continue;
        }
        else if (object.id == asf::guid_file_properties_object) {
            file_properties.read(*file, object.size);
        }
        else if (object.id == asf::guid_stream_properties_object) {
            parse_stream_properties(streams);
        }
        else if (object.id == asf::guid_extended_stream_properties_object) {
            parse_extended_stream_properties(streams);
        }
        else if (object.id == asf::guid_data_object) {
            data_start_offset = file_offset + 50;
            data_length       = object.size - 50;
        }
        else if (object.id == asf::guid_marker_object) {
            if (mode & audio::metadata) {
                marker.parse(*file);
            }
        }
        else if (object.id == asf::guid_content_description_object) {
            if (mode & audio::metadata) {
                content_description.read(*file);
            }
        }
        else if (object.id == asf::guid_extended_content_description_object) {
            if (mode & (audio::metadata | audio::pictures)) {
                read_extended_content_description_object(*file, attributes);
            }
        }
        else if (object.id == asf::guid_metadata_object ||
                 object.id == asf::guid_metadata_library_object) {
            if (mode & (audio::metadata | audio::pictures)) {
                read_metadata_object(*file, attributes);
            }
        }
        else if (object.id == asf::guid_advanced_content_encryption_object ||
                 object.id == asf::guid_extended_content_encryption_object ||
                 object.id == asf::guid_content_encryption_object) {
            raise(errc::not_implemented,
                  "ASF file contains DRM-protected content");
        }

        file_offset += object.size;
        file->seek(file_offset);
    }

    if (mode & (audio::playback | audio::metadata)) {
        find_first_audio_stream(streams);
        if (mode & audio::playback) {
            file->seek(data_start_offset);
        }
        if (mode & audio::metadata) {
            auto const preroll = file_properties.preroll * (10000000 / 1000);
            for (auto&& entry : marker.entries) {
                entry.presentation_time -= preroll;
            }
        }
    }
}

auto demuxer::read_payload_parsing_info()
{
    asf::payload_parsing_info info;
    info.length_type_flags = file->read<uint8>();

    if (info.length_type_flags & 0x80) {
        file->skip(info.length_type_flags & 0xf);
        info.length_type_flags = file->read<uint8>();
    }

    auto const size0 = asf::coded_size(info.length_type_flags, 5);
    auto const size1 = asf::coded_size(info.length_type_flags, 1);
    auto const size2 = asf::coded_size(info.length_type_flags, 3);
    auto const size3 = uint32{info.length_type_flags} & 0x1;

    uint8 buf[1 + 4 + 4 + 4 + 4 + 2 + 1];
    file->read(buf, 1 + size0 + size1 + size2 + 4 + 2 + size3);
    io::reader r{buf};

    info.property_flags = r.read_unchecked<uint8>();
    info.packet_length  = asf::read_coded(r, size0);
    info.sequence       = asf::read_coded(r, size1);
    info.padding_length = asf::read_coded(r, size2);
    info.send_time      = r.read_unchecked<uint32,LE>();
    info.duration       = r.read_unchecked<uint16,LE>();
    info.payload_flags  = (size3 ? r.read_unchecked<uint8>() : uint8{0});

    auto const min_packet_size = file_properties.min_packet_size;
    auto const max_packet_size = file_properties.max_packet_size;

    if (info.packet_length == 0) {
        info.packet_length = min_packet_size;
    }
    else if (info.packet_length < min_packet_size) {
        info.padding_length += min_packet_size - info.packet_length;
        info.packet_length   = min_packet_size;
    }

    if (info.packet_length < info.padding_length) {
        raise(errc::out_of_bounds,
              "ASF packet length (%" PRIu32 " bytes) exceeds "
              "padding length (%" PRIu32 " bytes)",
              info.packet_length, info.padding_length);
    }
    if (info.packet_length > max_packet_size) {
        raise(errc::out_of_bounds,
              "ASF packet length (%" PRIu32 " bytes) exceeds "
              "max packet length (%" PRIu32 " bytes)",
              info.packet_length, max_packet_size);
    }
    return info;
}

auto demuxer::read_payload_data_header(asf::payload_parsing_info const& info)
{
    auto const size0 = asf::coded_size(info.property_flags, 4);
    auto const size1 = asf::coded_size(info.property_flags, 2);
    auto const size2 = asf::coded_size(info.property_flags, 0);

    uint8 buf[1 + 4 + 4 + 4];
    file->read(buf, 1 + size0 + size1 + size2);
    io::reader r{buf};

    payload_data_header head;
    head.stream_number = static_cast<uint8>(r.read_unchecked<uint8>() & 0x7f);
    head.media_object_number      = asf::read_coded(r, size0);
    head.offset_into_media_object = asf::read_coded(r, size1);
    head.replicated_data_length   = asf::read_coded(r, size2);
    return head;
}

auto demuxer::read_payload_length(asf::payload_parsing_info const& info)
{
    auto const size0 = asf::coded_size(info.payload_flags, 6);
    uint8 buf[4];
    file->read(buf, size0);

    io::reader r{buf};
    return asf::read_coded(r, size0);
}

void demuxer::demux_payloads(asf::payload_parsing_info const& info,
                             uint64 const packet_start_offset)
{
    auto const packet_end_offset = packet_start_offset + info.packet_length;
    auto const multiple_payloads = (info.length_type_flags & 0x1);
    auto payload_count = multiple_payloads ? (info.payload_flags & 0x3f) : 1;

    while (payload_count-- != 0) {
        auto const head = read_payload_data_header(info);

        uint32 media_object_size{};
        uint32 presentation_time{};
        uint8  presentation_time_delta{};

        if (head.replicated_data_length >= 8) {
            file->gather<LE>(media_object_size,
                             presentation_time);

            if (head.replicated_data_length > 8) {
                file->skip(head.replicated_data_length - 8);
            }
        }
        else if (head.replicated_data_length == 1) {
            presentation_time_delta = file->read<uint8>();
        }
        else if (head.replicated_data_length != 0) {
            raise(errc::failure,
                  "invalid replicated data length: %" PRIu32,
                  head.replicated_data_length);
        }

        uint32 payload_length;
        if (multiple_payloads) {
            payload_length = read_payload_length(info);
        }
        else {
            auto const end  = packet_start_offset + info.packet_length;
            payload_length  = static_cast<uint32>(end - file->tell());
            payload_length -= info.padding_length;
        }

        if (head.stream_number != audio_stream_number) {
            file->skip(payload_length);
            continue;
        }

        if (head.replicated_data_length < 8) {
            media_object_size = payload_length;
        }

        if (packet_offset != head.offset_into_media_object) {
            raise(errc::failure, "invalid ASF media object offset");
        }
        if ((packet_offset + payload_length) > media_object_size) {
            raise(errc::out_of_bounds, "oversized ASF media object payload");
        }

        if (packet_buffer.empty()) {
            packet_buffer.resize(media_object_size, uninitialized);
        }

        file->read(&packet_buffer[packet_offset], payload_length);
        packet_offset += payload_length;

        if (packet_offset == packet_buffer.size()) {
            packet_offset = 0;
            packet_queue.push_back(std::move(packet_buffer));
            if (info.duration != 0) {
                instant_bit_rate = muldiv(media_object_size,
                                          uint32{1000} * 8,
                                          uint32{info.duration});
            }
            else {
                instant_bit_rate = average_bit_rate;
            }
        }
    }

    auto const file_offset = file->tell();
    if (file_offset != packet_end_offset) {
        if (file_offset > packet_end_offset) {
            raise(errc::failure, "ASF: read outside of packet boundaries");
        }
        file->seek(packet_end_offset);
    }

    if (multiple_payloads) {
        std::reverse(packet_queue.begin(), packet_queue.end());
    }
}

bool demuxer::is_beginning_of_packet(asf::payload_parsing_info const& info)
{
    auto const multiple_payloads = (info.length_type_flags & 0x1);
    auto payload_count = multiple_payloads ? (info.payload_flags & 0x3f) : 1;

    while (payload_count-- != 0) {
        auto const head = read_payload_data_header(info);
        if (head.stream_number == audio_stream_number) {
            return (head.offset_into_media_object == 0);
        }

        if (multiple_payloads) {
            file->skip(head.replicated_data_length);
            file->skip(read_payload_length(info));
        }
    }

    return false;
}

bool demuxer::feed(io::buffer& dest)
{
    while (packet_queue.empty()) {
        if (AMP_UNLIKELY(packet_number >= file_properties.packet_count)) {
            return false;
        }

        auto const packet_start_offset = file->tell();
        auto const info = read_payload_parsing_info();
        demux_payloads(info, packet_start_offset);
        ++packet_number;
    }

    packet_queue.back().swap(dest);
    packet_queue.pop_back();

    descramble(dest);
    return true;
}

void demuxer::seek(uint64 const target_pts)
{
    auto const packet_count = file_properties.packet_count;
    AMP_ASSERT(packet_count > 0);

    auto const sample_rate = format.sample_rate;
    AMP_ASSERT(sample_rate > 0);

    auto const frames_per_packet = total_frames / packet_count;
    AMP_ASSERT(frames_per_packet > 0);

    auto const bytes_per_packet = file_properties.min_packet_size;
    AMP_ASSERT(bytes_per_packet > 0);

    uint64 priming{};
    uint64 packet_start_offset{};

    packet_queue.clear();
    packet_buffer.clear();
    packet_offset = 0;
    packet_number = target_pts / frames_per_packet;

    for (;;) {
        if (packet_number >= packet_count) {
            packet_start_offset = data_length;
            priming = 0;
            break;
        }
        if (packet_number == 0) {
            packet_start_offset = 0;
            priming = target_pts;
            break;
        }

        packet_start_offset = packet_number * bytes_per_packet;
        file->seek(data_start_offset + packet_start_offset);

        auto const info = read_payload_parsing_info();
        auto const pts = muldiv(info.send_time, sample_rate, 1000);

        if (target_pts > pts) {
            if (is_beginning_of_packet(info)) {
                priming = target_pts - pts;
                break;
            }
            --packet_number;
            continue;
        }

        auto step = (pts - target_pts) / (frames_per_packet * 2);
        step = std::max(step, uint64{1});

        if (step > packet_number) {
            packet_start_offset = 0;
            priming = target_pts;
            break;
        }
        packet_number -= step;
    }

    file->seek(data_start_offset + packet_start_offset);
    Base::set_seek_target_and_offset(target_pts, priming);
}

auto demuxer::get_info(uint32 const number)
{
    audio::stream_info info{Base::get_format()};
    info.codec_id         = Base::format.codec_id;
    info.bits_per_sample  = Base::format.bits_per_sample;
    info.average_bit_rate = Base::average_bit_rate;
    info.props.emplace(tags::container, "ASF");

    if (content_description.title) {
        info.tags.emplace(tags::title, content_description.title);
    }
    if (content_description.artist) {
        info.tags.emplace(tags::artist, content_description.artist);
    }
    if (content_description.description) {
        info.tags.emplace(tags::comment, content_description.description);
    }
    if (content_description.copyright) {
        info.tags.emplace(tags::copyright, content_description.copyright);
    }
    if (content_description.rating) {
        info.tags.emplace(tags::rating, content_description.rating);
    }

    for (auto const& attr : attributes) {
        if (attr.stream_number != 0 &&
            attr.stream_number != audio_stream_number) {
            continue;
        }

        auto value = attr.to_string();
        if (!attr.name.empty() && !value.empty()) {
            info.tags.emplace(to_media_key(attr.name), std::move(value));
        }
    }

    info.frames = Base::total_frames;
    if (number != 0) {
        AMP_ASSERT(number <= marker.entries.size());
        info.start_offset = muldiv(marker.entries[number - 1].presentation_time,
                                   info.sample_rate, 10000000);

        if (number != marker.entries.size()) {
            info.frames = muldiv(marker.entries[number].presentation_time,
                                 info.sample_rate, 10000000);
        }
        info.frames -= info.start_offset;

        if (auto&& title = marker.entries[number - 1].description) {
            info.tags.insert_or_assign(tags::title, title);
        }
    }
    return info;
}

auto demuxer::get_image(media::image::type const type)
{
    media::image image;

    for (auto const& attr : attributes) {
        if (attr.stream_number != 0 &&
            attr.stream_number != audio_stream_number) {
            continue;
        }
        if (attr.type != asf::attribute::bytes) {
            continue;
        }
        if (!stricmpeq(attr.name, "WM/Picture")) {
            continue;
        }

        uint8  picture_type;
        uint32 picture_size;

        io::reader r(attr.data);
        r.gather<LE>(picture_type, picture_size);
        if (picture_type != static_cast<uint8>(type)) {
            continue;
        }

        auto read_next_string = [&]{
            for (auto pos = 0_sz; (r.remain() - pos) >= 2; pos += 2) {
                if (io::load<char16>(r.peek() + pos) == u'\0') {
                    return asf::load_string(r.read_n_unchecked(pos + 2), pos);
                }
            }
            r.skip_unchecked(r.remain());
            return u8string{};
        };

        auto mime_type   = read_next_string();
        auto description = read_next_string();

        if (r.remain() != 0) {
            image.set_data(r.peek(), r.remain());
            image.set_mime_type(std::move(mime_type));
            image.set_description(std::move(description));
            break;
        }
    }
    return image;
}

auto demuxer::get_chapter_count() const noexcept
{
    return static_cast<uint32>(marker.entries.size());
}

AMP_REGISTER_INPUT(demuxer, "asf", "wm", "wma", "wmv");

}}}   // namespace amp::asf::<unnamed>

