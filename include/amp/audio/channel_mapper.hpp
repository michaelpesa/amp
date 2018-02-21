////////////////////////////////////////////////////////////////////////////////
//
// amp/audio/channel_mapper.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_209A84AD_DDB9_4A60_B10A_D1CEBCCDCD18
#define AMP_INCLUDED_209A84AD_DDB9_4A60_B10A_D1CEBCCDCD18


#include <amp/stddef.hpp>

#include <memory>
#include <string_view>
#include <utility>


namespace amp {
namespace audio {

enum channel_layout_tag : uint32 {
    unknown             = 0xffff0000,
    use_descriptions    = (  0 << 16) | 0,
    use_bitmap          = (  1 << 16) | 0,
    mono                = (100 << 16) | 1,
    stereo              = (101 << 16) | 2,
    stereo_headphones   = (102 << 16) | 2,
    matrix_stereo       = (103 << 16) | 2,
    mid_side            = (104 << 16) | 2,
    xy                  = (105 << 16) | 2,
    binaural            = (106 << 16) | 2,
    ambisonic_b_format  = (107 << 16) | 4,
    quadraphonic        = (108 << 16) | 4,
    pentagonal          = (109 << 16) | 5,
    hexagonal           = (110 << 16) | 6,
    octagonal           = (111 << 16) | 8,
    cube                = (112 << 16) | 8,
    mpeg_3_0_a          = (113 << 16) | 3,
    mpeg_3_0_b          = (114 << 16) | 3,
    mpeg_4_0_a          = (115 << 16) | 4,
    mpeg_4_0_b          = (116 << 16) | 4,
    mpeg_5_0_a          = (117 << 16) | 5,
    mpeg_5_0_b          = (118 << 16) | 5,
    mpeg_5_0_c          = (119 << 16) | 5,
    mpeg_5_0_d          = (120 << 16) | 5,
    mpeg_5_1_a          = (121 << 16) | 6,
    mpeg_5_1_b          = (122 << 16) | 6,
    mpeg_5_1_c          = (123 << 16) | 6,
    mpeg_5_1_d          = (124 << 16) | 6,
    mpeg_6_1_a          = (125 << 16) | 7,
    mpeg_7_1_a          = (126 << 16) | 8,
    mpeg_7_1_b          = (127 << 16) | 8,
    mpeg_7_1_c          = (128 << 16) | 8,
    emagic_default_7_1  = (129 << 16) | 8,
    smpte_dtv           = (130 << 16) | 8,
    itu_2_1             = (131 << 16) | 3,
    itu_2_2             = (132 << 16) | 4,
    dvd_4               = (133 << 16) | 3,
    dvd_5               = (134 << 16) | 4,
    dvd_6               = (135 << 16) | 5,
    dvd_10              = (136 << 16) | 4,
    dvd_11              = (137 << 16) | 5,
    dvd_18              = (138 << 16) | 5,
    audiounit_6_0       = (139 << 16) | 6,
    audiounit_7_0       = (140 << 16) | 7,
    aac_6_0             = (141 << 16) | 6,
    aac_6_1             = (142 << 16) | 7,
    aac_7_0             = (143 << 16) | 7,
    aac_octagonal       = (144 << 16) | 8,
    tmh_10_2_standard   = (145 << 16) | 16,
    tmh_10_2_full       = (146 << 16) | 21,
    discrete_in_order   = (147 << 16) | 0,
    audiounit_7_0_front = (148 << 16) | 7,
    ac3_1_0_1           = (149 << 16) | 2,
    ac3_3_0             = (150 << 16) | 3,
    ac3_3_1             = (151 << 16) | 4,
    ac3_3_0_1           = (152 << 16) | 4,
    ac3_2_1_1           = (153 << 16) | 4,
    ac3_3_1_1           = (154 << 16) | 5,
    eac3_6_0_a          = (155 << 16) | 6,
    eac3_7_0_a          = (156 << 16) | 7,
    eac3_6_1_a          = (157 << 16) | 7,
    eac3_6_1_b          = (158 << 16) | 7,
    eac3_6_1_c          = (159 << 16) | 7,
    eac3_7_1_a          = (160 << 16) | 8,
    eac3_7_1_b          = (161 << 16) | 8,
    eac3_7_1_c          = (162 << 16) | 8,
    eac3_7_1_d          = (163 << 16) | 8,
    eac3_7_1_e          = (164 << 16) | 8,
    eac3_7_1_f          = (165 << 16) | 8,
    eac3_7_1_g          = (166 << 16) | 8,
    eac3_7_1_h          = (167 << 16) | 8,
    dts_3_1             = (168 << 16) | 4,
    dts_4_1             = (169 << 16) | 5,
    dts_6_0_a           = (170 << 16) | 6,
    dts_6_0_b           = (171 << 16) | 6,
    dts_6_0_c           = (172 << 16) | 6,
    dts_6_1_a           = (173 << 16) | 7,
    dts_6_1_b           = (174 << 16) | 7,
    dts_6_1_c           = (175 << 16) | 7,
    dts_7_0             = (176 << 16) | 7,
    dts_7_1             = (177 << 16) | 8,
    dts_8_0_a           = (178 << 16) | 8,
    dts_8_0_b           = (179 << 16) | 8,
    dts_8_1_a           = (180 << 16) | 9,
    dts_8_1_b           = (181 << 16) | 9,
    dts_6_1_d           = (182 << 16) | 7,
};

constexpr auto channel_layout_tag_first = channel_layout_tag::mono;
constexpr auto channel_layout_tag_last  = channel_layout_tag::dts_6_1_d;


class packet;

class channel_mapper
{
public:
    virtual ~channel_mapper() = default;

    virtual void process(audio::packet&) = 0;
    virtual uint32 get_channel_layout() const noexcept = 0;

    AMP_EXPORT
    static std::unique_ptr<channel_mapper> create(uint32);
    AMP_EXPORT
    static std::unique_ptr<channel_mapper> create(std::string_view);
};


inline std::string_view xiph_channel_map(uint32 const channels) noexcept
{
    switch (channels) {
    case 1: return "\x1";
    case 2: return "\x1\x2";
    case 3: return "\x1\x3\x2";
    case 4: return "\x1\x2\x5\x6";
    case 5: return "\x1\x3\x2\x5\x6";
    case 6: return "\x1\x3\x2\x5\x6\x4";
    case 7: return "\x1\x3\x2\xa\xb\x9\x4";
    case 8: return "\x1\x3\x2\xa\xb\x5\x6\x4";
    }
    return {};
}

}}    // namespace amp::audio


#endif  // AMP_INCLUDED_209A84AD_DDB9_4A60_B10A_D1CEBCCDCD18

