////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/au.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/decoder.hpp>
#include <amp/audio/demuxer.hpp>
#include <amp/audio/input.hpp>
#include <amp/audio/pcm.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/numeric.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include <algorithm>
#include <cinttypes>
#include <utility>


namespace amp {
namespace au {
namespace {

struct header
{
    uint32 signature;
    uint32 data_start;
    uint32 data_length;
    uint32 encoding;
    uint32 sample_rate;
    uint32 channels;
};

audio::codec_format make_codec_format(au::header const& head)
{
    audio::codec_format fmt;
    fmt.sample_rate    = head.sample_rate;
    fmt.channels       = head.channels;
    fmt.channel_layout = audio::guess_channel_layout(fmt.channels);

    switch (head.encoding) {
    case 1:
        fmt.codec_id = audio::codec::ulaw;
        fmt.bits_per_sample = 8;
        break;
    case 2:
        fmt.codec_id = audio::codec::lpcm;
        fmt.flags = audio::pcm::signed_int | audio::pcm::big_endian;
        fmt.bits_per_sample = 8;
        break;
    case 3:
        fmt.codec_id = audio::codec::lpcm;
        fmt.flags = audio::pcm::signed_int | audio::pcm::big_endian;
        fmt.bits_per_sample = 16;
        break;
    case 4:
        fmt.codec_id = audio::codec::lpcm;
        fmt.flags = audio::pcm::signed_int | audio::pcm::big_endian;
        fmt.bits_per_sample = 24;
        break;
    case 5:
        fmt.codec_id = audio::codec::lpcm;
        fmt.flags = audio::pcm::signed_int | audio::pcm::big_endian;
        fmt.bits_per_sample = 32;
        break;
    case 6:
        fmt.codec_id = audio::codec::lpcm;
        fmt.flags = audio::pcm::ieee_float | audio::pcm::big_endian;
        fmt.bits_per_sample = 32;
        break;
    case 7:
        fmt.codec_id = audio::codec::lpcm;
        fmt.flags = audio::pcm::ieee_float | audio::pcm::big_endian;
        fmt.bits_per_sample = 64;
        break;
    case 24:
        fmt.codec_id = audio::codec::adpcm_g722;
        fmt.bits_per_sample = 4;
        break;
    case 27:
        fmt.codec_id = audio::codec::alaw;
        fmt.bits_per_sample = 8;
        break;
    default:
        raise(errc::unsupported_format, "unrecognized AU encoding: %" PRIu32,
              head.encoding);
    }

    //auto const bits_per_frame  = fmt.bits_per_sample * fmt.channels;

    fmt.bit_rate          = fmt.bits_per_sample * fmt.sample_rate;
    fmt.bytes_per_packet  = fmt.bits_per_sample * fmt.channels / 8;
    fmt.bytes_per_packet  = std::max(fmt.bytes_per_packet, uint32{1});
    fmt.frames_per_packet = (head.encoding == 24) ? 2 : 1;
    //fmt.frames_per_packet = (fmt.bytes_per_packet * 8) / bits_per_frame;
    return fmt;
}


class demuxer final :
    public audio::basic_demuxer<au::demuxer>
{
    using Base = audio::basic_demuxer<au::demuxer>;
    friend class audio::basic_demuxer<au::demuxer>;

public:
    explicit demuxer(ref_ptr<io::stream>, audio::open_mode);

    void seek(uint64);

    auto get_info(uint32);
    auto get_image(media::image::type);
    auto get_chapter_count() const noexcept;

private:
    bool feed(io::buffer&);

    ref_ptr<io::stream> file;
    uint64 data_beg;
    uint64 data_end;
    uint32 packet_step{1};
};


demuxer::demuxer(ref_ptr<io::stream> s, audio::open_mode const mode) :
    file{std::move(s)}
{
    au::header head;
    file->gather<BE>(head.signature,
                     head.data_start,
                     head.data_length,
                     head.encoding,
                     head.sample_rate,
                     head.channels);

    if (head.signature != ".snd"_4cc) {
        raise(errc::invalid_data_format, "invalid AU file header signature");
    }
    if (!(mode & (audio::playback | audio::metadata))) {
        return;
    }

    data_beg = head.data_start;
    if (head.data_length != ~uint32{0}) {
        data_end = head.data_start + head.data_length;
    }
    else {
        data_end = file->size();
    }

    format = make_codec_format(head);
    Base::resolve_decoder();

    auto const frames = muldiv(data_end - data_beg,
                               format.frames_per_packet,
                               format.bytes_per_packet);

    Base::set_total_frames(frames);
    Base::average_bit_rate = format.bit_rate;
    Base::instant_bit_rate = format.bit_rate;

    packet_step = std::max(format.sample_rate / 10, uint32{1});
    file->seek(data_beg);
}

bool demuxer::feed(io::buffer& dest)
{
    auto const data_pos = file->tell();
    if (data_pos >= data_end) {
        return false;
    }

    auto const limit = data_end - data_pos;
    auto packet_size = packet_step * format.bytes_per_packet;
    if (packet_size > limit) {
        packet_size = static_cast<uint32>(limit);
        auto const unaligned = packet_size % format.bytes_per_packet;
        if (unaligned != 0) {
            if (unaligned == packet_size) {
                return false;
            }
            packet_size -= unaligned;
        }
    }

    dest.assign(*file, packet_size);
    return true;
}

void demuxer::seek(uint64 const pts)
{
    auto const nearest = pts / format.frames_per_packet;
    auto const priming = pts % format.frames_per_packet;
    auto const filepos = data_beg + (nearest * format.bytes_per_packet);

    file->seek(std::min(filepos, data_end));
    Base::set_seek_target_and_offset(pts, priming);
}

auto demuxer::get_info(uint32 const /* chapter_number */)
{
    audio::stream_info info{Base::get_format()};
    info.frames           = Base::total_frames;
    info.codec_id         = Base::format.codec_id;
    info.bits_per_sample  = Base::format.bits_per_sample;
    info.average_bit_rate = Base::average_bit_rate;
    info.props.emplace(tags::container, "AU");
    return info;
}

auto demuxer::get_image(media::image::type)
{
    return media::image{};
}

auto demuxer::get_chapter_count() const noexcept
{
    return uint32{0};
}

AMP_REGISTER_INPUT(demuxer, "au");

}}}   // namespace amp::au::<unnamed>

