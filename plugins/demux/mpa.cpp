////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/mpa.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/demuxer.hpp>
#include <amp/audio/input.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/reader.hpp>
#include <amp/io/memory.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/ape.hpp>
#include <amp/media/dictionary.hpp>
#include <amp/media/id3.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/muldiv.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include <algorithm>
#include <cinttypes>
#include <utility>
#include <vector>


namespace amp {
namespace mpa {
namespace {

struct header
{
    explicit operator bool() const noexcept
    { return (data != 0); }

    uint8 version() const noexcept
    { return static_cast<uint8>((data >> 19) & 0x3); }

    bool lsf() const noexcept
    { return (data & (0x1 << 19)) == 0; }

    bool v25() const noexcept
    { return (data & (0x1 << 20)) == 0; }

    uint8 layer() const noexcept
    { return static_cast<uint8>(4 - ((data >> 17) & 0x3)); }

    uint8 bit_rate_index() const noexcept
    { return static_cast<uint8>((data >> 12) & 0xf); }

    uint8 sample_rate_index() const noexcept
    { return static_cast<uint8>((data >> 10) & 0x3); }

    bool padding_bit() const noexcept
    { return (data & 0x00000200) != 0; }

    uint8 mode() const noexcept
    { return static_cast<uint8>((data >> 6) & 0x3); }

    uint32 bit_rate() const noexcept
    {
        static constexpr uint8 table[][3][16] {
            {   // MPEG-1
                {0,  4,  8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56},
                {0,  4,  6,  7,  8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48},
                {0,  4,  5,  6,  7,  8, 10, 12, 14, 16, 20, 24, 28, 32, 40},
            },
            {   // MPEG-2(.5)
                {0,  4,  6,  7,  8, 10, 12, 14, 16, 18, 20, 22, 24, 28, 32},
                {0,  1,  2,  3,  4,  5,  6,  7,  8, 10, 12, 14, 16, 18, 20},
                {0,  1,  2,  3,  4,  5,  6,  7,  8, 10, 12, 14, 16, 18, 20},
            },
        };

        return table[lsf()][layer() - 1][bit_rate_index()] * uint32{8000};
    }

    uint32 sample_rate() const noexcept
    {
        static constexpr uint16 table[] { 44100, 48000, 32000, };

        auto rate = uint32{table[sample_rate_index()]};
        rate >>= lsf();
        rate >>= v25();
        return rate;
    }

    uint32 channels() const noexcept
    {
        return (mode() == 3) ? 1 : 2;
    }

    uint16 samples_per_frame() const noexcept
    {
        static constexpr uint16 table[][3] {
            {384, 1152, 1152},
            {384, 1152,  576},
        };
        return table[lsf()][layer() - 1];
    }

    uint32 frame_size() const noexcept
    {
        auto const num = bit_rate();
        auto const den = sample_rate();
        auto const pad = padding_bit();

        switch (layer()) {
        case 3:
            if (lsf()) {
                return 72 * num / den + pad;
            }
            [[fallthrough]];
        case 2:
            return 144 * num / den + pad;
        case 1:
            return (12 * num / den + pad) << 2;
        }
        AMP_UNREACHABLE();
    }

    uint8 side_info_size() const noexcept
    {
        return !lsf()
             ? (channels() == 1 ? uint8{0x11} : uint8{0x20})
             : (channels() == 1 ? uint8{0x09} : uint8{0x11});
    }

    bool valid() const noexcept
    {
        return ((data >> 21) & 0x7ff) == 0x7ff         // sync word
            && ((data >> 19) & 0x003) != 0x001         // MPEG version
            && ((data >> 17) & 0x003) != 0x000         // MPEG audio layer
            && ((data >> 12) & 0x00f) != 0x00f         // bit rate index
            && ((data >> 10) & 0x003) != 0x003         // sample rate index
            && ((data >>  0) & 0x003) != 0x002;        // emphasis
    }

    uint32 data;
};


struct sync_info
{
    uint32 priming;
    uint32 padding;
    uint64 frames;
};


class demuxer final :
    public audio::basic_demuxer<mpa::demuxer>
{
    using Base = audio::basic_demuxer<mpa::demuxer>;
    friend class audio::basic_demuxer<mpa::demuxer>;

public:
    explicit demuxer(ref_ptr<io::stream>, audio::open_mode);

    void seek(uint64);

    auto get_info(uint32);
    auto get_image(media::image_type);
    auto get_chapter_count() const noexcept;

private:
    bool feed(io::buffer&);
    auto read_frame_header(uint64&, uint32&) noexcept;

    bool find_vbr_tags(mpa::header, mpa::sync_info&);
    void find_iTunSMPB(mpa::sync_info&);
    void fill_seek_table();

    uint64 id3v2_size{};
    uint64 id3v1_start{io::invalid_pos};
    uint64 apev2_start{io::invalid_pos};
    uint64 data_start{};
    uint64 data_end;
    ref_ptr<io::stream> file;
    std::vector<uint64> seek_table;
    uint32 packet_number{};
    uint8 mpeg_version;

    struct {
        uint32 vbr_scale{};
        uint8 vbr_method{};
        char version[10]{};
    } xing;
};


auto demuxer::read_frame_header(uint64& offset, uint32& length) noexcept
{
    offset = file->tell() + 4;
    if (AMP_UNLIKELY(offset >= data_end)) {
        return mpa::header{};
    }

    mpa::header head{file->read<uint32,BE>()};
    if (AMP_UNLIKELY(!head.valid())) {
        do {
            do {
                if (AMP_UNLIKELY(++offset == data_end)) {
                    return mpa::header{};
                }
                head.data = (head.data << 8) | file->read<uint8>();
            }
            while (((head.data >> 21) & 0x7ff) != 0x7ff);
        }
        while (!head.valid());
    }

    length = head.frame_size();
    offset -= 4;
    if (AMP_LIKELY((offset + length) <= data_end)) {
        return head;
    }
    return mpa::header{};
}

void demuxer::fill_seek_table()
{
    uint64 offset;
    uint32 length;

    file->seek(data_start);
    while (read_frame_header(offset, length)) {
        seek_table.push_back(offset);
        file->seek(offset + length);
    }
}

bool demuxer::find_vbr_tags(mpa::header const head, mpa::sync_info& sync)
{
    auto const xing_offset = uint32{head.side_info_size()} + 4;
    auto const vbri_offset = uint32{0x20} + 4;

    auto xing_tag = uint32{};
    auto vbri_tag = uint32{};

    file->seek(data_start);
    io::buffer buf{*file, head.frame_size()};
    io::reader r{buf};

    if (r.remain() >= (xing_offset + 4 + 4)) {
        xing_tag = io::load<uint32,BE>(r.peek() + xing_offset);
    }
    if (r.remain() >= (vbri_offset + 4 + 2)) {
        vbri_tag = io::load<uint32,BE>(r.peek() + vbri_offset);
    }

    switch (xing_tag) {
    case "Info"_4cc:
        format.bit_rate = head.bit_rate();
        [[fallthrough]];
    case "Xing"_4cc:
        r.skip_unchecked(xing_offset + 4);
        auto const flags = r.read_unchecked<uint32,BE>();

        if (flags & 0x1) {
            sync.frames = r.read<uint32,BE>() * format.frames_per_packet;
        }
        if (flags & 0x2) {
            r.skip(4);
        }
        if (flags & 0x4) {
            r.skip(100);
        }
        if (flags & 0x8) {
            xing.vbr_scale = r.read<uint32,BE>();
        }

        if (r.remain() >= 36) {
            r.read_unchecked(xing.version, 9);
            xing.vbr_method = r.read_unchecked<uint8>() & 0xf;
            r.skip_unchecked(11);

            auto const delay_and_padding = r.read_unchecked<uint32,BE>();
            sync.priming = (delay_and_padding >> 20) & 0xfff;
            sync.padding = (delay_and_padding >>  8) & 0xfff;

            if (sync.frames >= sync.priming) {
                sync.frames -= sync.priming;
            }
            if (sync.frames >= sync.padding) {
                sync.frames -= sync.padding;
            }
        }
        return true;
    }

    if (vbri_tag == "VBRI"_4cc) {
        r.skip_unchecked(vbri_offset + 4);

        auto const vbri_version = r.read_unchecked<uint16,BE>();
        if (vbri_version == 1 && r.remain() >= 10) {
            uint16 delay, quality;
            uint32 bytes, frames;
            r.gather_unchecked<BE>(delay, quality, bytes, frames);

            sync.frames  = frames * format.frames_per_packet;
            sync.priming = delay;

            if (sync.frames >= sync.priming) {
                sync.frames -= sync.priming;
            }
        }
        return true;
    }
    return false;
}

void demuxer::find_iTunSMPB(mpa::sync_info& sync)
{
    file->rewind();
    media::dictionary tags;
    id3v2::read(*file, tags);

    auto found = tags.find("comment:iTunSMPB");
    if (found != tags.end()) {
        uint32 priming, padding;
        uint64 frames;
        auto const ret = std::sscanf(found->second.c_str(),
                                     "%*8" SCNx32 " %8"  SCNx32
                                     " %8" SCNx32 " %16" SCNx64,
                                     &priming, &padding, &frames);
        if (ret == 3) {
            sync.priming = priming;
            sync.padding = padding;
            sync.frames = frames;
        }
    }
}


demuxer::demuxer(ref_ptr<io::stream> s, audio::open_mode const mode) :
    file{std::move(s)}
{
    if (id3v2::skip(*file)) {
        data_start = id3v2_size = file->tell();
    }

    if (ape::find(*file)) {
        data_end = apev2_start = file->tell();
    }
    else if (id3v1::find(*file)) {
        data_end = id3v1_start = file->tell();
    }
    else {
        data_end = file->size();
    }

    if (!(mode & (audio::playback | audio::metadata))) {
        return;
    }
    file->seek(data_start);

    uint64 unused1;
    uint32 unused2;
    auto const head = read_frame_header(unused1, unused2);
    if (AMP_UNLIKELY(!head)) {
        raise(errc::invalid_data_format,
              "no valid MPEG audio frame header(s) found");
    }

    mpeg_version             = head.version();
    format.sample_rate       = head.sample_rate();
    format.channels          = head.channels();
    format.channel_layout    = audio::guess_channel_layout(format.channels);
    format.frames_per_packet = head.samples_per_frame();
    format.codec_id          = head.layer() == 3 ? audio::codec::mpeg_layer3
                             : head.layer() == 2 ? audio::codec::mpeg_layer2
                             :                     audio::codec::mpeg_layer1;
    Base::resolve_decoder();

    mpa::sync_info sync{};
    if (find_vbr_tags(head, sync)) {
        data_start += head.frame_size();
    }
    if (sync.priming == 0 && id3v2_size != 0) {
        find_iTunSMPB(sync);
    }

    if (sync.frames != 0) {
        seek_table.reserve(sync.frames / format.frames_per_packet);
        seek_table.push_back(data_start);
    }
    else {
        fill_seek_table();
        sync.frames = seek_table.size() * format.frames_per_packet;
    }

    Base::set_total_frames(sync.frames);
    Base::set_encoder_delay(sync.priming);
    file->seek(data_start);

    average_bit_rate = format.bit_rate;
    if (average_bit_rate == 0) {
        average_bit_rate = static_cast<uint32>(muldiv(data_end - data_start,
                                                      format.sample_rate * 8,
                                                      total_frames));
    }
}

bool demuxer::feed(io::buffer& dest)
{
    uint64 offset;
    uint32 length;

    auto const head = read_frame_header(offset, length);
    if (AMP_UNLIKELY(!head)) {
        return false;
    }

    dest.resize(length, uninitialized);
    io::store<BE>(&dest[0], head.data);
    file->read(&dest[4], length - 4);

    AMP_ASSERT(seek_table.size() >= packet_number);
    if (seek_table.size() == packet_number) {
        seek_table.push_back(offset);
    }

    ++packet_number;
    instant_bit_rate = head.bit_rate();
    return true;
}

void demuxer::seek(uint64 const pts)
{
    auto nearest = static_cast<uint32>(pts / format.frames_per_packet);
    auto priming = static_cast<uint32>(pts % format.frames_per_packet);

    auto const preroll = std::min(nearest, uint32{10});
    nearest -= preroll;
    priming += preroll * format.frames_per_packet;

    AMP_ASSERT(!seek_table.empty());

    if (nearest >= seek_table.size()) {
        file->seek(seek_table.back());
        do {
            uint64 offset;
            uint32 length;

            if (!read_frame_header(offset, length)) {
                nearest = static_cast<uint32>(seek_table.size() - 1);
                priming = 0;
                break;
            }

            if (seek_table.back() != offset) {
                seek_table.push_back(offset);
            }
            file->seek(offset + length);
        }
        while (nearest >= seek_table.size());
    }

    packet_number = nearest;
    file->seek(seek_table[packet_number]);
    Base::set_seek_target_and_offset(pts, priming);
}

auto demuxer::get_info(uint32 const /* chapter_number */)
{
    audio::stream_info info{Base::get_format()};
    info.frames           = Base::total_frames;
    info.codec_id         = Base::format.codec_id;
    info.average_bit_rate = Base::average_bit_rate;
    info.props.emplace(tags::container,
                       (  mpeg_version == 0b11 ? "MPEG 1"
                        : mpeg_version == 0b10 ? "MPEG 2"
                        :                        "MPEG 2.5"));

    if (io::load<uint32,BE>(xing.version) == "LAME"_4cc) {
        auto profile = [&] {
            if (xing.vbr_method == 1 || xing.vbr_method == 8) {
                return u8string::from_utf8_unchecked("CBR");
            }
            if (xing.vbr_method == 2 || xing.vbr_method == 9) {
                return u8string::from_utf8_unchecked("ABR");
            }
            if (xing.vbr_method >= 3 && xing.vbr_method <= 6) {
                //auto const V = (100 - xing.vbr_scale) / 10;
                //auto const q = (100 - xing.vbr_scale) % 10;
                return u8format("VBR %d", (100 - xing.vbr_scale) / 10);
            }
            return u8string{};
        }();
        info.props.emplace(tags::codec_profile, std::move(profile));
        info.props.emplace(tags::encoder,       xing.version);
    }

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

AMP_REGISTER_INPUT(demuxer, "m1a", "m2a", "mp1", "mp2", "mp3", "mpa");

}}}   // namespace amp::mpa::<unnamed>

