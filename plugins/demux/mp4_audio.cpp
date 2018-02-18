////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/mp4_audio.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/format.hpp>
#include <amp/error.hpp>
#include <amp/bitops.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>
#include <amp/utility.hpp>

#include "mp4_audio.hpp"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>


namespace amp {
namespace mp4 {

std::array<uint32, 16> const sample_rates {{
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025,  8000, 7350,
}};

std::array<uint8, 8> const channels {{
    0, 1, 2, 3, 4, 5, 6, 8,
}};


namespace {

class bit_reader
{
public:
    using size_type = std::size_t;

    constexpr bit_reader(uint8 const* const p, size_type const n) noexcept :
        data_{p},
        size_{n * 8}
    {}

    constexpr auto size() const noexcept
    { return size_; }

    constexpr auto tell() const noexcept
    { return cursor_; }

    constexpr auto remain() const noexcept
    { return size() - tell(); }

    AMP_NOINLINE auto peek(size_type n) const
    {
        AMP_ASSERT(n <= 64 && "cannot peek at more than 64 bits");
        AMP_ASSUME(n <= 64);

        if (AMP_UNLIKELY(remain() < n)) {
            raise(errc::out_of_bounds);
        }

        auto const offset = cursor_ & 7;

        auto p = &data_[cursor_ >> 3];
        auto v = (uint64{*p++} << offset) & 0xff;

        if (n <= (8 - offset)) {
            return v >> (8 - n);
        }

        v = v >> offset;
        n = n - 8 + offset;

        for (; n >= 8; n -= 8) {
            v = (v << 8) | uint64{*p++};
        }
        if (n != 0) {
            v = (v << n) | (uint64{*p} >> (8 - n));
        }
        return v;
    }

    AMP_NOINLINE auto read(size_type const n)
    {
        auto const v = peek(n);
        cursor_ += n;
        return v;
    }

    template<size_type N>
    AMP_INLINE auto peek() const
    {
        using T = conditional_t<(N <=  8), uint8,
                  conditional_t<(N <= 16), uint16,
                  conditional_t<(N <= 32), uint32,
                  conditional_t<(N <= 64), uint64, void>>>>;
        return static_cast<T>(peek(N));
    }

    template<size_type N>
    AMP_INLINE auto read()
    {
        using T = conditional_t<(N <=  8), uint8,
                  conditional_t<(N <= 16), uint16,
                  conditional_t<(N <= 32), uint32,
                  conditional_t<(N <= 64), uint64, void>>>>;
        return static_cast<T>(read(N));
    }

    void seek(size_type const pos)
    {
        if (AMP_UNLIKELY(pos > size_)) {
            raise(errc::out_of_bounds);
        }
        cursor_ = pos;
    }

    void skip(size_type const n)
    { seek(tell() + n); }

    void byte_align() noexcept
    { cursor_ = align_up(cursor_, 8); }

private:
    uint8 const* data_;
    size_type    size_;
    size_type    cursor_{};
};


enum class aot : uint8 {
    none             = 0x00,
    aac_main         = 0x01,
    aac_lc           = 0x02,
    aac_ssr          = 0x03,
    aac_ltp          = 0x04,
    sbr              = 0x05,
    aac_scalable     = 0x06,
    twinvq           = 0x07,
    celp             = 0x08,
    hvxc             = 0x09,
    ttsi             = 0x0c,
    mainsynth        = 0x0d,
    wavesynth        = 0x0e,
    midi             = 0x0f,
    safx             = 0x10,
    er_aac_lc        = 0x11,
    er_aac_ltp       = 0x13,
    er_aac_scalable  = 0x14,
    er_twinvq        = 0x15,
    er_bsac          = 0x16,
    er_aac_ld        = 0x17,
    er_celp          = 0x18,
    er_hvxc          = 0x19,
    er_hiln          = 0x1a,
    er_parametric    = 0x1b,
    ssc              = 0x1c,
    ps               = 0x1d,
    mpeg_surround    = 0x1e,
    mpeg_layer1      = 0x20,
    mpeg_layer2      = 0x21,
    mpeg_layer3      = 0x22,
    dst              = 0x23,
    als              = 0x24,
    sls              = 0x25,
    sls_non_core     = 0x26,
    er_aac_eld       = 0x27,
    smr_simple       = 0x28,
    smr_main         = 0x29,
    usac_no_sbr      = 0x2a,
    saoc             = 0x2b,
    ld_mpeg_surround = 0x2c,
    usac             = 0x2d,
};

constexpr bool is_error_resilient(mp4::aot const x) noexcept
{
    return (x == mp4::aot::er_aac_eld)
        || (x >= mp4::aot::er_aac_lc && x <= mp4::aot::er_parametric);
}

constexpr bool is_valid_aot(uint8 const x) noexcept
{
    return (x >= 0x01) && (x <= 0x2d)
        && (x != 0x0a) && (x != 0x0b)
        && (x != 0x12) && (x != 0x1f);
}

inline auto validate_aot(uint8 const x)
{
    if (AMP_LIKELY(mp4::is_valid_aot(x))) {
        return static_cast<mp4::aot>(x);
    }
    raise(errc::failure, "invalid MP4 audio object type: %#02" PRIx8, x);
}

inline auto read_object_type(bit_reader& r)
{
    auto object_type = r.read<5>();
    if (object_type == 0x1f) {
        object_type = (0x20 + r.read<6>());
    }
    return mp4::validate_aot(object_type);
}

inline auto read_sample_rate(bit_reader& r)
{
    auto const index = r.read<4>();
    return (index == 0xf) ? r.read<24>() : mp4::sample_rates[index];
}


class audio_specific_config
{
public:
    explicit audio_specific_config(bit_reader);

    mp4::aot object_type;
    uint32   sample_rate;
    uint8    channel_config;
    uint32   channels{};
    mp4::aot extension_object_type{};
    uint32   extension_sample_rate{};
    uint8    extension_channel_config{};
    uint32   frame_length{};
    bool     sbr_present{};
    bool     ps_present{};

    uint32 output_sample_rate() const noexcept
    { return std::max(sample_rate, extension_sample_rate); }

    uint32 output_channels() const noexcept
    { return channels << ps_present; }

    uint32 output_channel_layout() const noexcept
    { return audio::aac_channel_layout(output_channels()); }

    uint32 get_codec_id() const noexcept;

private:
    void parse_ALS_specific_config(bit_reader&);
    void parse_ELD_specific_config(bit_reader&);
    void parse_GA_specific_config(bit_reader&);
    void parse_extension_config(bit_reader&);
    void parse_program_config(bit_reader&);
};


inline void audio_specific_config::parse_ALS_specific_config(bit_reader& r)
{
    if (r.remain() < 112) {
        raise(errc::failure, "insufficient ALS specific config size");
    }
    if (r.read<32>() != "ALS\0"_4cc) {
        raise(errc::failure, "'ALS' tag missing from ALS specific config");
    }

    sample_rate = r.read<32>();
    (void)        r.read<32>();
    channels    = r.read<16>() + 1;
}

inline void audio_specific_config::parse_ELD_specific_config(bit_reader& r)
{
    frame_length = (r.read<1>() ? 480 : 512);
    r.skip(1 +      // section_data_resilience_flag
           1 +      // scale_factor_data_resilience_flag
           1);      // spectral_data_resilience_flag

    sbr_present = r.read<1>();
    if (sbr_present) {
        extension_sample_rate = (sample_rate << r.read<1>());
        r.skip(1);      // eld_sbr_crc_flag
    }

    // parse ExtTypeConfigData
    while ((r.remain() > 4) && (r.read<4>() != 0)) {
        auto eld_extension_len = uint32{r.read<4>()};
        if (eld_extension_len == 0xf) {
            eld_extension_len += r.read<8>();
            if (eld_extension_len == (0xf + 0xff)) {
                eld_extension_len += r.read<16>();
            }
        }
        r.skip(eld_extension_len);
    }
}

inline void audio_specific_config::parse_program_config(bit_reader& r)
{
    r.skip(4);      // element_instance_tag

    object_type = mp4::validate_aot(r.read<2>());
    sample_rate = mp4::sample_rates[r.read<4>()];

    auto const num_front_channel_elements = r.read<4>();
    auto const num_side_channel_elements  = r.read<4>();
    auto const num_back_channel_elements  = r.read<4>();
    auto const num_lfe_channel_elements   = r.read<2>();
    auto const num_assoc_data_elements    = r.read<3>();
    auto const num_valid_cc_elements      = r.read<4>();

    if (r.read<1>()) {      // mono_mixdown_present_flag
        r.skip(1);          // mono_mixdown_element_number
    }
    if (r.read<1>()) {      // stereo_mixdown_present_flag
        r.skip(4);          // stereo_mixdown_element_number
    }
    if (r.read<1>()) {      // matrix_mixdown_idx_present_flag
        r.skip(2 + 1);      // matrix_mixdown_idx, pseudo_surround_enable
    }

    channels = num_front_channel_elements
             + num_side_channel_elements
             + num_back_channel_elements
             + num_lfe_channel_elements;

    for (auto i = 0; i < num_front_channel_elements; ++i) {
        channels += r.read<1>();    // front_element_is_cpe
        r.skip(4);                  // front_element_tag_select
    }
    for (auto i = 0; i < num_side_channel_elements; ++i) {
        channels += r.read<1>();    // side_element_is_cpe
        r.skip(4);                  // side_element_tag_select
    }
    for (auto i = 0; i < num_back_channel_elements; ++i) {
        channels += r.read<1>();    // back_element_is_cpe
        r.skip(4);                  // back_element_tag_select
    }

    r.skip(num_lfe_channel_elements * 4 +   // lfe_element_tag_select
           num_assoc_data_elements  * 4 +   // assoc_data_element_tag_select
           num_valid_cc_elements    * 1 +   // cc_element_is_ind_sw
           num_valid_cc_elements    * 4);   // valid_cc_element_tag_select

    r.byte_align();

    auto const comment_field_bytes = r.read<8>();
    r.skip(comment_field_bytes * 8);
}

inline void audio_specific_config::parse_GA_specific_config(bit_reader& r)
{
    frame_length = (r.read<1>() ? 960 : 1024);
    if (r.read<1>()) {      // depends_on_core_coder
        r.skip(14);         // core_coder_delay
    }

    auto const extension_flag = r.read<1>();
    if (channel_config == 0) {
        parse_program_config(r);
    }

    if (object_type == mp4::aot::aac_scalable ||
        object_type == mp4::aot::er_aac_scalable) {
        r.skip(3);      // layer_number
    }

    if (extension_flag) {
        if (object_type == mp4::aot::er_bsac) {
            r.skip(5 + 11);     // num_of_sub_frame, layer_length
        }
        if (object_type == mp4::aot::er_aac_lc       ||
            object_type == mp4::aot::er_aac_ltp      ||
            object_type == mp4::aot::er_aac_scalable ||
            object_type == mp4::aot::er_aac_ld) {
            r.skip(1 +          // aac_section_data_resilience_flag
                   1 +          // aac_scale_factor_data_resilience_flag
                   1);          // aac_spectral_data_resilience_flag
        }
        r.skip(1);              // extension_flag_3
    }
}

inline void audio_specific_config::parse_extension_config(bit_reader& r)
{
    if (r.read<11>() == 0x2b7) {
        extension_object_type = read_object_type(r);
        if (extension_object_type == mp4::aot::sbr) {
            sbr_present = r.read<1>();
            if (sbr_present) {
                extension_sample_rate = read_sample_rate(r);
            }
            if (r.remain() >= 12 && r.read<11>() == 0x548) {
                ps_present = r.read<1>();
            }
        }
        else if (extension_object_type == mp4::aot::er_bsac) {
            sbr_present = r.read<1>();
            if (sbr_present) {
                extension_sample_rate = read_sample_rate(r);
            }
            extension_channel_config = r.read<4>();
        }
    }
}

audio_specific_config::audio_specific_config(bit_reader r) :
    object_type{read_object_type(r)},
    sample_rate{read_sample_rate(r)},
    channel_config{r.read<4>()}
{
    if (channel_config < mp4::channels.size()) {
        channels       = mp4::channels[channel_config];
    }

    if (object_type == mp4::aot::sbr || object_type == mp4::aot::ps) {
        sbr_present = true;
        ps_present = (object_type == mp4::aot::ps);

        extension_object_type = mp4::aot::sbr;
        extension_sample_rate = read_sample_rate(r);
        object_type           = read_object_type(r);
    }

    if (object_type == mp4::aot::aac_main        ||
        object_type == mp4::aot::aac_lc          ||
        object_type == mp4::aot::aac_ssr         ||
        object_type == mp4::aot::aac_ltp         ||
        object_type == mp4::aot::aac_scalable    ||
        object_type == mp4::aot::twinvq          ||
        object_type == mp4::aot::er_aac_lc       ||
        object_type == mp4::aot::er_aac_ltp      ||
        object_type == mp4::aot::er_aac_scalable ||
        object_type == mp4::aot::er_twinvq       ||
        object_type == mp4::aot::er_bsac         ||
        object_type == mp4::aot::er_aac_ld) {
        parse_GA_specific_config(r);
    }
    else if (object_type == mp4::aot::er_aac_eld) {
        parse_ELD_specific_config(r);
    }
    else if (object_type == mp4::aot::als) {
        r.skip(5);
        if (r.peek<24>() != "\0ALS"_4cc) {
            r.skip(24);
        }
        parse_ALS_specific_config(r);
    }

    if (mp4::is_error_resilient(object_type)) {
        auto const ep_config = r.read<2>();
        if (ep_config == 2 || ep_config == 3) {
            raise(errc::not_implemented,
                  "MPEG-4 EPConfig support is not implemented");
        }
    }

    if ((extension_object_type != mp4::aot::sbr) && (r.remain() >= 16)) {
        parse_extension_config(r);
    }

    if (sbr_present && (object_type == mp4::aot::aac_lc)) {
        if (frame_length <= 1024) {
            frame_length *= 2;
        }
        ps_present &= (channels == 1);
    }
}

uint32 audio_specific_config::get_codec_id() const noexcept
{
    switch (object_type) {
    case aot::aac_lc:
        if (ps_present) {
            return audio::codec::he_aac_v2;
        }
        if (sbr_present) {
            return audio::codec::he_aac_v1;
        }
        return audio::codec::aac_lc;
    case aot::er_aac_lc:
        return audio::codec::aac_lc;
    case aot::aac_ltp:
    case aot::er_aac_ltp:
        return audio::codec::aac_ltp;
    case aot::er_aac_ld:
        return audio::codec::aac_ld;
    case aot::er_aac_eld:
        if (sbr_present) {
            return audio::codec::aac_eld_sbr;
        }
        return audio::codec::aac_eld;
    case aot::aac_main:
        return audio::codec::aac_main;
    case aot::aac_ssr:
        return audio::codec::aac_ssr;
    case aot::aac_scalable:
    case aot::er_aac_scalable:
        return audio::codec::aac_scalable;
    case aot::mpeg_layer1:
        return audio::codec::mpeg_layer1;
    case aot::mpeg_layer2:
        return audio::codec::mpeg_layer2;
    case aot::mpeg_layer3:
        return audio::codec::mpeg_layer3;
    case aot::als:
        return audio::codec::als;
    case aot::none:
    case aot::sbr:
    case aot::twinvq:
    case aot::celp:
    case aot::hvxc:
    case aot::ttsi:
    case aot::mainsynth:
    case aot::wavesynth:
    case aot::midi:
    case aot::safx:
    case aot::er_twinvq:
    case aot::er_bsac:
    case aot::er_celp:
    case aot::er_hvxc:
    case aot::er_hiln:
    case aot::er_parametric:
    case aot::ssc:
    case aot::ps:
    case aot::mpeg_surround:
    case aot::dst:
    case aot::sls:
    case aot::sls_non_core:
    case aot::smr_simple:
    case aot::smr_main:
    case aot::usac_no_sbr:
    case aot::saoc:
    case aot::ld_mpeg_surround:
    case aot::usac:
        return 0;
    }
}

}     // namespace <unnamed>


void parse_audio_specific_config(audio::codec_format& fmt)
{
    bit_reader r{fmt.extra.data(), fmt.extra.size()};
    mp4::audio_specific_config asc{r};

    fmt.codec_id          = asc.get_codec_id();
    fmt.sample_rate       = asc.output_sample_rate();
    fmt.channels          = asc.output_channels();
    fmt.channel_layout    = asc.output_channel_layout();
    fmt.frames_per_packet = asc.frame_length;
}

}}    // namespace amp::mp4

