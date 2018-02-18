////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/real.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/demuxer.hpp>
#include <amp/audio/input.hpp>
#include <amp/cxp/string.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/memory.hpp>
#include <amp/io/stream.hpp>
#include <amp/media/image.hpp>
#include <amp/media/tags.hpp>
#include <amp/muldiv.hpp>
#include <amp/net/endian.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/string_view.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include <algorithm>
#include <cinttypes>
#include <memory>
#include <utility>
#include <vector>


namespace amp {
namespace real {
namespace {

struct audio_specific_data
{
    uint16 version;
    uint16 bytes_per_minute;
    uint32 data_size;
    uint16 flavor;
    uint32 coded_frame_size;
    uint16 sub_packet_h;
    uint16 frame_size;
    uint16 sub_packet_size;
    uint16 sample_rate;
    uint16 sample_size;
    uint16 channels;
    uint32 deint_id;
    uint32 codec_id;
};


class splitter
{
public:
    void reset(real::audio_specific_data const& head)
    {
        type = [&] {
            switch (head.deint_id) {
            case "Int0"_4cc: return type::int0;
            case "Int4"_4cc: return type::int4;
            case "genr"_4cc: return type::genr;
            case "sipr"_4cc: return type::sipr;
            case "vbrs"_4cc: return type::vbrs;
            case "vbrf"_4cc: return type::vbrs;
            }
            raise(errc::invalid_data_format,
                  "invalid RealAudio deinterleaver ID");
        }();

        switch (type) {
        case type::vbrs:
            return;
        case type::int0:
            dnet = (head.codec_id == "dnet"_4cc);
            return;
        case type::int4:
            frame_size      = head.frame_size;
            sub_packet_size = head.coded_frame_size;
            width           = head.sub_packet_h / 2;
            break;
        case type::genr:
            sub_packet_size = head.sub_packet_size;
            width           = head.frame_size / sub_packet_size;
            break;
        case type::sipr:
            sub_packet_size = head.frame_size;
            width           = 1;
            break;
        }

        height         = head.sub_packet_h;
        sub_packet_pos = width * height;

        auto&& sub_packets = sub_packet_array[0];
        sub_packets.resize(sub_packet_pos * sub_packet_size, uninitialized);
    }

    void flush() noexcept
    {
        switch (type) {
        case type::int0:
            sub_packet_ready = false;
            break;
        case type::int4:
        case type::genr:
        case type::sipr:
            sub_packet_pos = width * height;
            sub_packet_row = 0;
            break;
        case type::vbrs:
            sub_packet_pos   = 0;
            sub_packet_count = 0;
            break;
        }
    }

    void send(io::stream& file, uint32 const size)
    {
        switch (type) {
        case type::int0:
            {
                auto&& sub_packets = sub_packet_array[0];
                sub_packets.assign(file, size);
                if (dnet) {
                    for (auto i = 0_sz; i != size; i += 2) {
                        std::swap(sub_packets[i+0], sub_packets[i+1]);
                    }
                }
                sub_packet_ready = true;
                break;
            }
        case type::int4:
        case type::genr:
        case type::sipr:
            {
                if (AMP_UNLIKELY(size != (sub_packet_size * width))) {
                    raise(errc::failure,
                          "expected packet size: %" PRIu32
                          ", actual packet size: %" PRIu32,
                          sub_packet_size * width, size);
                }

                auto const h   = height;
                auto const y   = sub_packet_row++;
                auto const sps = sub_packet_size;

                auto&& sub_packets = sub_packet_array[0];
                for (auto const x : xrange(width)) {
                    uint32 index;
                    if (type == type::int4) {
                        index = y + (x * 2 * frame_size / sps);
                    }
                    else if (type == type::genr) {
                        index = h * x + ((h + 1) / 2) * (y & 1) + (y >> 1);
                    }
                    else {
                        index = y;
                    }
                    file.read(&sub_packets[index * sps], sps);
                }

                if (sub_packet_row == height) {
                    sub_packet_row = 0;
                    sub_packet_pos = 0;
                    if (type == type::sipr) {
                        reorder_sipr_sub_packets();
                    }
                }
                break;
            }
        case type::vbrs:
            {
                sub_packet_count = file.read<uint16,BE>();
                sub_packet_count = (sub_packet_count & 0xf0) >> 4;

                uint16 sub_packet_sizes[0xf];
                file.read(reinterpret_cast<uchar*>(sub_packet_sizes),
                          sub_packet_count * sizeof(uint16));

                auto actual_size = (1 + sub_packet_count) * uint32{2};
                for (auto const i : xrange(sub_packet_count)) {
                    sub_packet_sizes[i] = net::to_host<BE>(sub_packet_sizes[i]);
                    actual_size += sub_packet_sizes[i];
                }

                if (AMP_UNLIKELY(size != actual_size)) {
                    raise(errc::failure,
                          "expected packet size=%" PRIu32
                          ", actual packet size=%" PRIu32,
                          size, actual_size);
                }

                for (auto const i : xrange(sub_packet_count)) {
                    sub_packet_array[i].assign(file, sub_packet_sizes[i]);
                }
                sub_packet_pos = 0;
                break;
            }
        }
    }

    bool recv(io::buffer& dest)
    {
        switch (type) {
        case type::int0:
            if (sub_packet_ready) {
                sub_packet_array[0].swap(dest);
                sub_packet_ready = false;
                return true;
            }
            return false;
        case type::int4:
        case type::genr:
        case type::sipr:
            if (sub_packet_pos < (width * height)) {
                auto const pos = sub_packet_pos++;
                auto const sps = sub_packet_size;
                dest.assign(&sub_packet_array[0][pos * sps], sps);
                return true;
            }
            return false;
        case type::vbrs:
            if (sub_packet_pos < sub_packet_count) {
                sub_packet_array[sub_packet_pos++].swap(dest);
                return true;
            }
            return false;
        }
    }

private:
    enum class type : uint8 {
        int0,
        int4,
        genr,
        sipr,
        vbrs,
    };

    io::buffer sub_packet_array[0xf];
    uint32     sub_packet_size;
    uint32     frame_size;
    uint32     width;
    uint32     height;

    union {
        uint32 sub_packet_pos = 0;
        bool   sub_packet_ready;
    };
    union {
        uint32 sub_packet_row = 0;
        uint32 sub_packet_count;
    };

    splitter::type type{type::int0};
    bool           dnet{false};

    void reorder_sipr_sub_packets() noexcept
    {
        static constexpr uint8 swap_table[38][2] {
            {  0, 63 }, {  1, 22 }, {  2, 44 }, {  3, 90 },
            {  5, 81 }, {  7, 31 }, {  8, 86 }, {  9, 58 },
            { 10, 36 }, { 12, 68 }, { 13, 39 }, { 14, 73 },
            { 15, 53 }, { 16, 69 }, { 17, 57 }, { 19, 88 },
            { 20, 34 }, { 21, 71 }, { 24, 46 }, { 25, 94 },
            { 26, 54 }, { 28, 75 }, { 29, 50 }, { 32, 70 },
            { 33, 92 }, { 35, 74 }, { 38, 85 }, { 40, 56 },
            { 42, 87 }, { 43, 65 }, { 45, 59 }, { 48, 79 },
            { 49, 93 }, { 51, 89 }, { 55, 95 }, { 61, 76 },
            { 67, 83 }, { 77, 80 },
        };

        auto&& sub_packets = sub_packet_array[0];
        auto const nibbles_per_sub_packet = height * sub_packet_size * 2 / 96;

        for (auto&& entry : swap_table) {
            auto i = nibbles_per_sub_packet * entry[0];
            auto j = nibbles_per_sub_packet * entry[1];

            // swap nibbles (4 bits) of block 'i' with block 'j'
            for (auto n = nibbles_per_sub_packet; n != 0; --n, ++i, ++j) {
                auto& byte0 = sub_packets[i >> 1];
                auto& byte1 = sub_packets[j >> 1];

                auto const odd0 = ((i & 1) << 2);
                auto const odd1 = ((j & 1) << 2);

                auto const nib0 = (byte0 >> odd0) & 0xf;
                auto const nib1 = (byte1 >> odd1) & 0xf;

                byte0 &= (0xf0 >> odd0);
                byte0 |= (nib1 << odd0);

                byte1 &= (0xf0 >> odd1);
                byte1 |= (nib0 << odd1);
            }
        }
    }
};


struct object_header
{
    uint32 type;
    uint32 size;
    uint16 version;
};

struct file_header
{
    uint32 file_version;
    uint32 header_count;

    void read(io::stream& file)
    {
        uint32 object_size;
        uint16 object_version;
        file.gather<BE>(object_size, object_version);

        switch (object_size) {
        case 16:
            file_version = file.read<uint16,BE>();
            break;
        case 18:
            file_version = file.read<uint32,BE>();
            break;
        default:
            raise(errc::invalid_data_format,
                  "invalid RealMedia file header size");
        }
        header_count = file.read<uint32,BE>();
    }
};

struct properties
{
    uint32 maximum_bit_rate;
    uint32 average_bit_rate;
    uint32 maximum_packet_size;
    uint32 average_packet_size;
    uint32 packet_count;
    uint32 duration;
    uint32 preroll;
    uint32 index_offset;
    uint32 data_offset;
    uint16 stream_count;
    uint16 flags;

    void read(io::stream& file)
    {
        file.gather<BE>(maximum_bit_rate,
                        average_bit_rate,
                        maximum_packet_size,
                        average_packet_size,
                        packet_count,
                        duration,
                        preroll,
                        index_offset,
                        data_offset,
                        stream_count,
                        flags);
    }
};

struct media_properties
{
    uint16   stream_number;
    uint32   maximum_bit_rate;
    uint32   average_bit_rate;
    uint32   maximum_packet_size;
    uint32   average_packet_size;
    uint32   start_time;
    uint32   preroll;
    uint32   duration;
    u8string stream_name;
    u8string mime_type;

    void read(io::stream& file)
    {
        file.gather<BE>(stream_number,
                        maximum_bit_rate,
                        average_bit_rate,
                        maximum_packet_size,
                        average_packet_size,
                        start_time,
                        preroll,
                        duration);

        char buf[255];
        auto len = file.read<uint8>();
        file.read(buf, len);
        stream_name = u8string::from_utf8(buf, len);

        len = file.read<uint8>();
        file.read(buf, len);
        mime_type = u8string::from_utf8(buf, len);
    }
};

struct content_description
{
    u8string title;
    u8string artist;
    u8string copyright;
    u8string comment;

    void read(io::stream& file, bool const wide = true)
    {
        title     = read_string(file, wide);
        artist    = read_string(file, wide);
        copyright = read_string(file, wide);
        comment   = read_string(file, wide);
    }

private:
    static u8string read_string(io::stream& file, bool const wide)
    {
        auto len = wide
                 ? std::size_t{file.read<uint16,BE>()}
                 : std::size_t{file.read<uint8>()};

        if (len != 0) {
            io::buffer buf{file, len};

            auto const s = reinterpret_cast<char const*>(&buf[0]);
            len = cxp::strlen(s, len);

            return is_valid_utf8(s, len)
                 ? u8string::from_utf8_unchecked(s, len)
                 : u8string::from_latin1_lossy(s, len);
        }
        return u8string{};
    }
};


auto make_codec_format(real::audio_specific_data const& head)
{
    audio::codec_format fmt;
    fmt.bits_per_sample = head.sample_size;
    fmt.sample_rate     = head.sample_rate;
    fmt.channels        = head.channels;
    fmt.channel_layout  = audio::guess_channel_layout(fmt.channels);

    switch (head.codec_id) {
    case "lpcJ"_4cc:
    case "14_4"_4cc:
        fmt.codec_id = audio::codec::ra_14_4;
        fmt.bytes_per_packet = head.frame_size;
        fmt.bit_rate = 8000;
        break;
    case "28_8"_4cc:
        fmt.codec_id = audio::codec::ra_28_8;
        fmt.bytes_per_packet = head.coded_frame_size;
        fmt.bit_rate = 15200;
        break;
    case "dnet"_4cc:
        fmt.codec_id = audio::codec::ac3;
        fmt.frames_per_packet = 1536;
        fmt.bytes_per_packet = head.frame_size;
        fmt.bit_rate = muldiv(fmt.bytes_per_packet,
                              fmt.sample_rate * 8,
                              fmt.frames_per_packet);
        break;
    case "sipr"_4cc:
        fmt.codec_id = audio::codec::sipr;
        switch (head.flavor) {
        case 0:
            fmt.bytes_per_packet = 29;
            fmt.bit_rate = 6500;
            break;
        case 1:
            fmt.bytes_per_packet = 19;
            fmt.bit_rate = 8500;
            break;
        case 2:
            fmt.bytes_per_packet = 37;
            fmt.bit_rate = 5000;
            break;
        case 3:
            fmt.bytes_per_packet = 20;
            fmt.bit_rate = 16000;
            break;
        default:
            raise(errc::invalid_data_format,
                  "invalid SIPR flavor=%" PRIu16, head.flavor);
        }
        break;
    case "cook"_4cc:
        fmt.codec_id = audio::codec::cook;
        fmt.bytes_per_packet = head.sub_packet_size;
        break;
    case "atrc"_4cc:
        fmt.codec_id = audio::codec::atrac3;
        fmt.bytes_per_packet = head.sub_packet_size;
        break;
    case "raac"_4cc:
        fmt.codec_id = audio::codec::aac_lc;
        fmt.frames_per_packet = 1024;
        break;
    case "racp"_4cc:
        fmt.codec_id = audio::codec::he_aac_v1;
        fmt.frames_per_packet = 2048;
        break;
    default:
        raise(errc::unsupported_format, "unrecognized RealAudio codec ID: %#x",
              head.codec_id);
    }
    return fmt;
}

auto read_common_audio_specific_data(io::stream& file, uint32 const size,
                                     real::audio_specific_data& head,
                                     real::content_description* const cont)
{
    head.version = file.read<uint16,BE>();
    auto const min_size = [&]() -> uint32 {
        switch (head.version) {
        case 3: return 16;
        case 4: return 63;
        case 5: return 68;
        }
        raise(errc::invalid_data_format,
              "invalid RealAudio codec-specific data version");
    }();

    if (AMP_UNLIKELY(size < min_size)) {
        raise(errc::invalid_data_format,
              "invalid RealAudio codec-specific data size");
    }

    uint16 header_size;
    switch (head.version) {
    case 3:
        file.gather<BE>(header_size,
                        io::ignore<8>,
                        head.bytes_per_minute,
                        head.data_size);
        if (cont) {
            cont->read(file, false);
            file.seek(uint64{header_size} + 8);
        }

        head.flavor           = 0;
        head.coded_frame_size = 0;
        head.sub_packet_h     = 1;
        head.frame_size       = 20;
        head.sub_packet_size  = 0;
        head.sample_rate      = 8000;
        head.sample_size      = 0;
        head.channels         = 1;
        head.codec_id         = "lpcJ"_4cc;
        head.deint_id         = "Int0"_4cc;
        break;
    case 4:
        file.gather<BE>(io::ignore<6>,
                        head.data_size,
                        io::ignore<6>,
                        head.flavor,
                        head.coded_frame_size,
                        io::ignore<12>,
                        head.sub_packet_h,
                        head.frame_size,
                        head.sub_packet_size,
                        io::ignore<2>,
                        head.sample_rate,
                        io::ignore<2>,
                        head.sample_size,
                        head.channels,
                        io::ignore<1>,
                        head.deint_id,
                        io::ignore<1>,
                        head.codec_id,
                        io::ignore<3>);
        break;
    case 5:
        file.gather<BE>(io::ignore<6>,
                        head.data_size,
                        io::ignore<6>,
                        head.flavor,
                        head.coded_frame_size,
                        io::ignore<12>,
                        head.sub_packet_h,
                        head.frame_size,
                        head.sub_packet_size,
                        io::ignore<8>,
                        head.sample_rate,
                        io::ignore<2>,
                        head.sample_size,
                        head.channels,
                        head.deint_id,
                        head.codec_id,
                        io::ignore<4>);
        break;
    }
    return head;
}

auto read_old_audio_specific_data(io::stream& file,
                                  real::audio_specific_data& head,
                                  real::content_description& cont)
{
    read_common_audio_specific_data(file, UINT32_MAX, head, &cont);
    if (head.version == 4 || head.version == 5) {
        cont.read(file, false);
    }
    return make_codec_format(head);
}

auto read_audio_specific_data(io::stream& file, uint32 const size,
                              real::audio_specific_data& head)
{
    read_common_audio_specific_data(file, size, head, nullptr);

    auto extra_size = uint32{};
    switch (head.codec_id) {
    case "raac"_4cc:
    case "racp"_4cc:
        extra_size = file.read<uint32,BE>();
        if (extra_size != 0) {
            extra_size -= 1;
            file.skip(1);
        }
        break;
    case "atrc"_4cc:
    case "cook"_4cc:
    case "sipr"_4cc:
        extra_size = file.read<uint32,BE>();
        break;
    }

    auto fmt = make_codec_format(head);
    if (extra_size != 0) {
        fmt.extra.assign(file, extra_size);
    }
    return fmt;
}

auto read_lossless_audio_specific_data(io::stream& file, uint32 const size)
{
    audio::codec_format fmt;

    fmt.extra.resize(size + 4, uninitialized);
    io::store<BE>(&fmt.extra[0], "LSD:"_4cc);
    file.read(&fmt.extra[4], size);

    fmt.codec_id       = audio::codec::ra_lossless;
    fmt.channels       = io::load<uint16,BE>(&fmt.extra[8]);
    fmt.sample_rate    = io::load<uint32,BE>(&fmt.extra[12]);
    fmt.channel_layout = audio::guess_channel_layout(fmt.channels);
    return fmt;
}


struct index_record
{
    uint32 pts;
    uint32 offset;
    uint32 number;

    friend bool operator<(index_record const& x, uint64 const y) noexcept
    { return (x.pts < y); }
};

struct index_header
{
    uint32 record_count;
    uint16 stream_number;
    uint32 next_offset;

    static auto read(io::stream& file)
    {
        real::index_header head;
        file.gather<BE>(head.record_count,
                        head.stream_number,
                        head.next_offset);
        return head;
    }

    auto read_records(io::stream& file) const
    {
        std::vector<real::index_record> index(record_count);

        auto last_pts = int64{-1};

        for (auto const i : xrange(record_count)) {
            file.gather<BE>(io::ignore<2>,
                            index[i].pts,
                            index[i].offset,
                            index[i].number);
            if (int64{index[i].pts} < last_pts) {
                raise(errc::invalid_data_format,
                      "RealMedia: index records are not sorted"
                      " (record=%u PTS=%u lastPTS=%lld)",
                      i, index[i].pts, last_pts);
            }
            last_pts = index[i].pts;
        }
        return index;
    }
};

struct data_header
{
    uint32 packet_count;
    uint32 next_offset;
    uint64 beg_pos;
    uint64 end_pos;

    void read(io::stream& file, real::object_header const& object)
    {
        file.gather<BE>(packet_count,
                        next_offset);

        beg_pos = file.tell();
        end_pos = beg_pos + (object.size - 10);
    }
};


struct stream
{
    real::media_properties          mdpr;
    real::audio_specific_data       head;
    real::splitter                  deint;
    std::vector<real::index_record> index;

    uint32 read(io::stream& file, audio::codec_format& fmt)
    {
        mdpr.read(file);

        uint32 size, type;
        file.gather<BE>(size, type);

        if (type == ".ra\xfd"_4cc) {
            fmt = real::read_audio_specific_data(file, size, head);
            deint.reset(head);
        }
        else if (type == "LSD:"_4cc) {
            fmt = real::read_lossless_audio_specific_data(file, size);
            head.sub_packet_h = 1;
        }
        return type;
    }

    void read_old(io::stream& file, audio::codec_format& fmt,
                  real::content_description& cont)
    {
        fmt = real::read_old_audio_specific_data(file, head, cont);
        deint.reset(head);
    }
};


struct packet_header
{
private:
    struct version0
    {
        uint16 flags;
    };

    struct version1
    {
        uint16 asm_rule;
        uint8  asm_flags;
    };

public:
    uint16 version;
    uint16 length;
    uint16 stream_number;
    uint32 pts;

    union {
        packet_header::version0 v0;
        packet_header::version1 v1;
    };
};


class demuxer final :
    public audio::basic_demuxer<real::demuxer>
{
    using Base = audio::basic_demuxer<real::demuxer>;
    friend class audio::basic_demuxer<real::demuxer>;

public:
    explicit demuxer(ref_ptr<io::stream>, audio::open_mode);

    void seek(uint64);

    auto get_info(uint32);
    auto get_image(media::image_type);
    auto get_chapter_count() const noexcept;

private:
    void build_seek_index();
    void read_header();
    void read_header_old();

    bool parse_packet_header(packet_header&);
    uint32 get_packet_size();

    bool feed(io::buffer&);

    ref_ptr<io::stream>       file;
    real::stream              stream;
    real::properties          prop;
    real::data_header         data;
    real::content_description cont;
    uint32                    packet_number{};
    bool                      is_rmff;
};


void demuxer::build_seek_index()
{
    AMP_ASSERT(is_rmff);

    file->seek(data.beg_pos);
    packet_number = 0;

    auto const sub_packet_h = stream.head.sub_packet_h;
    auto sub_packet_number = uint32{};
    auto file_offset = file->tell();

    real::packet_header head;
    while (parse_packet_header(head)) {
        if (head.stream_number == stream.mdpr.stream_number) {
            if ((sub_packet_number++ % sub_packet_h) == 0) {
                real::index_record record;
                record.pts    = head.pts;
                record.offset = static_cast<uint32>(file_offset);
                record.number = packet_number;
                stream.index.push_back(record);
            }
        }
        file->skip(head.length);
        file_offset = file->tell();
    }

    if (stream.index.empty()) {
        raise(errc::failure, "RealMedia stream contains no seek index");
    }
}

void demuxer::read_header()
{
    real::file_header file_header;
    file_header.read(*file);

    auto const file_length = file->size();
    auto file_offset = file->tell();
    auto cont_found = false;
    auto data_found = false;
    auto prop_found = false;
    auto stream_selected = false;

    while (file_header.header_count-- != 0) {
        auto const remain = file_length - file_offset;
        if (remain < 10) {
            break;
        }

        real::object_header object;
        file->gather<BE>(object.type,
                         object.size,
                         object.version);

        if (object.size > remain) {
            object.size = static_cast<uint32>(remain);
            file_header.header_count = 0;
        }
        else if (object.size < 10) {
            if (object.type == "DATA"_4cc) {
                object.size = static_cast<uint32>(remain);
            }
            else {
                raise(errc::invalid_data_format,
                      "RealMedia object is too small");
            }
        }

        if (object.type == "CONT"_4cc) {
            if (cont_found) {
                raise(errc::invalid_data_format,
                      "multiple RealMedia 'CONT' objects");
            }
            cont.read(*file);
            cont_found = true;
        }
        else if (object.type == "PROP"_4cc) {
            if (prop_found) {
                raise(errc::invalid_data_format,
                      "multiple RealMedia 'PROP' objects");
            }
            prop.read(*file);
            prop_found = true;
        }
        else if (object.type == "DATA"_4cc) {
            data.read(*file, object);
            data_found = true;
        }
        else if (object.type == "MDPR"_4cc && !stream_selected) {
            auto const type = stream.read(*file, format);
            if (type == ".ra\xfd"_4cc || type == "LSD:"_4cc) {
                if (Base::try_resolve_decoder()) {
                    Base::total_frames = muldiv(stream.mdpr.duration,
                                                format.sample_rate, 1000);
                    stream_selected = true;
                }
            }
        }
        else if (object.type == "INDX"_4cc && stream_selected) {
            auto const head = real::index_header::read(*file);
            if (stream.mdpr.stream_number == head.stream_number) {
                stream.index = head.read_records(*file);
            }
        }

        file_offset += object.size;
        file->seek(file_offset);
    }

    if (!data_found || !prop_found) {
        raise(errc::invalid_data_format,
              "required RealMedia object '%s' not present",
              (data_found ? "PROP" : "DATA"));
    }
    if (!stream_selected) {
        raise(errc::failure, "no audio streams(s) in RealMedia file");
    }
}

void demuxer::read_header_old()
{
    stream.read_old(*file, format, cont);
    data.beg_pos = file->tell();
    data.end_pos = file->size();

    Base::resolve_decoder();
    Base::set_total_frames(muldiv(data.end_pos - data.beg_pos,
                                  format.sample_rate * 8,
                                  format.bit_rate));
}

demuxer::demuxer(ref_ptr<io::stream> s, audio::open_mode const mode) :
    file{std::move(s)}
{
    if (!(mode & (audio::playback | audio::metadata))) {
        return;
    }

    auto const signature = file->read<uint32,BE>();
    is_rmff = (signature == ".RMF"_4cc);

    if (is_rmff) {
        read_header();
        average_bit_rate = stream.mdpr.average_bit_rate;
    }
    else if (signature == ".ra\xfd"_4cc) {
        read_header_old();
        average_bit_rate = (stream.head.version == 3)
                         ? (stream.head.bytes_per_minute * 8) / 60
                         : format.bit_rate;
    }
    else {
        raise(errc::invalid_data_format,
              "no RealAudio or RealMedia file signature");
    }

    if (mode & audio::playback) {
        file->seek(data.beg_pos);
    }
}

bool demuxer::parse_packet_header(real::packet_header& head)
{
    AMP_ASSERT(is_rmff);
    if (AMP_UNLIKELY(packet_number >= data.packet_count)) {
        return false;
    }
    if (AMP_UNLIKELY((file->tell() + 13) >= data.end_pos)) {
        return false;
    }

    file->gather<BE>(head.version,
                     head.length,
                     head.stream_number,
                     head.pts);

    auto overhead = uint16{12};
    if (head.version == 0) {
        file->gather<BE>(head.v0.flags);
    }
    else {
        ++overhead;
        file->gather<BE>(head.v1.asm_rule,
                         head.v1.asm_flags);
    }

    if (AMP_UNLIKELY(head.length < overhead)) {
        raise(errc::out_of_bounds,
              "packet length (%" PRIu16 ") is less than its "
              "header length (%" PRIu16 ")",
              head.length, overhead);
    }

    head.length -= overhead;
    ++packet_number;
    return true;
}

uint32 demuxer::get_packet_size()
{
    if (is_rmff) {
        real::packet_header head;
        while (parse_packet_header(head)) {
            if (head.stream_number == stream.mdpr.stream_number) {
                return head.length;
            }
            file->skip(head.length);
        }
    }
    else {
        if ((file->tell() + stream.head.frame_size) <= data.end_pos) {
            return stream.head.frame_size;
        }
    }
    return 0;
}

bool demuxer::feed(io::buffer& dest)
{
    while (!stream.deint.recv(dest)) {
        auto const size = get_packet_size();
        if (AMP_UNLIKELY(!size)) {
            return false;
        }
        stream.deint.send(*file, size);
    }

    // FIXME: actually compute the instant bit rate!
    instant_bit_rate = average_bit_rate;
    return true;
}

void demuxer::seek(uint64 const pts)
{
    auto priming = uint64{};
    auto seekpos = uint64{};

    if (is_rmff) {
        if (AMP_UNLIKELY(stream.index.empty())) {
            build_seek_index();
        }

        auto record = std::lower_bound(stream.index.cbegin(),
                                       stream.index.cend(),
                                       muldiv(pts, 1000, format.sample_rate));
        if (record != stream.index.cbegin()) {
            record--;
        }

        if (record != stream.index.cend()) {
            priming = pts - muldiv(record->pts, format.sample_rate, 1000);
            seekpos = record->offset;
            packet_number = record->number;
        }
        else {
            priming = 0;
            seekpos = data.end_pos;
            packet_number = data.packet_count;
        }
    }
    else {
        auto const frames_per_packet
            = (format.sample_rate * 8 * stream.head.frame_size)
            / (format.bit_rate);

        auto nearest = pts / frames_per_packet;
        nearest = nearest - (nearest % stream.head.sub_packet_h);
        priming = pts - (nearest * frames_per_packet);
        seekpos = data.beg_pos + (nearest * stream.head.frame_size);
    }

    stream.deint.flush();
    file->seek(std::min(seekpos, data.end_pos));
    Base::set_seek_target_and_offset(pts, priming);
}

auto demuxer::get_info(uint32 const /* chapter_number */)
{
    audio::stream_info info{Base::get_format()};
    info.frames           = Base::total_frames;
    info.codec_id         = Base::format.codec_id;
    info.bits_per_sample  = Base::format.bits_per_sample;
    info.average_bit_rate = Base::average_bit_rate;

    if (cont.title) {
        info.tags.emplace(tags::title, cont.title);
    }
    if (cont.artist) {
        info.tags.emplace(tags::artist, cont.artist);
    }
    if (cont.copyright) {
        info.tags.emplace(tags::copyright, cont.copyright);
    }
    if (cont.comment) {
        info.tags.emplace(tags::comment, cont.comment);
    }
    info.props.emplace(tags::container, (is_rmff ? "RealMedia" : "RealAudio"));
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

AMP_REGISTER_INPUT(demuxer, "ra", "rm", "rma", "rmvb");

}}}   // namespace amp::real::<unnamed>

