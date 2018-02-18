////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/adts.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/demuxer.hpp>
#include <amp/audio/input.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/ape.hpp>
#include <amp/media/id3.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/muldiv.hpp>
#include <amp/stddef.hpp>

#include "mp4_audio.hpp"

#include <algorithm>
#include <utility>
#include <vector>


namespace amp {
namespace adts {
namespace {

class frame_header
{
public:
    frame_header() = default;

    explicit frame_header(io::stream& file)
    {
        file.read(buf_);
        valid_ = ((io::load<uint16,BE>(buf_) & 0xfff6) == 0xfff0)
              && (sample_rate_index() != 0xf)
              && (channel_config() != 0x0)
              && (full_size() >= header_size());
    }

    explicit operator bool() const noexcept
    { return valid_; }

    bool protection_absent() const noexcept
    { return (buf_[1] & 0x1) != 0; }

    uint8 profile() const noexcept
    { return static_cast<uint8>(buf_[2] >> 6); }

    uint8 sample_rate_index() const noexcept
    { return static_cast<uint8>((buf_[2] & 0x3c) >> 2); }

    uint32 sample_rate() const noexcept
    { return mp4::sample_rates[sample_rate_index()]; }

    uint8 channel_config() const noexcept
    { return static_cast<uint8>(((buf_[2] & 0x01) << 2) | (buf_[3] >> 6)); }

    uint8 channels() const noexcept
    { return mp4::channels[channel_config()]; }

    uint32 header_size() const noexcept
    { return protection_absent() ? 7 : 9; }

    uint32 full_size() const noexcept
    { return (io::load<uint32,BE>(&buf_[3]) >> 13) & 0x1fff; }

    uint32 data_size() const noexcept
    { return full_size() - header_size(); }

private:
    uint8 buf_[7];
    bool valid_{};
};


class demuxer :
    public audio::basic_demuxer<adts::demuxer>
{
    using Base = audio::basic_demuxer<adts::demuxer>;
    friend class audio::basic_demuxer<adts::demuxer>;

public:
    explicit demuxer(ref_ptr<io::stream>, audio::open_mode);

    void seek(uint64);

    auto get_info(uint32);
    auto get_image(media::image_type);
    auto get_chapter_count() const noexcept;

private:
    bool feed(io::buffer&);
    adts::frame_header read_frame_header(uint64);

    uint64 id3v2_size{};
    uint64 id3v1_start{io::invalid_pos};
    uint64 apev2_start{io::invalid_pos};
    uint64 data_start{};
    uint64 data_end;
    ref_ptr<io::stream> file;
    std::vector<uint64> seek_table;
};


adts::frame_header demuxer::read_frame_header(uint64 const offset)
{
    if (AMP_LIKELY((offset + 9) < data_end)) {
        return adts::frame_header{*file};
    }
    return adts::frame_header{};
}

demuxer::demuxer(ref_ptr<io::stream> s, audio::open_mode const mode) :
    file{std::move(s)}
{
    if (!(mode & (audio::playback | audio::metadata))) {
        return;
    }
    file->seek(data_start);

    auto offset = data_start;
    auto header = read_frame_header(offset);
    if (AMP_UNLIKELY(!header)) {
        raise(errc::invalid_data_format, "not an ADTS file");
    }


    format.channels = header.channels();
    format.sample_rate = header.sample_rate();
    format.frames_per_packet = 1024;
    format.codec_id = [&]() noexcept -> uint32 {
        switch (header.profile()) {
        case 0x0: return audio::codec::aac_main;
        case 0x1: return audio::codec::aac_lc;
        case 0x2: return audio::codec::aac_ssr;
        case 0x3: return audio::codec::aac_ltp;
        }
        AMP_UNREACHABLE();
    }();

    if (format.sample_rate <= 24000) {
        format.sample_rate *= 2;
        format.frames_per_packet *= 2;
        if (format.channels == 1) {
            format.channels = 2;
            format.codec_id = audio::codec::he_aac_v2;
        }
        else {
            format.codec_id = audio::codec::he_aac_v1;
        }
    }
    format.channel_layout = audio::aac_channel_layout(format.channels);
    Base::resolve_decoder();

    do {
        seek_table.push_back(offset);
        offset += header.full_size();
        file->seek(offset);
    }
    while ((header = read_frame_header(offset)));
    file->seek(data_start);

    Base::set_total_frames(format.frames_per_packet * seek_table.size());
    average_bit_rate = static_cast<uint32>(muldiv(data_end - data_start,
                                                  format.sample_rate * 8,
                                                  total_frames));
}

bool demuxer::feed(io::buffer& dest)
{
    auto const header = read_frame_header(file->tell());
    if (AMP_UNLIKELY(!header)) {
        return false;
    }

    if (!header.protection_absent()) {
        file->skip(2);
    }

    auto const bytes = header.data_size();
    dest.assign(*file, bytes);
    instant_bit_rate = muldiv(bytes,
                              format.sample_rate * 8,
                              format.frames_per_packet);
    return true;
}

void demuxer::seek(uint64 const pts)
{
    auto nearest = pts / format.frames_per_packet;
    auto priming = pts % format.frames_per_packet;

    if (nearest >= seek_table.size()) {
        nearest  = seek_table.size() - 1;
        priming  = 0;
    }
    else {
        auto const preroll = std::min(nearest, uint64{10});
        nearest -= preroll;
        priming += preroll * format.frames_per_packet;
    }

    file->seek(seek_table[nearest]);
    Base::set_seek_target_and_offset(pts, priming);
}

auto demuxer::get_info(uint32 const /* chapter_number */)
{
    audio::stream_info info{Base::get_format()};
    info.frames           = Base::total_frames;
    info.codec_id         = Base::format.codec_id;
    info.average_bit_rate = Base::average_bit_rate;
    info.props.emplace(tags::container, "ADTS");

    if (id3v2_size != 0) {
        file->rewind();
        id3v2::read(*file, info.tags);
    }
    else if (apev2_start != io::invalid_pos) {
        file->seek(apev2_start);
        ape::read(*file, info.tags);
    }
    else if (id3v1_start != io::invalid_pos) {
        file->seek(id3v1_start);
        id3v1::read(*file, info.tags);
    }
    return info;
}

auto demuxer::get_image(media::image_type const type)
{
    if (id3v2_size != 0) {
        file->rewind();
        return id3v2::find_image(*file, type);
    }
    if (apev2_start != io::invalid_pos) {
        file->seek(apev2_start);
        return ape::find_image(*file, type);
    }
    return media::image{};
}

auto demuxer::get_chapter_count() const noexcept
{
    return uint32{0};
}

AMP_REGISTER_INPUT(demuxer, "aac", "aacp", "adts");

}}}   // namespace amp::adts::<unnamed>

