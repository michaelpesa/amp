////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/caf.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/decoder.hpp>
#include <amp/audio/demuxer.hpp>
#include <amp/audio/input.hpp>
#include <amp/audio/pcm.hpp>
#include <amp/cxp/map.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/memory.hpp>
#include <amp/io/reader.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/dictionary.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/muldiv.hpp>
#include <amp/numeric.hpp>
#include <amp/stddef.hpp>
#include <amp/string.hpp>
#include <amp/string_view.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include "mp4_audio.hpp"
#include "mp4_descriptor.hpp"

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>


namespace amp {
namespace caf {
namespace {

struct file_header
{
    uint32 type;
    uint16 version;
    uint16 flags;
};

struct chunk_header
{
    uint32 type;
    uint64 size;
};

struct audio_description_chunk
{
    double sample_rate;
    uint32 format_id;
    uint32 format_flags;
    uint32 bytes_per_packet;
    uint32 frames_per_packet;
    uint32 channels_per_frame;
    uint32 bits_per_channel;
};

struct packet_description
{
    uint32 frames;
    uint32 bytes;
};

struct packet_table_chunk
{
    uint64 number_packets;
    uint64 number_valid_frames;
    uint32 priming_frames;
    uint32 remainder_frames;
    std::vector<packet_description> descriptions;
};

struct data_chunk
{
    uint64 offset;
    uint64 size;
};


void validate_file_header(io::stream& file)
{
    caf::file_header header;
    file.gather<BE>(header.type, header.version, header.flags);

    if (header.type != "caff"_4cc || header.version != 1) {
        raise(errc::invalid_data_format, "invalid CAF file header");
    }
}


class demuxer final :
    public audio::basic_demuxer<caf::demuxer>
{
    using Base = audio::basic_demuxer<caf::demuxer>;
    friend class audio::basic_demuxer<caf::demuxer>;

public:
    explicit demuxer(ref_ptr<io::stream>, audio::open_mode);

    void seek(uint64);

    auto get_info(uint32);
    auto get_image(media::image_type);
    auto get_chapter_count() const noexcept;

private:
    bool feed(io::buffer&);

    void read_kuki_chunk(uint64);
    void read_pakt_chunk(uint64);
    void read_info_chunk(uint64);
    void prepare_for_playback();

    ref_ptr<io::stream> file;
    caf::audio_description_chunk desc{};
    caf::packet_table_chunk pakt{};
    caf::data_chunk data{};
    media::dictionary tags;
    uint64 packet_number{};
    uint32 packet_step{1};
};


void demuxer::read_kuki_chunk(uint64 const size)
{
    format.extra.assign(*file, numeric_cast<std::size_t>(size));
}

void demuxer::read_pakt_chunk(uint64 const size)
{
    io::buffer buf{*file, numeric_cast<std::size_t>(size)};
    io::reader r{buf};

    r.gather<BE>(pakt.number_packets,
                 pakt.number_valid_frames,
                 pakt.priming_frames,
                 pakt.remainder_frames);

    auto const count = numeric_cast<std::size_t>(pakt.number_packets);
    pakt.descriptions.resize(count);

    for (auto&& entry : pakt.descriptions) {
        entry.bytes  = desc.bytes_per_packet != 0
                     ? desc.bytes_per_packet
                     : mp4::read_descriptor_length(r);
        entry.frames = desc.frames_per_packet != 0
                     ? desc.frames_per_packet
                     : mp4::read_descriptor_length(r);

        if (entry.bytes == 0 || entry.frames == 0) {
            raise(errc::failure,
                  "CAF packet entries cannot have zero bytes or frames");
        }
    }
}

void demuxer::read_info_chunk(uint64 const size)
{
    io::buffer buf{*file, numeric_cast<std::size_t>(size)};
    io::reader r{buf};

    tags.reserve(r.read<uint32,BE>());

    auto const text = reinterpret_cast<char const*>(r.peek());
    auto const strings = tokenize(string_view{text, r.remain()}, '\0');

    auto const last = strings.end();
    for (auto first = strings.begin(); first != last; ) {
        auto const key = *first++;
        if (first != last) {
            tags.emplace(tags::map_common_key(key), *first++);
        }
    }
}

constexpr cxp::map<uint32, uint32, 20> format_id_to_codec {{
    { ".mp1"_4cc,       audio::codec::mpeg_layer1  },
    { ".mp2"_4cc,       audio::codec::mpeg_layer2  },
    { ".mp3"_4cc,       audio::codec::mpeg_layer3  },
    { "MAC3"_4cc,       audio::codec::mace3        },
    { "MAC6"_4cc,       audio::codec::mace6        },
    { "QDM2"_4cc,       audio::codec::qdesign2     },
    { "QDMC"_4cc,       audio::codec::qdesign      },
    { "Qclp"_4cc,       audio::codec::qcelp        },
    { "ac-3"_4cc,       audio::codec::ac3          },
    { "agsm"_4cc,       audio::codec::gsm          },
    { "alaw"_4cc,       audio::codec::alaw         },
    { "ec-3"_4cc,       audio::codec::eac3         },
    { "ilbc"_4cc,       audio::codec::ilbc         },
    { "ima4"_4cc,       audio::codec::adpcm_ima_qt },
    { "ms\x00\x02"_4cc, audio::codec::adpcm_ms     },
    { "ms\x00\x11"_4cc, audio::codec::adpcm_ima_ms },
    { "ms\x00\x31"_4cc, audio::codec::gsm_ms       },
    { "ms\x00\x55"_4cc, audio::codec::mpeg_layer3  },
    { "samr"_4cc,       audio::codec::amr_nb       },
    { "ulaw"_4cc,       audio::codec::ulaw         },
}};
static_assert(cxp::is_sorted(format_id_to_codec), "");

void demuxer::prepare_for_playback()
{
    AMP_ASSERT(decoder == nullptr);
    AMP_ASSERT(desc.format_id != 0);

    if (pakt.number_valid_frames == 0) {
        pakt.number_valid_frames = pakt.number_packets * desc.frames_per_packet;
    }

    format.sample_rate = numeric_cast<uint32>(desc.sample_rate);
    format.channels = desc.channels_per_frame;
    format.channel_layout = audio::guess_channel_layout(format.channels);
    format.bits_per_sample = desc.bits_per_channel;
    format.bytes_per_packet = desc.bytes_per_packet;
    format.frames_per_packet = desc.frames_per_packet;

    switch (desc.format_id) {
    case "aac "_4cc:
    case "celp"_4cc:
    case "hvxc"_4cc:
    case "twvq"_4cc:
        {
            mp4::decoder_config_descriptor dcd;
            dcd.parse(format.extra);
            dcd.setup(format);

            if (format.codec_id == audio::codec::he_aac_v1 ||
                format.codec_id == audio::codec::he_aac_v2) {
                desc.frames_per_packet   *= 2;
                pakt.priming_frames      *= 2;
                pakt.remainder_frames    *= 2;
                pakt.number_valid_frames *= 2;
                for (auto&& entry : pakt.descriptions) {
                    entry.frames *= 2;
                }
            }
            format.frames_per_packet = desc.frames_per_packet;
            break;
        }
    case "alac"_4cc:
        if (format.extra.size() > 24) {
            if (io::load<uint32,BE>(&format.extra[4]) == "frma"_4cc &&
                io::load<uint32,BE>(&format.extra[8]) == "alac"_4cc) {
                format.extra.pop_front(24);
            }
        }
        format.codec_id = audio::codec::alac;
        break;
    case "lpcm"_4cc:
        if (desc.format_flags & 0x1) {
            format.flags |= audio::pcm::ieee_float;
        }
        else {
            format.flags |= audio::pcm::signed_int;
        }
        if (!(desc.format_flags & 0x2)) {
            format.flags |= audio::pcm::big_endian;
        }
        format.codec_id = audio::codec::lpcm;
        break;
    default:
        auto found = caf::format_id_to_codec.find(desc.format_id);
        if (found == caf::format_id_to_codec.end()) {
            raise(errc::unsupported_format, "unrecognized CAF format ID: %#x",
                  desc.format_id);
        }
        format.codec_id = found->second;
    }

    Base::resolve_decoder();
    Base::set_total_frames(pakt.number_valid_frames);
    Base::set_encoder_delay(pakt.priming_frames);

    if (format.codec_id == audio::codec::lpcm ||
        format.codec_id == audio::codec::alaw ||
        format.codec_id == audio::codec::ulaw) {
        packet_step = std::max(format.sample_rate / 10, uint32{1});
    }
}


demuxer::demuxer(ref_ptr<io::stream> s, audio::open_mode const mode) :
    file{std::move(s)}
{
    caf::validate_file_header(*file);
    if (!(mode & (audio::playback | audio::metadata))) {
        // CAF doesn't support embedded pictures, so we have nothing else
        // to do here.
        return;
    }

    caf::chunk_header chunk;
    file->gather<BE>(chunk.type, chunk.size);

    if (chunk.type != "desc"_4cc || chunk.size != 32) {
        raise(errc::invalid_data_format,
              "CAF audio description chunk not present");
    }

    file->gather<BE>(desc.sample_rate,
                     desc.format_id,
                     desc.format_flags,
                     desc.bytes_per_packet,
                     desc.frames_per_packet,
                     desc.channels_per_frame,
                     desc.bits_per_channel);

    auto const file_length = file->size();
    auto       file_offset = file->tell();

    while ((file_offset + 12) <= file_length) {
        file->gather<BE>(chunk.type, chunk.size);

        switch (chunk.type) {
        case "data"_4cc:
            data.offset = file_offset + 16;
            if (chunk.size == std::numeric_limits<uint64>::max()) {
                data.size = file->size() - file_offset - 16;
            }
            else {
                if (chunk.size <= 4) {
                    raise(errc::invalid_data_format,
                          "CAF 'data' chunk is too small");
                }
                data.size = chunk.size - 4;
            }
            break;
        case "info"_4cc:
            if (mode & audio::metadata) {
                read_info_chunk(chunk.size);
            }
            break;
        case "kuki"_4cc:
            read_kuki_chunk(chunk.size);
            break;
        case "pakt"_4cc:
            read_pakt_chunk(chunk.size);
            break;
        }

        file_offset += 12 + chunk.size;
        file->seek(file_offset);
    }

    if (data.offset == 0) {
        raise(errc::invalid_data_format, "CAF audio data chunk not present");
    }
    if (pakt.number_packets == 0) {
        if (desc.bytes_per_packet == 0 || desc.frames_per_packet == 0) {
            raise(errc::invalid_data_format,
                  "CAF files containing variable bit rate or variable "
                  "frame rate codecs must contain a packet table chunk");
        }
        pakt.number_packets = data.size / desc.bytes_per_packet;
    }

    prepare_for_playback();
    average_bit_rate = static_cast<uint32>(muldiv(data.size,
                                                  format.sample_rate * 8,
                                                  total_frames));
    if (mode & audio::playback) {
        file->seek(data.offset);
    }
}

bool demuxer::feed(io::buffer& dest)
{
    if (AMP_UNLIKELY(packet_number >= pakt.number_packets)) {
        return false;
    }

    auto const bytes = desc.bytes_per_packet != 0
                     ? desc.bytes_per_packet
                     : pakt.descriptions[packet_number].bytes;

    auto const frames = desc.frames_per_packet != 0
                      ? desc.frames_per_packet
                      : pakt.descriptions[packet_number].frames;

    instant_bit_rate = muldiv(bytes, format.sample_rate * 8, frames);

    auto n = packet_step;
    if (AMP_UNLIKELY(n > (pakt.number_packets - packet_number))) {
        n = static_cast<uint32>(pakt.number_packets - packet_number);
    }

    dest.assign(*file, bytes * n);
    packet_number += n;
    return true;
}

void demuxer::seek(uint64 const target)
{
    auto target_offset = uint64{};

    if (desc.bytes_per_packet != 0 && desc.frames_per_packet != 0) {
        packet_number = target / desc.frames_per_packet;
        target_offset = target % desc.frames_per_packet;
        file->seek(data.offset + (packet_number * desc.bytes_per_packet));
    }
    else {
        auto accum_frames = uint64{};
        auto accum_bytes  = uint64{};

        packet_number = 0;
        for ( ; packet_number < pakt.descriptions.size(); ++packet_number) {
            auto const& entry = pakt.descriptions[packet_number];
            if (target <= entry.frames + accum_frames) {
                target_offset = target - accum_frames;
                file->seek(data.offset + accum_bytes);
                break;
            }
            accum_frames += entry.frames;
            accum_bytes  += entry.bytes;
        }
    }

    Base::set_seek_target_and_offset(target, target_offset);
}

auto demuxer::get_info(uint32 const /* chapter_number */)
{
    audio::stream_info info{Base::get_format()};
    info.frames           = Base::total_frames;
    info.codec_id         = Base::format.codec_id;
    info.bits_per_sample  = Base::format.bits_per_sample;
    info.average_bit_rate = Base::average_bit_rate;
    info.props.emplace(tags::container, "CAF (Core Audio Format)");
    info.tags = tags;
    return info;
}

auto demuxer::get_image(media::image_type)
{
    return media::image{};
}

auto demuxer::get_chapter_count() const noexcept
{
    return uint32{0};
}

AMP_REGISTER_INPUT(demuxer, "caf");

}}}   // namespace amp::caf::<unnamed>

