////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/mac.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/demuxer.hpp>
#include <amp/audio/input.hpp>
#include <amp/audio/packet.hpp>
#include <amp/bitops.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/memory.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/tags.hpp>
#include <amp/numeric.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include <algorithm>
#include <cinttypes>
#include <memory>
#include <utility>


namespace amp {
namespace mac {
namespace {

enum : uint32 {
    format_flag_8_bit             = 1 << 0,
    format_flag_crc32             = 1 << 1,
    format_flag_has_peak_level    = 1 << 2,
    format_flag_24_bit            = 1 << 3,
    format_flag_has_seek_elements = 1 << 4,
    format_flag_create_wav_header = 1 << 5,
};

struct descriptor
{
    uint8  signature[4];
    uint16 version;
    uint16 padding;
    uint32 descriptor_size;
    uint32 ape_header_size;
    uint32 seek_table_size;
    uint32 wav_header_size;
    uint64 audio_data_size;
    uint32 trail_data_size;
    uint8  md5[16];
};

struct header
{
    uint16 compression_level;
    uint16 format_flags;
    uint32 blocks_per_frame;
    uint32 final_frame_blocks;
    uint32 total_frames;
    uint16 bits_per_sample;
    uint16 channels;
    uint32 sample_rate;
};

struct frame
{
    uint64 pos;
    uint32 size;
    uint32 skip;
};


class demuxer final :
    public audio::basic_demuxer<mac::demuxer>
{
    using Base = audio::basic_demuxer<mac::demuxer>;
    friend class audio::basic_demuxer<mac::demuxer>;

public:
    explicit demuxer(ref_ptr<io::stream>, audio::open_mode);

    void seek(uint64);

    auto get_info(uint32);
    auto get_image(media::image::type);
    auto get_chapter_count() const noexcept;

private:
    bool feed(io::buffer&);

    ref_ptr<io::stream> const file;
    std::unique_ptr<mac::frame[]> frames;
    uint64 id3v1_start{io::invalid_pos};
    uint64 apev2_start{io::invalid_pos};
    uint32 current_frame{0};
    mac::descriptor desc;
    mac::header head;
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

    if (!(mode & (audio::playback | audio::metadata))) {
        return;
    }

    file->rewind();
    file->gather<LE>(desc.signature, desc.version);

    if (io::load<uint32,BE>(desc.signature) != "MAC "_4cc) {
        raise(errc::invalid_data_format, "not a Monkey's Audio file");
    }
    if (desc.version < 3800 || desc.version > 3990) {
        raise(errc::not_implemented,
              "unsupported Monkey's Audio file version: %d.%02d",
              (desc.version / 1000),
              (desc.version % 1000) / 10);
    }

    if (desc.version >= 3980) {
        file->gather<LE>(desc.padding,
                         desc.descriptor_size,
                         desc.ape_header_size,
                         desc.seek_table_size,
                         desc.wav_header_size,
                         desc.audio_data_size,
                         desc.trail_data_size,
                         desc.md5);

        if (desc.descriptor_size > 52) {
            file->skip(desc.descriptor_size - 52);
        }

        file->gather<LE>(head.compression_level,
                         head.format_flags,
                         head.blocks_per_frame,
                         head.final_frame_blocks,
                         head.total_frames,
                         head.bits_per_sample,
                         head.channels,
                         head.sample_rate);
    }
    else {
        desc.descriptor_size = 6;
        desc.ape_header_size = 26;

        file->gather<LE>(head.compression_level,
                         head.format_flags,
                         head.channels,
                         head.sample_rate,
                         desc.wav_header_size,
                         desc.trail_data_size,
                         head.total_frames,
                         head.final_frame_blocks);

        if (head.format_flags & mac::format_flag_has_peak_level) {
            file->skip(4);
            desc.ape_header_size += 4;
        }

        if (head.format_flags & mac::format_flag_has_seek_elements) {
            desc.seek_table_size = file->read<uint32,LE>();
            desc.ape_header_size += 4;
        }
        else {
            desc.seek_table_size = head.total_frames;
        }
        desc.seek_table_size *= 4;

        if (head.format_flags & mac::format_flag_8_bit) {
            head.bits_per_sample = 8;
        }
        else if (head.format_flags & mac::format_flag_24_bit) {
            head.bits_per_sample = 24;
        }
        else {
            head.bits_per_sample = 16;
        }

        if (desc.version >= 3950) {
            head.blocks_per_frame = 73728 * 4;
        }
        else if (desc.version >= 3900) {
            head.blocks_per_frame = 73728;
        }
        else if (desc.version >= 3800 && head.compression_level >= 4000) {
            head.blocks_per_frame = 73728;
        }
        else {
            head.blocks_per_frame = 9216;
        }

        if (!(head.format_flags & mac::format_flag_create_wav_header)) {
            file->skip(desc.wav_header_size);
        }
    }

    if (head.total_frames == 0) {
        raise(errc::failure, "Monkey's Audio file contains zero frames");
    }
    if ((desc.seek_table_size / 4) < head.total_frames) {
        raise(errc::invalid_data_format,
              "seek table size (%" PRIu32 ") is less than the "
              "total frame count (%" PRIu32 ")",
              desc.seek_table_size / 4, head.total_frames);
    }

    auto data_start = desc.descriptor_size
                    + desc.ape_header_size
                    + desc.seek_table_size
                    + desc.wav_header_size;

    if (desc.version < 3810) {
        data_start += head.total_frames;
    }

    auto const file_size = file->size();
    auto data_end = (apev2_start != io::invalid_pos) ? apev2_start
                  : (id3v1_start != io::invalid_pos) ? id3v1_start
                  :                                    file_size;
    data_end -= desc.trail_data_size;

    frames.reset(new mac::frame[head.total_frames]);
    frames[0].pos  = data_start;
    frames[0].skip = 0;

    io::buffer seek_table{*file, desc.seek_table_size};
    for (auto const i : xrange(uint32{1}, head.total_frames)) {
        auto const pos     = io::load<uint32,LE>(&seek_table[i * 4]);
        frames[i].pos      = pos;
        frames[i].skip     = static_cast<uint32>(pos - data_start) & 3;
        frames[i - 1].size = static_cast<uint32>(pos - frames[i - 1].pos);
    }

    auto&& last_frame = frames[head.total_frames - 1];
    last_frame.size = static_cast<uint32>(data_end - last_frame.pos);

    for (auto const i : xrange(head.total_frames)) {
        if (frames[i].skip) {
            frames[i].pos  -= frames[i].skip;
            frames[i].size += frames[i].skip;
        }
        frames[i].size = align_up(frames[i].size, 4);
    }

    if ((last_frame.pos + last_frame.size) > file_size) {
        last_frame.size = static_cast<uint32>(file_size - last_frame.pos);
    }

    if (desc.version < 3810) {
        seek_table.assign(*file, head.total_frames);
        for (auto const i : xrange(head.total_frames)) {
            if ((i < (head.total_frames - 1)) && seek_table[i + 1]) {
                frames[i].size += 4;
            }
            frames[i].skip <<= 3;
            frames[i].skip  += seek_table[i];
        }
    }

    format.extra.resize(6, uninitialized);
    io::scatter<LE>(format.extra.data(),
                    uint16{desc.version},
                    uint16{head.compression_level},
                    uint16{head.format_flags});

    format.codec_id          = audio::codec::monkeys_audio;
    format.channel_layout    = audio::guess_channel_layout(head.channels);
    format.channels          = head.channels;
    format.sample_rate       = head.sample_rate;
    format.bits_per_sample   = head.bits_per_sample;
    format.frames_per_packet = head.blocks_per_frame;
    Base::resolve_decoder();

    auto const total_blocks = head.blocks_per_frame * (head.total_frames - 1)
                            + head.final_frame_blocks;
    Base::set_total_frames(total_blocks);

    average_bit_rate = static_cast<uint32>(muldiv(data_end - data_start,
                                                  format.sample_rate * 8,
                                                  total_blocks));
}

bool demuxer::feed(io::buffer& dest)
{
    if (current_frame >= head.total_frames) {
        return false;
    }

    auto&& frame = frames[current_frame];
    file->seek(frame.pos);

    auto const blocks = ((current_frame + 1) == head.total_frames)
                      ? head.final_frame_blocks
                      : head.blocks_per_frame;

    dest.resize(frame.size + 8, uninitialized);
    io::scatter<LE>(dest.data(),
                    uint32{blocks},
                    uint32{frame.skip});

    file->read(&dest[8], frame.size);

    current_frame += 1;
    instant_bit_rate = muldiv(frame.size, format.sample_rate * 8, blocks);
    return true;
}

void demuxer::seek(uint64 pts)
{
    pts = std::min(pts, Base::total_frames);

    auto const nearest = pts / format.frames_per_packet;
    auto const priming = pts % format.frames_per_packet;

    file->seek(frames[nearest].pos);
    current_frame = static_cast<uint32>(nearest);
    Base::set_seek_target_and_offset(pts, priming);
}

auto demuxer::get_info(uint32 const /* chapter_number */)
{
    audio::stream_info info{Base::get_format()};
    info.frames           = Base::total_frames;
    info.codec_id         = Base::format.codec_id;
    info.bits_per_sample  = Base::format.bits_per_sample;
    info.average_bit_rate = Base::average_bit_rate;

    info.props.emplace(tags::container,
                       u8format("Monkey's Audio %d.%02d",
                                (desc.version / 1000),
                                (desc.version % 1000) / 10));

    auto const compression_level = [&]() -> char const* {
        switch (head.compression_level) {
        case 1000: return "Fast";
        case 2000: return "Normal";
        case 3000: return "High";
        case 4000: return "Extra high";
        case 5000: return "Insane";
        }
        return nullptr;
    }();

    if (compression_level != nullptr) {
        info.props.emplace(tags::codec_profile,
                           u8format("%s compression", compression_level));
    }

    if (apev2_start != io::invalid_pos) {
        file->seek(apev2_start);
        ape::read(*file, info.tags);
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
    return media::image{};
}

auto demuxer::get_chapter_count() const noexcept
{
    return uint32{0};
}

AMP_REGISTER_INPUT(demuxer, "ape");

}}}   // namespace amp::mac::<unnamed>

