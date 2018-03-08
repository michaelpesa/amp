////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/tta.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/decoder.hpp>
#include <amp/audio/demuxer.hpp>
#include <amp/audio/input.hpp>
#include <amp/crc.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/memory.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/net/endian.hpp>
#include <amp/numeric.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <numeric>
#include <utility>
#include <vector>


namespace amp {
namespace tta {
namespace {

inline uint32 channel_layout(uint32 const channels) noexcept
{
    switch (channels) {
    case 1: return audio::channel_layout_mono;
    case 2: return audio::channel_layout_stereo;
    case 3: return audio::channel_layout_2_1;
    case 4: return audio::channel_layout_quad;
    case 6: return audio::channel_layout_5_1;
    case 7: return audio::channel_layout_6_1;
    case 8: return audio::channel_layout_7_1_front;
    }
    return 0;
}


class demuxer final :
    public audio::basic_demuxer<tta::demuxer>
{
    using Base = audio::basic_demuxer<tta::demuxer>;
    friend class audio::basic_demuxer<tta::demuxer>;

public:
    explicit demuxer(ref_ptr<io::stream>, audio::open_mode);

    void seek(uint64);

    auto get_info(uint32);
    auto get_image(media::image::type);
    auto get_chapter_count() const noexcept;

private:
    bool feed(io::buffer&);

    ref_ptr<io::stream> file;
    std::vector<uint32> seek_table;
    uint64              id3v2_end{};
    uint64              id3v1_start{io::invalid_pos};
    uint64              apev2_start{io::invalid_pos};
    uint64              data_start{};
    uint32              packet_number{};
};


struct header
{
    uint8  signature[4];
    uint16 flags;
    uint16 channels;
    uint16 bits_per_sample;
    uint32 sample_rate;
    uint32 frames;
    uint32 crc;
};


demuxer::demuxer(ref_ptr<io::stream> s, audio::open_mode const mode) :
    file{std::move(s)}
{
    if (ape::find(*file)) {
        apev2_start = file->tell();
    }
    else if (id3v1::find(*file)) {
        id3v1_start = file->tell();
    }

    file->rewind();
    if (id3v2::skip(*file)) {
        id3v2_end = file->tell();
    }

    format.extra.assign(*file, 22);

    tta::header head;
    io::gather<LE>(format.extra.data(),
                   head.signature,
                   head.flags,
                   head.channels,
                   head.bits_per_sample,
                   head.sample_rate,
                   head.frames,
                   head.crc);

    if (io::load<uint32,BE>(head.signature) != "TTA1"_4cc) {
        raise(errc::invalid_data_format,
              "invalid TTA file header signature");
    }
    if (head.flags != 0x1 && head.flags != 0x2) {
        raise(errc::invalid_data_format,
              "unsupported TTA format flags: 0x%04" PRIx16, head.flags);
    }
    if (head.channels == 0 || head.channels > 18) {
        raise(errc::unsupported_format,
              "invalid TTA channel count: %" PRIu32, head.channels);
    }
    if (head.bits_per_sample == 0) {
        raise(errc::unsupported_format,
              "TTA bits per sample is zero");
    }
    if (head.sample_rate < 8000 || head.sample_rate > 384000) {
        raise(errc::unsupported_format,
              "invalid TTA sample rate: %" PRIu32, head.sample_rate);
    }
    if (head.frames == 0) {
        raise(errc::failure, "TTA file contains zero frames");
    }

    auto crc = crc32::compute(format.extra.data(), format.extra.size() - 4);
    if (crc != head.crc) {
        raise(errc::failure, "TTA file header CRC mismatch");
    }

    if (!(mode & (audio::playback | audio::metadata))) {
        return;
    }

    format.codec_id          = audio::codec::tta;
    format.channels          = head.channels;
    format.bits_per_sample   = head.bits_per_sample;
    format.sample_rate       = head.sample_rate;
    format.channel_layout    = tta::channel_layout(format.channels);
    format.frames_per_packet = (format.sample_rate * 256) / 245;
    Base::resolve_decoder();

    auto const packet_count = (head.frames / format.frames_per_packet)
                            + (head.frames % format.frames_per_packet != 0);

    seek_table.resize(packet_count);
    file->read(reinterpret_cast<uchar*>(seek_table.data()),
               packet_count * sizeof(uint32));

    crc = crc32::compute(seek_table.data(), packet_count * sizeof(uint32));
    auto const seek_table_crc = file->read<uint32,LE>();
    if (crc != seek_table_crc) {
        raise(errc::failure, "TTA seek table CRC mismatch");
    }

    std::transform(seek_table.begin(), seek_table.end(),
                   seek_table.begin(), net::to_host<LE>);

    data_start = file->tell();
    auto const data_end = (apev2_start != io::invalid_pos) ? apev2_start
                        : (id3v1_start != io::invalid_pos) ? id3v1_start
                        :                                    file->size();
    average_bit_rate = static_cast<uint32>(muldiv(data_end - data_start,
                                                  head.sample_rate * 8,
                                                  head.frames));
    Base::set_total_frames(head.frames);
}

bool demuxer::feed(io::buffer& dest)
{
    if (AMP_UNLIKELY(packet_number >= seek_table.size())) {
        return false;
    }

    auto const packet_size = seek_table[packet_number++];
    dest.assign(*file, packet_size);
    instant_bit_rate = muldiv(packet_size,
                              format.sample_rate * 8,
                              format.frames_per_packet);
    return true;
}

void demuxer::seek(uint64 const pts)
{
    auto nearest = static_cast<uint32>(pts / format.frames_per_packet);
    auto priming = static_cast<uint32>(pts % format.frames_per_packet);

    if (nearest >= seek_table.size()) {
        nearest = static_cast<uint32>(seek_table.size());
        priming = 0;
    }

    auto const data_offset = std::accumulate(
        seek_table.cbegin(),
        seek_table.cbegin() + static_cast<std::ptrdiff_t>(nearest),
        data_start);

    file->seek(data_offset);
    packet_number = nearest;
    Base::set_seek_target_and_offset(pts, priming);
}

auto demuxer::get_info(uint32 const /* chapter_number */)
{
    audio::stream_info info{Base::get_format()};
    info.frames           = Base::total_frames;
    info.codec_id         = Base::format.codec_id;
    info.bits_per_sample  = Base::format.bits_per_sample;
    info.average_bit_rate = Base::average_bit_rate;

    if (apev2_start != io::invalid_pos) {
        file->seek(apev2_start);
        ape::read(*file, info.tags);
    }
    else if (id3v2_end != 0) {
        file->rewind();
        id3v2::read(*file, info.tags);
    }
    else if (id3v1_start != io::invalid_pos) {
        file->seek(id3v1_start);
        id3v1::read(*file, info.tags);
    }
    return info;
}

auto demuxer::get_image(media::image::type const type)
{
    if (apev2_start != io::invalid_pos) {
        file->seek(apev2_start);
        return ape::find_image(*file, type);
    }
    if (id3v2_end != 0) {
        file->rewind();
        return id3v2::find_image(*file, type);
    }
    return media::image{};
}

auto demuxer::get_chapter_count() const noexcept
{
    return uint32{0};
}

AMP_REGISTER_INPUT(demuxer, "tta");

}}}   // namespace amp::tta::<unnamed>

