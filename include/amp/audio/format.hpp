////////////////////////////////////////////////////////////////////////////////
//
// amp/audio/format.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_5BB90317_8226_4BF1_90E3_4BD10D2A6C09
#define AMP_INCLUDED_5BB90317_8226_4BF1_90E3_4BD10D2A6C09


#include <amp/io/buffer.hpp>
#include <amp/stddef.hpp>

#include <cstddef>


namespace amp {
namespace audio {

constexpr uint32 min_channels =  1;
constexpr uint32 max_channels = 18;

constexpr uint32 min_sample_rate =   8000;
constexpr uint32 max_sample_rate = 384000;


struct format
{
    uint32 channels;
    uint32 channel_layout;
    uint32 sample_rate;

    AMP_EXPORT
    void validate() const;
};

struct codec_format
{
    io::buffer extra;
    uint32     flags{};
    uint32     codec_id{};
    uint32     sample_rate{};
    uint32     channels{};
    uint32     channel_layout{};
    uint32     bits_per_sample{};
    uint32     bytes_per_packet{};
    uint32     frames_per_packet{};
    uint32     bit_rate{};
};


enum channel_bit : uint32 {
    front_left         = (1 <<  0),
    front_right        = (1 <<  1),
    front_center       = (1 <<  2),
    lfe                = (1 <<  3),
    back_left          = (1 <<  4),
    back_right         = (1 <<  5),
    front_center_left  = (1 <<  6),
    front_center_right = (1 <<  7),
    back_center        = (1 <<  8),
    side_left          = (1 <<  9),
    side_right         = (1 << 10),
    top_center         = (1 << 11),
    top_front_left     = (1 << 12),
    top_front_center   = (1 << 13),
    top_front_right    = (1 << 14),
    top_back_left      = (1 << 15),
    top_back_center    = (1 << 16),
    top_back_right     = (1 << 17),
};


constexpr auto channel_layout_mono
    = channel_bit::front_center;

constexpr auto channel_layout_stereo
    = channel_bit::front_left
    | channel_bit::front_right;

constexpr auto channel_layout_surround
    = channel_layout_stereo
    | channel_bit::front_center;

constexpr auto channel_layout_quad
    = channel_layout_stereo
    | channel_bit::back_left
    | channel_bit::back_right;

constexpr auto channel_layout_4_0
    = channel_layout_surround
    | channel_bit::back_center;

constexpr auto channel_layout_5_0
    = channel_layout_surround
    | channel_bit::back_left
    | channel_bit::back_right;

constexpr auto channel_layout_5_0_side
    = channel_layout_surround
    | channel_bit::side_left
    | channel_bit::side_right;

constexpr auto channel_layout_6_0
    = channel_layout_5_0
    | channel_bit::back_center;

constexpr auto channel_layout_6_0_side
    = channel_layout_5_0_side
    | channel_bit::back_center;

constexpr auto channel_layout_7_0
    = channel_layout_5_0
    | channel_bit::side_left
    | channel_bit::side_right;

constexpr auto channel_layout_7_0_front
    = channel_layout_5_0
    | channel_bit::front_center_left
    | channel_bit::front_center_right;


constexpr auto channel_layout_2_1
    = channel_layout_stereo
    | channel_bit::lfe;

constexpr auto channel_layout_4_1
    = channel_layout_4_0
    | channel_bit::lfe;

constexpr auto channel_layout_5_1
    = channel_layout_5_0
    | channel_bit::lfe;

constexpr auto channel_layout_5_1_side
    = channel_layout_5_0_side
    | channel_bit::lfe;

constexpr auto channel_layout_6_1
    = channel_layout_6_0
    | channel_bit::lfe;

constexpr auto channel_layout_6_1_side
    = channel_layout_6_0_side
    | channel_bit::lfe;

constexpr auto channel_layout_7_1
    = channel_layout_7_0
    | channel_bit::lfe;

constexpr auto channel_layout_7_1_front
    = channel_layout_7_0_front
    | channel_bit::lfe;


inline uint32 guess_channel_layout(uint32 const channels) noexcept
{
    switch (channels) {
    case 1: return channel_layout_mono;
    case 2: return channel_layout_stereo;
    case 4: return channel_layout_quad;
    case 5: return channel_layout_5_0;
    case 6: return channel_layout_5_1;
    case 7: return channel_layout_6_1;
    case 8: return channel_layout_7_1;
    }
    return 0;
}

inline uint32 aac_channel_layout(uint32 const channels) noexcept
{
    switch (channels) {
    case 1: return channel_layout_mono;
    case 2: return channel_layout_stereo;
    case 3: return channel_layout_surround;
    case 4: return channel_layout_quad;
    case 5: return channel_layout_5_0;
    case 6: return channel_layout_5_1;
    case 8: return channel_layout_7_1_front;
    }
    return 0;
}

inline uint32 xiph_channel_layout(uint32 const channels) noexcept
{
    switch (channels) {
    case 1: return channel_layout_mono;
    case 2: return channel_layout_stereo;
    case 3: return channel_layout_surround;
    case 4: return channel_layout_quad;
    case 5: return channel_layout_5_0;
    case 6: return channel_layout_5_1;
    case 7: return channel_layout_6_1_side;
    case 8: return channel_layout_7_1;
    }
    return 0;
}

}}    // namespace amp::audio


#endif  // AMP_INCLUDED_5BB90317_8226_4BF1_90E3_4BD10D2A6C09

