////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/mp4_track.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/decoder.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/pcm.hpp>
#include <amp/cxp/map.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/memory.hpp>
#include <amp/numeric.hpp>
#include <amp/optional.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/utility.hpp>

#include "mp4_audio.hpp"
#include "mp4_track.hpp"

#include <algorithm>
#include <cinttypes>
#include <numeric>


namespace amp {
namespace mp4 {
namespace {

enum compression_id : int16 {
    variable                 = -2,
    fixed                    = -1,
    uncompressed             =  0,
    two_to_one               =  1,
    eight_to_three           =  2,
    three_to_one             =  3,
    six_to_one               =  4,
    six_to_one_packet_size   =  8,
    three_to_one_packet_size = 16,
};

struct compression_info
{
    uint32 frames_per_packet;
    uint32 bytes_per_packet;
    uint32 bits_per_sample;
};


bool has_fixed_compression(mp4::box const& entry) noexcept
{
    if (entry.soun.compression_id != compression_id::variable) {
        if (entry.up()->stsd.version() == 0) {
            auto found = entry.find("/ftyp");
            if (!found || found->ftyp.compatible_with(brand::qt)) {
                return true;
            }
        }
    }
    return false;
}

auto get_implicit_fixed_compression_info(mp4::box const& entry)
{
    mp4::compression_info info;
    auto&& soun = entry.soun;

    switch (entry.type()) {
    case "ima4"_4cc:
    case "ms\x00\x11"_4cc:
        info.frames_per_packet = 64;
        info.bytes_per_packet  = 34 * soun.channels;
        info.bits_per_sample   = 16;
        break;
    case "MAC3"_4cc:
        info.frames_per_packet = 6;
        info.bytes_per_packet  = 2 * soun.channels;
        info.bits_per_sample   = 8;
        break;
    case "MAC6"_4cc:
        info.frames_per_packet = 6;
        info.bytes_per_packet  = 1 * soun.channels;
        info.bits_per_sample   = 8;
        break;
    case "agsm"_4cc:
        info.frames_per_packet = 160;
        info.bytes_per_packet  = 33;
        info.bits_per_sample   = 16;
        break;
    case "alaw"_4cc:
    case "ulaw"_4cc:
        info.frames_per_packet = 1;
        info.bytes_per_packet  = 1 * soun.channels;
        info.bits_per_sample   = 8;
        break;
    case 0x00000000:
    case "NONE"_4cc:
    case "fl32"_4cc:
    case "fl64"_4cc:
    case "in24"_4cc:
    case "in32"_4cc:
    case "lpcm"_4cc:
    case "raw "_4cc:
    case "sowt"_4cc:
    case "twos"_4cc:
        info.frames_per_packet = 1;
        info.bytes_per_packet  = soun.sample_size * soun.channels / 8;
        info.bits_per_sample   = soun.sample_size;
        break;
    default:
        raise(errc::failure,
              "no implicit parameters for audio sample entry type: %#x",
              entry.type());
    }

    return info;
}

auto get_fixed_compression_info(mp4::box const& entry)
{
    if (!has_fixed_compression(entry)) {
        return optional<mp4::compression_info>{};
    }

    mp4::compression_info info;
    auto&& soun = entry.soun;

    switch (soun.version) {
    case 0:
        info = mp4::get_implicit_fixed_compression_info(entry);
        break;
    case 1:
        info.frames_per_packet = soun.v1.samples_per_packet;
        info.bytes_per_packet  = soun.v1.bytes_per_frame;
        info.bits_per_sample   = soun.sample_size;
        break;
    case 2:
        info.frames_per_packet = soun.v2.const_lpcm_frames_per_audio_packet;
        info.bytes_per_packet  = soun.v2.const_bytes_per_audio_packet;
        info.bits_per_sample   = soun.v2.const_bits_per_channel;
        break;
    default:
        AMP_UNREACHABLE();
    }
    return make_optional(info);
}


void parse_entry_lpcm(mp4::box const& entry, audio::codec_format& fmt)
{
    auto get_v2_flags = [&]() -> uint32 {
        auto found = entry.find_first_of("enda", "wave/enda");
        if (!found || !found->enda.little_endian) {
            return audio::pcm::big_endian;
        }
        return 0;
    };

    switch (entry.type()) {
    case "\0\0\0\0"_4cc:
    case "NONE"_4cc:
    case "raw "_4cc:
        if (entry.soun.sample_size == 16) {
            fmt.flags = audio::pcm::big_endian | audio::pcm::signed_int;
        }
        break;
    case "twos"_4cc:
        fmt.flags = audio::pcm::big_endian | audio::pcm::signed_int;
        break;
    case "sowt"_4cc:
        fmt.flags = audio::pcm::signed_int;
        break;
    case "in24"_4cc:
        fmt.flags = audio::pcm::signed_int | get_v2_flags();
        fmt.bits_per_sample = 24;
        break;
    case "in32"_4cc:
        fmt.flags = audio::pcm::signed_int | get_v2_flags();
        fmt.bits_per_sample = 32;
        break;
    case "fl32"_4cc:
        fmt.flags = audio::pcm::ieee_float | get_v2_flags();
        fmt.bits_per_sample = 32;
        break;
    case "fl64"_4cc:
        fmt.flags = audio::pcm::ieee_float | get_v2_flags();
        fmt.bits_per_sample = 64;
        break;
    case "lpcm"_4cc:
        if (entry.soun.version != 2) {
            raise(errc::invalid_data_format,
                  "MP4 'lpcm' must be a version 2 sound description box");
        }
        if (entry.soun.v2.format_specific_flags & 0x1) {
            fmt.flags |= audio::pcm::ieee_float;
        }
        if (entry.soun.v2.format_specific_flags & 0x2) {
            fmt.flags |= audio::pcm::big_endian;
        }
        if (entry.soun.v2.format_specific_flags & 0x4) {
            fmt.flags |= audio::pcm::signed_int;
        }
        fmt.bits_per_sample = entry.soun.v2.const_bits_per_channel;
        break;
    }

    fmt.codec_id = audio::codec::lpcm;
    fmt.bit_rate = fmt.bits_per_sample * fmt.sample_rate * fmt.channels;
}

void apply_format_specific_overrides(audio::codec_format& fmt)
{
    switch (fmt.codec_id) {
    case audio::codec::ac3:
    case audio::codec::eac3:
        fmt.frames_per_packet = 1536;
        break;
    case audio::codec::amr_nb:
        fmt.frames_per_packet = 160;
        fmt.sample_rate = 8000;
        fmt.channels = 1;
        break;
    case audio::codec::amr_wb:
        fmt.frames_per_packet = 320;
        fmt.sample_rate = 16000;
        fmt.channels = 1;
        break;
    case audio::codec::qcelp:
        fmt.frames_per_packet = 160;
        fmt.channels = 1;
        break;
    case audio::codec::adpcm_ima_ms:
        fmt.bits_per_sample = 4;
        break;
    case audio::codec::alac:
        if (fmt.extra.size() >= 16) {
            fmt.frames_per_packet = io::load<uint32,BE>(&fmt.extra[12]);
        }
        break;
    }
}


constexpr cxp::map<uint32, uint32, 31> audio_sample_entry_to_codec {{
    { ".mp1"_4cc,       audio::codec::mpeg_layer1   },
    { ".mp2"_4cc,       audio::codec::mpeg_layer2   },
    { ".mp3"_4cc,       audio::codec::mpeg_layer3   },
    { "DTS "_4cc,       audio::codec::dts           },
    { "MAC3"_4cc,       audio::codec::mace3         },
    { "MAC6"_4cc,       audio::codec::mace6         },
    { "Qclp"_4cc,       audio::codec::qcelp         },
    { "Qclq"_4cc,       audio::codec::qcelp         },
    { "ac-3"_4cc,       audio::codec::ac3           },
    { "agsm"_4cc,       audio::codec::gsm           },
    { "alaw"_4cc,       audio::codec::alaw          },
    { "dtsc"_4cc,       audio::codec::dts           },
    { "dtse"_4cc,       audio::codec::dts_express   },
    { "dtsh"_4cc,       audio::codec::dts_hd        },
    { "dtsl"_4cc,       audio::codec::dts_hd_ma     },
    { "dvca"_4cc,       audio::codec::dv_audio      },
    { "ec-3"_4cc,       audio::codec::eac3          },
    { "ilbc"_4cc,       audio::codec::ilbc          },
    { "ima4"_4cc,       audio::codec::adpcm_ima_qt  },
    { "ms\x00\x02"_4cc, audio::codec::adpcm_ms      },
    { "ms\x00\x11"_4cc, audio::codec::adpcm_ima_ms  },
    { "ms\x00\x31"_4cc, audio::codec::gsm_ms        },
    { "ms\x00\x50"_4cc, audio::codec::mpeg_layer2   },
    { "ms\x00\x55"_4cc, audio::codec::mpeg_layer3   },
    { "nmos"_4cc,       audio::codec::nellymoser    },
    { "samr"_4cc,       audio::codec::amr_nb        },
    { "sawb"_4cc,       audio::codec::amr_wb        },
    { "sawp"_4cc,       audio::codec::amr_wb_plus   },
    { "sqcp"_4cc,       audio::codec::qcelp         },
    { "ulaw"_4cc,       audio::codec::ulaw          },
    { "vdva"_4cc,       audio::codec::dv_audio      },
}};
static_assert(cxp::is_sorted(audio_sample_entry_to_codec), "");

}     // namespace <unnamed>

optional<audio::codec_format> track::select_first_audio_sample_entry()
{
    auto select = [&](mp4::box const& entry) -> optional<audio::codec_format> {
        audio::codec_format fmt;
        auto&& soun = entry.soun;

        switch (soun.version) {
        case 2:
            fmt.sample_rate = numeric_cast<uint32>(soun.v2.audio_sample_rate);
            fmt.channels = soun.v2.audio_channels;
            fmt.bits_per_sample = soun.v2.const_bits_per_channel;
            break;
        case 1:
            fmt.channels = soun.channels;
            fmt.sample_rate = soun.sample_rate >> 16;
            fmt.bits_per_sample = soun.v1.bytes_per_sample * 8;
            break;
        case 0:
            fmt.channels = soun.channels;
            fmt.sample_rate = soun.sample_rate >> 16;
            fmt.bits_per_sample = soun.sample_size;
            break;
        default:
            AMP_UNREACHABLE();
        }

        switch (entry.type()) {
        case "mp4a"_4cc:
            if (auto found = entry.find_first_of("esds", "wave/esds")) {
                found->esds.dcd.setup(fmt);
            }
            break;
        case "alac"_4cc:
            if (auto found = entry.find_first_of("alac", "wave/alac")) {
                fmt.extra    = found->alac.extra;
                fmt.codec_id = audio::codec::alac;
            }
            break;
        case "QDMC"_4cc:
        case "QDM2"_4cc:
            if (auto found = entry.find("wave")) {
                fmt.extra    = found->wave.extra;
                fmt.codec_id = (entry.type() == "QDMC"_4cc)
                             ? audio::codec::qdesign
                             : audio::codec::qdesign2;
            }
            break;
        case "\0\0\0\0"_4cc:
        case "NONE"_4cc:
        case "fl32"_4cc:
        case "fl64"_4cc:
        case "in24"_4cc:
        case "in32"_4cc:
        case "lpcm"_4cc:
        case "raw "_4cc:
        case "sowt"_4cc:
        case "twos"_4cc:
            parse_entry_lpcm(entry, fmt);
            break;
        default:
            auto found = audio_sample_entry_to_codec.find(entry.type());
            if (found != audio_sample_entry_to_codec.end()) {
                fmt.codec_id = found->second;
            }
        }

        if (fmt.codec_id == 0) {
            return nullopt;
        }

        if (auto const info = mp4::get_fixed_compression_info(entry)) {
            qtff_sample_size        = info->bytes_per_packet;
            qtff_samples_per_packet = info->frames_per_packet;
            fmt.frames_per_packet   = info->frames_per_packet;

            if (fmt.bits_per_sample == 0) {
                fmt.bits_per_sample = info->bits_per_sample;
            }
        }
        else {
            qtff_samples_per_packet = 1;
            qtff_sample_size        = stsz.sample_size;
        }

        apply_format_specific_overrides(fmt);
        if (fmt.bytes_per_packet == 0) {
            fmt.bytes_per_packet = qtff_sample_size;
        }
        if (fmt.channel_layout == 0) {
            fmt.channel_layout = audio::guess_channel_layout(fmt.channels);
        }
        return make_optional(std::move(fmt));
    };

    auto const stsd = stbl.find("stsd");
    AMP_ASSERT(stsd);

    for (auto&& entry : stsd->children) {
        if (auto fmt = select(entry)) {
            return fmt;
        }
    }
    return nullopt;
}

uint32 track::get_average_bit_rate() const noexcept
{
    auto&& stsd = *stbl.find("stsd");
    if (auto found = stsd.find_first_of("mp4a/esds", "mp4a/wave/esds")) {
        if (found->esds.dcd.average_bit_rate != 0) {
            return found->esds.dcd.average_bit_rate;
        }
    }
    if (auto found = stsd.find_first_of("alac/alac", "alac/wave/alac")) {
        if (found->alac.extra.size() >= 20) {
            return io::load<uint32,BE>(&found->alac.extra[16]);
        }
    }

    if (mdhd.duration == 0) {
        return 0;
    }

    uint64 bits;
    if (qtff_sample_size != 0) {
        bits = uint64{qtff_sample_size} * stsz.sample_count;
    }
    else {
        bits = std::accumulate(stsz.entries.begin(),
                               stsz.entries.end(),
                               uint64{0});
    }
    return static_cast<uint32>(muldiv(bits, mdhd.time_scale, mdhd.duration));
}


namespace {

inline auto& stsc_entry_for_sample(mp4::stsc_box_data const& stsc,
                                   uint32 const sample_number) noexcept
{
    AMP_ASSERT(!stsc.entries.empty());
    for (auto const i : xrange(uint32{1}, stsc.entries.size())) {
        if (sample_number < stsc.entries[i].first_sample) {
            return stsc.entries[i - 1];
        }
    }
    return stsc.entries.back();
}

inline auto& stsc_entry_for_chunk(mp4::stsc_box_data const& stsc,
                                  uint32 const chunk_number) noexcept
{
    AMP_ASSERT(!stsc.entries.empty());
    for (auto const i : xrange(uint32{1}, stsc.entries.size())) {
        if (chunk_number < stsc.entries[i].first_chunk) {
            return stsc.entries[i - 1];
        }
    }
    return stsc.entries.back();
}

}     // namespace <unnamed>


bool track::feed(io::stream& file, io::buffer& dest)
{
    if (sample_number >= get_sample_count()) {
        return segment.feed(file, dest);
    }
    if (sample_number >= last_sample_in_chunk) {
        if (AMP_UNLIKELY(++chunk_number > stco.entries.size())) {
            raise(errc::out_of_bounds,
                  "invaid chunk number: %" PRIu32 "/%" PRIu32,
                  chunk_number, stco.entries.size());
        }

        auto const& entry = stsc_entry_for_chunk(stsc, chunk_number);
        auto const offset = chunk_number - entry.first_chunk;

        last_sample_in_chunk  = entry.first_sample;
        last_sample_in_chunk += entry.samples_per_chunk * (offset + 1);
        file.seek(stco.entries[chunk_number - 1]);
    }

    auto const sample_size = qtff_sample_size != 0
                           ? qtff_sample_size
                           : stsz.entries[sample_number];

    dest.assign(file, sample_size);
    sample_number += qtff_samples_per_packet;
    return true;
}

void track::seek(io::stream& file, uint64 pts, uint64& priming)
{
    segment.reset();
    if (pts >= mdhd.duration) {
        pts -= mdhd.duration;
        sample_number = get_sample_count();
        return segment.seek(file, pts, priming);
    }

    auto nearest = uint32{0};
    for (auto const& entry : stts.entries) {
        if (pts < (entry.sample_count * entry.sample_delta)) {
            if (entry.sample_delta != 0) {
                nearest += pts / entry.sample_delta;
                priming += pts % entry.sample_delta;
            }
            break;
        }
        nearest += entry.sample_count;
        pts     -= entry.sample_count * entry.sample_delta;
    }

    sample_number = nearest;
    auto const& entry = stsc_entry_for_sample(stsc, sample_number);
    auto const spc = entry.samples_per_chunk;

    chunk_number  = entry.first_chunk;
    chunk_number += (sample_number - entry.first_sample) / spc;

    if (AMP_UNLIKELY(chunk_number > stco.entries.size())) {
        raise(errc::out_of_bounds,
              "invaid chunk number: %" PRIu32 "/%" PRIu32,
              chunk_number, stco.entries.size());
    }

    last_sample_in_chunk  = entry.first_sample;
    last_sample_in_chunk += ((chunk_number - entry.first_chunk) + 1) * spc;

    auto const first_sample_in_chunk = last_sample_in_chunk - spc;

    uint64 file_offset;
    if (qtff_sample_size != 0) {
        file_offset  = sample_number - first_sample_in_chunk;
        file_offset /= qtff_samples_per_packet;
        file_offset *= qtff_sample_size;
    }
    else {
        file_offset = std::accumulate(
            stsz.entries.cbegin() + first_sample_in_chunk,
            stsz.entries.cbegin() + sample_number,
            uint64{0});
    }

    file_offset += stco.entries[chunk_number - 1];
    file.seek(file_offset);
}

}}    // namespace amp::mp4

