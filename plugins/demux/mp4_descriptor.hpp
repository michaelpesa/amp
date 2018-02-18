////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/mp4_descriptor.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_76E06F4C_B830_4207_A6AE_F62C98A56D5E
#define AMP_INCLUDED_76E06F4C_B830_4207_A6AE_F62C98A56D5E


#include <amp/audio/codec.hpp>
#include <amp/audio/format.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/reader.hpp>
#include <amp/stddef.hpp>

#include "mp4_audio.hpp"

#include <algorithm>
#include <tuple>
#include <utility>


namespace amp {
namespace mp4 {

inline auto read_descriptor_length(io::reader& r) noexcept
{
    auto length = uint32{};
    auto remain = std::min(r.remain(), 4_sz);

    while (remain-- != 0) {
        auto const byte = r.read_unchecked<uint8>();
        length = (length << 7) | (byte & 0x7f);
        if (!(byte & 0x80)) {
            break;
        }
    }
    return length;
}

inline auto read_descriptor_header(io::reader& r)
{
    auto const tag = r.read<uint8>();
    return std::make_pair(tag, mp4::read_descriptor_length(r));
}


struct decoder_config_descriptor
{
    uint8      object_type_indication{};
    uint32     maximum_bit_rate{};
    uint32     average_bit_rate{};
    io::buffer dsi;

    void setup(audio::codec_format& fmt) const
    {
        fmt.extra = dsi;
        switch (object_type_indication) {
        case 0x40:
        case 0x66:
        case 0x67:
        case 0x68:
            mp4::parse_audio_specific_config(fmt);
            break;
        case 0x69:
        case 0x6b:
            fmt.codec_id = audio::codec::mpeg_layer3;
            break;
        case 0xa5:
            fmt.codec_id = audio::codec::ac3;
            break;
        case 0xa6:
            fmt.codec_id = audio::codec::eac3;
            break;
        case 0xa9:
            fmt.codec_id = audio::codec::dts;
            break;
        case 0xaa:
            fmt.codec_id = audio::codec::dts_hd;
            break;
        case 0xab:
            fmt.codec_id = audio::codec::dts_hd_ma;
            break;
        case 0xac:
            fmt.codec_id = audio::codec::dts_express;
            break;
        case 0xad:
            fmt.codec_id = audio::codec::opus;
            break;
        case 0xdd:
            fmt.codec_id = audio::codec::vorbis;
            break;
        case 0xe1:
            fmt.codec_id = audio::codec::qcelp;
            break;
        }
    }

    void parse(io::reader r)
    {
        uint8  tag;
        uint32 len;

        std::tie(tag, len) = mp4::read_descriptor_header(r);
        if (tag == 0x03) {
            uint8 flags;
            r.gather<BE>(io::ignore<2>,         // ES_id
                         flags);

            if (flags & 0x80) { r.skip(2); }
            if (flags & 0x40) { r.skip(r.read<uint8>()); }
            if (flags & 0x20) { r.skip(2); }
        }
        else {
            r.skip(2);
        }

        std::tie(tag, len) = mp4::read_descriptor_header(r);
        if (tag == 0x04) {
            r.gather<BE>(object_type_indication,
                         io::ignore<1 + 3>,     // flags, buffer_size_db
                         maximum_bit_rate,
                         average_bit_rate);

            std::tie(tag, len) = mp4::read_descriptor_header(r);
            if (tag == 0x05) {
                dsi.resize(len, uninitialized);
                r.read(dsi.data(), len);
            }
        }
    }
};

}}    // namespace amp::mp4


#endif  // AMP_INCLUDED_76E06F4C_B830_4207_A6AE_F62C98A56D5E

