////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/wave.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/demuxer.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/input.hpp>
#include <amp/audio/pcm.hpp>
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
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include "wave.hpp"

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>


namespace amp {
namespace wave {
namespace {

constexpr cxp::map<uint32, std::string_view, 16> info_keys {{
    { "IART"_4cc, tags::artist       },
    { "ICMT"_4cc, tags::comment      },
    { "ICOP"_4cc, tags::copyright    },
    { "ICRD"_4cc, tags::date         },
    { "IENC"_4cc, tags::encoded_by   },
    { "IFRM"_4cc, tags::disc_number  },
    { "IGNR"_4cc, tags::genre        },
    { "INAM"_4cc, tags::title        },
    { "IPRD"_4cc, tags::album        },
    { "IPRT"_4cc, tags::track_number },
    { "ISBJ"_4cc, tags::album_artist },
    { "ISFT"_4cc, tags::encoder      },
    { "ISRF"_4cc, tags::group        },
    { "ITRK"_4cc, tags::track_number },
    { "TRCK"_4cc, tags::track_number },
    { "itrk"_4cc, tags::track_number },
}};
static_assert(cxp::is_sorted(info_keys), "");

constexpr cxp::map<uint16, uint32, 55> format_tags {{
    { 0x0001, audio::codec::lpcm              },
    { 0x0002, audio::codec::adpcm_ms          },
    { 0x0003, audio::codec::lpcm              },
    { 0x0006, audio::codec::alaw              },
    { 0x0007, audio::codec::ulaw              },
    { 0x0008, audio::codec::dts               },
    { 0x000a, audio::codec::wma_voice         },
    { 0x000b, audio::codec::wma_voice         },
    { 0x0010, audio::codec::adpcm_ima_oki     },
    { 0x0011, audio::codec::adpcm_ima_ms      },
    { 0x0017, audio::codec::adpcm_ima_oki     },
    { 0x0020, audio::codec::adpcm_yamaha      },
    { 0x0022, audio::codec::truespeech        },
    { 0x0031, audio::codec::gsm_ms            },
    { 0x0032, audio::codec::gsm_ms            },
    { 0x0038, audio::codec::amr_nb            },
    { 0x0040, audio::codec::adpcm_g726        },
    { 0x0042, audio::codec::g723_1            },
    { 0x0045, audio::codec::adpcm_g726        },
    { 0x0050, audio::codec::mpeg_layer2       },
    { 0x0055, audio::codec::mpeg_layer3       },
    { 0x0057, audio::codec::amr_nb            },
    { 0x0058, audio::codec::amr_wb            },
    { 0x0061, audio::codec::adpcm_ima_dk4     },
    { 0x0062, audio::codec::adpcm_ima_dk3     },
    { 0x0064, audio::codec::adpcm_g726        },
    { 0x0065, audio::codec::adpcm_g722        },
    { 0x0069, audio::codec::adpcm_ima_ms      },
    { 0x00ff, audio::codec::aac_lc            },
    { 0x0111, audio::codec::g723_1            },
    { 0x0130, audio::codec::sipr              },
    { 0x0160, audio::codec::wma_v1            },
    { 0x0161, audio::codec::wma_v2            },
    { 0x0162, audio::codec::wma_pro           },
    { 0x0163, audio::codec::wma_lossless      },
    { 0x0200, audio::codec::adpcm_creative    },
    { 0x0270, audio::codec::atrac3            },
    { 0x028f, audio::codec::adpcm_g722        },
    { 0x0401, audio::codec::intel_music_coder },
    { 0x0402, audio::codec::indeo_audio       },
    { 0x1500, audio::codec::gsm_ms            },
    { 0x1501, audio::codec::truespeech        },
    { 0x2000, audio::codec::ac3               },
    { 0x2001, audio::codec::dts               },
    { 0x5346, audio::codec::adpcm_swf         },
    { 0x594a, audio::codec::dpcm_xan          },
    { 0x6c75, audio::codec::ulaw              },
    { 0x706d, audio::codec::aac_lc            },
    { 0x7361, audio::codec::amr_nb            },
    { 0x7362, audio::codec::amr_wb            },
    { 0x7363, audio::codec::amr_wb_plus       },
    { 0xa100, audio::codec::g723_1            },
    { 0xa106, audio::codec::aac_lc            },
    { 0xa109, audio::codec::speex             },
    { 0xf1ac, audio::codec::flac              },
}};
static_assert(cxp::is_sorted(format_tags), "");

namespace subtype {
    constexpr auto base        = "00000000-0000-1000-8000-00aa00389b71"_guid;
    constexpr auto eac3        = "af87fba7-022d-fb42-a4d4-05cd93843bdd"_guid;
    constexpr auto mpeg_layer1 = "2b806de0-46db-cf11-b4d1-00805f6cbbea"_guid;
    constexpr auto ac3         = "2c806de0-46db-cf11-b4d1-00805f6cbbea"_guid;
    constexpr auto atrac3_plus = "bfaa23e9-58cb-7144-a119-fffa01e4ce62"_guid;
}

struct waveformatextensible
{
    uint16 format_tag;
    uint16 channels;
    uint32 sample_rate;
    uint32 byte_rate;
    uint16 block_align;
    uint16 bits_per_sample;
    uint16 extra_size;
    uint16 valid_bits_per_sample;
    uint32 channel_mask;
    guid sub_format;
};

inline uint16 get_format_tag(guid sub_format) noexcept
{
    if (std::memcmp(&sub_format.data[4], &subtype::base.data[4], 12) == 0) {
        return io::load<uint16,LE>(&sub_format.data[0]);
    }
    return uint16{0xfffe};
}

void get_codec_id_and_flags(waveformatextensible const& wfx,
                            audio::codec_format& out) noexcept
{
    auto format_tag = wfx.format_tag;
    if (format_tag == 0xfffe) {
        format_tag = get_format_tag(wfx.sub_format);
    }

    if (format_tag == 0xfffe) {
        if (wfx.sub_format == subtype::eac3) {
            out.codec_id = audio::codec::eac3;
        }
        else if (wfx.sub_format == subtype::mpeg_layer1) {
            out.codec_id = audio::codec::mpeg_layer1;
        }
        else if (wfx.sub_format == subtype::ac3) {
            out.codec_id = audio::codec::ac3;
        }
        else if (wfx.sub_format == subtype::atrac3_plus) {
            out.codec_id = audio::codec::atrac3_plus;
        }
        else {
            raise(errc::unsupported_format, "unrecognized WAVE format GUID");
        }
    }
    else {
        auto found = wave::format_tags.find(format_tag);
        if (found != wave::format_tags.end()) {
            out.codec_id = found->second;
        }
        else {
            raise(errc::unsupported_format,
                  "unrecognized WAVE format tag: 0x%04" PRIx16,
                  format_tag);
        }
    }

    out.flags = {};
    if (format_tag == 0x1) {
        if (wfx.bits_per_sample != 8) {
            out.flags |= audio::pcm::signed_int;
        }
    }
    else if (format_tag == 0x3) {
        out.flags |= audio::pcm::ieee_float;
    }
}

uint32 get_frames_per_packet(uint32 const codec_id,
                             waveformatextensible const& wfx) noexcept
{
    switch (codec_id) {
    case audio::codec::lpcm:
    case audio::codec::alaw:
    case audio::codec::ulaw:
        return 1;
    case audio::codec::gsm_ms:
        return 320;
    case audio::codec::atrac3:
        return 1024;
    case audio::codec::atrac3_plus:
        return 2048;
    case audio::codec::truespeech:
        return 240 * (wfx.block_align / 32);
    case audio::codec::g723_1:
        return 240 * (wfx.block_align / 24);
    case audio::codec::adpcm_g722:
    case audio::codec::adpcm_g726:
        return wfx.block_align * 8 / wfx.bits_per_sample;
    case audio::codec::indeo_audio:
    case audio::codec::intel_music_coder:
        return 4 * wfx.block_align / wfx.channels;
    case audio::codec::adpcm_ms:
        return 2 + (wfx.block_align - 7 * wfx.channels) * 2 / wfx.channels;
    case audio::codec::adpcm_ima_ms:
        return 1 + (wfx.block_align - 4 * wfx.channels)
                 / (wfx.bits_per_sample * wfx.channels) * 8;
    case audio::codec::adpcm_ima_dk3:
        return ((wfx.block_align - 16) * 2 / 3 * 4) / wfx.channels;
    case audio::codec::adpcm_ima_dk4:
        return 1 + (wfx.block_align - 4 * wfx.channels) * 2 / wfx.channels;
    case audio::codec::wma_v1:
    case audio::codec::wma_v2:
        return (wfx.block_align * wfx.sample_rate) / wfx.byte_rate;
    }

    if (wfx.bits_per_sample > 0 && wfx.block_align > 0 && wfx.channels > 0) {
        return (uint32{wfx.block_align} * 8)
             / (uint32{wfx.bits_per_sample} * wfx.channels);
    }
    return 0;
}

}     // namespace <unnamed>

audio::codec_format parse_format(io::reader r)
{
    waveformatextensible wfx;
    r.gather<LE>(wfx.format_tag,
                 wfx.channels,
                 wfx.sample_rate,
                 wfx.byte_rate,
                 wfx.block_align);

    wfx.bits_per_sample = r.try_read<uint16,LE>().value_or(8);
    wfx.extra_size      = r.try_read<uint16,LE>().value_or(0);
    if (wfx.extra_size > r.remain()) {
        wfx.extra_size = static_cast<uint16>(r.remain());
    }

    if (wfx.extra_size >= 22 && wfx.format_tag == 0xfffe) {
        wfx.extra_size -= 22;
        r.gather_unchecked<LE>(wfx.valid_bits_per_sample,
                               wfx.channel_mask,
                               wfx.sub_format);
    }
    else {
        wfx.valid_bits_per_sample = 0;
        wfx.channel_mask = 0;
        wfx.sub_format = {};
    }

    if (wfx.sample_rate == 0) {
        raise(errc::unsupported_format, "cannot have a sample rate of zero");
    }
    if (wfx.channels == 0) {
        raise(errc::unsupported_format, "cannot have zero channels");
    }

    audio::codec_format fmt;
    if (wfx.extra_size != 0) {
        fmt.extra.assign(r.peek(), wfx.extra_size);
    }
    wave::get_codec_id_and_flags(wfx, fmt);

    fmt.frames_per_packet = wave::get_frames_per_packet(fmt.codec_id, wfx);
    fmt.sample_rate       = wfx.sample_rate;
    fmt.bytes_per_packet  = wfx.block_align;
    fmt.bit_rate          = wfx.byte_rate * uint32{8};

    if (wfx.format_tag == 0x0045 || wfx.format_tag == 0x0064) {
        fmt.bits_per_sample = fmt.bit_rate / fmt.sample_rate;
    }
    else if (wfx.valid_bits_per_sample != 0) {
        fmt.bits_per_sample = wfx.valid_bits_per_sample;
    }
    else {
        fmt.bits_per_sample = wfx.bits_per_sample;
    }

    fmt.channels       = wfx.channels;
    fmt.channel_layout = wfx.channel_mask;
    if (fmt.channel_layout == 0) {
        fmt.channel_layout = audio::guess_channel_layout(fmt.channels);
    }
    return fmt;
}


namespace {

class demuxer final :
    public audio::basic_demuxer<wave::demuxer>
{
    using Base = audio::basic_demuxer<wave::demuxer>;
    friend class audio::basic_demuxer<wave::demuxer>;

public:
    explicit demuxer(ref_ptr<io::stream>, audio::open_mode);

    void seek(uint64);

    auto get_info(uint32);
    auto get_image(media::image::type);
    auto get_chapter_count() const noexcept;

private:
    bool feed(io::buffer&);

    void parse_wave(bool&, bool&);
    void parse_wave64(bool&, bool&);

    ref_ptr<io::stream> file;
    uint64 data_chunk_begin{};
    uint64 data_chunk_end{};
    uint64 info_chunk_begin{};
    uint32 info_chunk_len{};
    uint64 id3_chunk_begin{};
    uint64 packet_count;
    uint32 packet_step;
    uint32 riff_type;
};


demuxer::demuxer(ref_ptr<io::stream> s, audio::open_mode const mode) :
    file(std::move(s)),
    riff_type(file->read<uint32,BE>())
{
    auto found_chunk_data = false;
    auto found_chunk_fmt  = false;

    switch (riff_type) {
    case "RIFF"_4cc:
    case "RF64"_4cc:
        parse_wave(found_chunk_data, found_chunk_fmt);
        break;
    case "riff"_4cc:
        file->rewind();
        parse_wave64(found_chunk_data, found_chunk_fmt);
        break;
    default:
        raise(errc::invalid_data_format, "invalid 'RIFF' chunk tag");
    }

    if (AMP_UNLIKELY(!found_chunk_data || !found_chunk_fmt)) {
        raise(errc::invalid_data_format, "missing required chunk '%s'",
              (found_chunk_data ? "fmt " : "data"));
    }

    if (!(mode & (audio::playback | audio::metadata))) {
        return;
    }

    Base::resolve_decoder();
    packet_count = (data_chunk_end - data_chunk_begin)
                 / format.bytes_per_packet;

    if (Base::total_frames == 0) {
        Base::total_frames = packet_count * format.frames_per_packet;
    }
    if (format.bit_rate == 0) {
        format.bit_rate = static_cast<uint32>(muldiv(
                data_chunk_end - data_chunk_begin,
                Base::format.sample_rate * 8,
                Base::total_frames));
    }
    Base::average_bit_rate = format.bit_rate;
    Base::instant_bit_rate = format.bit_rate;

    if (mode & audio::playback) {
        if (format.codec_id == audio::codec::lpcm ||
            format.codec_id == audio::codec::alaw ||
            format.codec_id == audio::codec::ulaw) {
            packet_step = std::max(format.sample_rate / 10, uint32{1});
        }
        else {
            packet_step = 1;
        }
        file->seek(data_chunk_begin);
    }
}

void demuxer::parse_wave(bool& found_chunk_data, bool& found_chunk_fmt)
{
    uint32 riff_chunk_size;
    uint8  riff_chunk_type[4];
    file->gather<LE>(riff_chunk_size, riff_chunk_type);

    if (io::load<uint32,BE>(riff_chunk_type) != "WAVE"_4cc) {
        raise(errc::invalid_data_format, "invalid RIFF chunk type");
    }

    if (riff_type == "RF64"_4cc) {
        uint8  chunk_id[4];
        uint32 chunk_len;
        file->gather<LE>(chunk_id, chunk_len);

        if (AMP_UNLIKELY(io::load<uint32,BE>(chunk_id) != "ds64"_4cc)) {
            raise(errc::invalid_data_format, "missing required chunk 'ds64'");
        }
        if (AMP_UNLIKELY(chunk_len < 24)) {
            raise(errc::invalid_data_format, "'ds64' chunk is too small");
        }

        uint64 frames;
        file->gather<LE>(io::ignore<8>, data_chunk_end, frames);
        file->skip(chunk_len - 24);
        Base::total_frames = frames;
    }

    auto const file_length = file->size();
    auto       file_offset = file->tell();

    while ((file_offset + 8) < file_length) {
        uint8  chunk_id[4];
        uint32 chunk_len;
        file->gather<LE>(chunk_id, chunk_len);

        switch (io::load<uint32,BE>(chunk_id)) {
        case "data"_4cc:
            found_chunk_data = true;
            if (data_chunk_end == 0) {
                data_chunk_end = chunk_len;
            }
            data_chunk_begin += file_offset + 8;
            data_chunk_end   += data_chunk_begin;
            break;
        case "fmt "_4cc:
            found_chunk_fmt = true;
            format = wave::parse_format(io::buffer{*file, chunk_len});
            break;
        case "fact"_4cc:
            if (Base::total_frames == 0 && chunk_len >= 4) {
                Base::total_frames = file->read<uint32,LE>();
            }
            break;
        case "id3 "_4cc:
            id3_chunk_begin = file_offset + 8;
            break;
        case "LIST"_4cc:
            if (chunk_len >= 4 && file->read<uint32,BE>() == "INFO"_4cc) {
                info_chunk_begin = file_offset + 8 + 4;
                info_chunk_len   = chunk_len - 4;
            }
            break;
        }

        file_offset = align_up(file_offset + 8 + chunk_len, 2);
        file->seek(file_offset);
    }
}

void demuxer::parse_wave64(bool& found_chunk_data, bool& found_chunk_fmt)
{
    constexpr auto guid_riff = "72696666-2e91-cf11-a5d6-28db04c10000"_guid;
    constexpr auto guid_wave = "77617665-f3ac-d311-8cd1-00c04f8edb8a"_guid;
    constexpr auto guid_fmt  = "666d7420-f3ac-d311-8cd1-00c04f8edb8a"_guid;
    constexpr auto guid_data = "64617461-f3ac-d311-8cd1-00c04f8edb8a"_guid;
    //constexpr auto guid_fact = "66616374-f3ac-d311-8cd1-00c04f8edb8a"_guid;
    //constexpr auto guid_summ = "bc945f92-5a52-d211-86dc-00c04f8edb8a"_guid;

    guid   riff_chunk_id;
    uint64 riff_chunk_len;
    guid   riff_chunk_type;

    file->gather<LE>(riff_chunk_id,
                     riff_chunk_len,
                     riff_chunk_type);

    if (riff_chunk_id   != guid_riff ||
        riff_chunk_type != guid_wave ||
        riff_chunk_len  < uint64{24 * 3}) {
        raise(errc::invalid_data_format, "invalid Wave64 RIFF chunk");
    }

    auto const file_length = file->size();
    auto       file_offset = file->tell();

    while ((file_offset + 24) < file_length) {
        guid   chunk_id;
        uint64 chunk_len;
        file->gather<LE>(chunk_id, chunk_len);

        if (AMP_UNLIKELY(chunk_len < 24)) {
            raise(errc::invalid_data_format,
                  "Wave64 chunk cannot be smaller than its header");
        }

        if (chunk_id == guid_fmt) {
            found_chunk_fmt = true;
            format = wave::parse_format(io::buffer{*file, chunk_len});
        }
        else if (chunk_id == guid_data) {
            found_chunk_data = true;
            data_chunk_begin = file_offset + 24;
            data_chunk_end   = file_offset + chunk_len;
        }

        file_offset += chunk_len;
        file->seek(file_offset);
    }
}

bool demuxer::feed(io::buffer& dest)
{
    auto const remain = data_chunk_end - file->tell();
    if (AMP_UNLIKELY(remain < format.bytes_per_packet)) {
        return false;
    }

    auto packet_size = format.bytes_per_packet * packet_step;
    if (AMP_UNLIKELY(packet_size > remain)) {
        packet_size = static_cast<uint32>(remain);
        packet_size = packet_size - (packet_size % format.bytes_per_packet);
    }

    dest.assign(*file, packet_size);
    return true;
}

void demuxer::seek(uint64 const pts)
{
    auto nearest = pts / format.frames_per_packet;
    auto priming = pts % format.frames_per_packet;

    if (nearest >= packet_count) {
        nearest  = packet_count;
        priming  = 0;
    }

    file->seek(data_chunk_begin + (nearest * format.bytes_per_packet));
    Base::set_seek_target_and_offset(pts, priming);
}

auto demuxer::get_info(uint32 const /* chapter_number */)
{
    audio::stream_info info{Base::get_format()};
    info.frames           = Base::total_frames;
    info.codec_id         = Base::format.codec_id;
    info.bits_per_sample  = Base::format.bits_per_sample;
    info.average_bit_rate = Base::average_bit_rate;

    auto const container = (riff_type == "RIFF"_4cc) ? "Wave"
                         : (riff_type == "RF64"_4cc) ? "RF64"
                         :                             "Wave64";
    info.props.emplace(tags::container, container);

    if (id3_chunk_begin != 0) {
        file->seek(id3_chunk_begin);
        id3v2::read(*file, info.tags);
    }
    else if (info_chunk_len != 0) {
        file->seek(info_chunk_begin);
        io::buffer info_chunk{*file, info_chunk_len};

        for (io::reader r{info_chunk}; r.remain() > 8; ) {
            uint8  chunk_id[4];
            uint32 chunk_len;
            r.gather_unchecked<LE>(chunk_id, chunk_len);

            auto found = wave::info_keys.find(io::load<uint32,BE>(chunk_id));
            if (found != wave::info_keys.end()) {
                auto const s = reinterpret_cast<char const*>(r.peek(chunk_len));
                auto const len = cxp::strlen(s, chunk_len);
                info.tags.emplace(found->second, u8string{s, len});
            }
            r.skip((chunk_len + 1) & ~uint32{1});
        }
        info.props.emplace(tags::tag_type, "INFO chunk");
    }
    return info;
}

auto demuxer::get_image(media::image::type const type)
{
    if (id3_chunk_begin != 0) {
        file->seek(id3_chunk_begin);
        return id3v2::find_image(*file, type);
    }
    return media::image{};
}

auto demuxer::get_chapter_count() const noexcept
{
    return uint32{0};
}

AMP_REGISTER_INPUT(demuxer, "rf64", "w64", "wav");

}}}   // namespace amp::wave::<unnamed>

