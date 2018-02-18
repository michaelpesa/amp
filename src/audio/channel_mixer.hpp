////////////////////////////////////////////////////////////////////////////////
//
// audio/channel_mixer.hpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/filter.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/packet.hpp>
#include <amp/error.hpp>
#include <amp/numeric.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>

#include <algorithm>
#include <cmath>
#include <utility>


namespace amp {
namespace audio {
namespace {

enum : uint32 {
    FL = 0,
    FR,
    FC,
    LFE,
    BL,
    BR,
    FLC,
    FRC,
    BC,
    SL,
    SR,
    TC,
    TFL,
    TFC,
    TFR,
    TBL,
    TBC,
    TBR,
};


AMP_INLINE constexpr bool is_even(uint32 const x) noexcept
{
    return (x == 0) || ((x & (x - 1)) != 0);
}

AMP_INLINE constexpr bool is_balanced(uint32 const layout) noexcept
{
    return ((layout & ((1 << FL) | (1 << FR) | (1 << FC))) != 0)
        && is_even(layout & ((1 << FL) | (1 << FR)))
        && is_even(layout & ((1 << BL) | (1 << BR)))
        && is_even(layout & ((1 << SL) | (1 << SR)))
        && is_even(layout & ((1 << FLC) | (1 << FRC)))
        && is_even(layout & ((1 << TFL) | (1 << TFR)))
        && is_even(layout & ((1 << TBL) | (1 << TBR)));
}


auto build_matrix(uint32 const src_layout, uint32 const dst_layout,
                  float(&out)[max_channels][max_channels])
{
    if (!is_balanced(src_layout) || !is_balanced(dst_layout)) {
        raise(errc::unsupported_format,
              "cannot mix unbalanced channel layouts");
    }

    auto const same = src_layout & dst_layout;
    auto const diff = src_layout & ~dst_layout;

    float matrix[max_channels][max_channels]{};
    for (auto const i : xrange(max_channels)) {
        if (same & (1 << i)) {
            matrix[i][i] = 1.f;
        }
    }

    if (diff & (1 << FC)) {
        matrix[FL][FC] += sqrt1_2<float>;
        matrix[FR][FC] += sqrt1_2<float>;
    }

    if (diff & (1 << FL)) {
        if (dst_layout & (1 << FC)) {
            matrix[FC][FL] += sqrt1_2<float>;
            matrix[FC][FR] += sqrt1_2<float>;
            if (src_layout & (1 << FC)) {
                matrix[FC][FC] = 1.f;
            }
        }
    }

    if (diff & (1 << BC)) {
        if (dst_layout & (1 << BL)) {
            matrix[BL][BC] += sqrt1_2<float>;
            matrix[BR][BC] += sqrt1_2<float>;
        }
        else if (dst_layout & (1 << SL)) {
            matrix[SL][BC] += sqrt1_2<float>;
            matrix[SR][BC] += sqrt1_2<float>;
        }
        else if (dst_layout & (1 << FL)) {
            matrix[FL][BC] += .5f;
            matrix[FR][BC] += .5f;
        }
        else if (dst_layout & (1 << FC)) {
            matrix[FC][BC] += .5f;
        }
    }

    if (diff & (1 << BL)) {
        if (dst_layout & (1 << BC)) {
            matrix[BC][BL] += sqrt1_2<float>;
            matrix[BC][BR] += sqrt1_2<float>;
        }
        else if (dst_layout & (1 << SL)) {
            if (src_layout & (1 << SL)) {
                matrix[SL][BL] += sqrt1_2<float>;
                matrix[SR][BR] += sqrt1_2<float>;
            }
            else {
                matrix[SL][BL] += 1.f;
                matrix[SR][BR] += 1.f;
            }
        }
        else if (dst_layout & (1 << FL)) {
            matrix[FL][BL] += .5f;
            matrix[FR][BR] += .5f;
        }
        else if (dst_layout & (1 << FC)) {
            matrix[FC][BL] += .5f;
            matrix[FC][BR] += .5f;
        }
    }

    if (diff & (1 << SL)) {
        if (dst_layout & (1 << BL)) {
            if (src_layout & (1 << BL)) {
                matrix[BL][SL] += sqrt1_2<float>;
                matrix[BR][SR] += sqrt1_2<float>;
            }
            else {
                matrix[BL][SL] += 1.f;
                matrix[BR][SR] += 1.f;
            }
        }
        else if (dst_layout & (1 << BC)) {
            matrix[BC][SL] += sqrt1_2<float>;
            matrix[BC][SR] += sqrt1_2<float>;
        }
        else if (dst_layout & (1 << FL)) {
            matrix[FL][SL] += .5f;
            matrix[FR][SR] += .5f;
        }
        else if (dst_layout & (1 << FC)) {
            matrix[FC][SL] += .5f;
            matrix[FC][SR] += .5f;
        }
    }

    if (diff & (1 << FLC)) {
        if (dst_layout & (1 << FL)) {
            matrix[FL][FLC] += 1.f;
            matrix[FR][FRC] += 1.f;
        }
        else if (dst_layout & (1 << FC)) {
            matrix[FC][FLC] += sqrt1_2<float>;
            matrix[FC][FRC] += sqrt1_2<float>;
        }
    }

    auto max_coeff = 0.f;
    auto out_i = 0_sz;

    for (auto const i : xrange(max_channels)) {
        auto coeff = 0.f;
        auto out_j = 0_sz;

        for (auto const j : xrange(max_channels)) {
            coeff += std::fabs(matrix[i][j]);
            if (src_layout & (1 << j)) {
                out[out_i][out_j++] = matrix[i][j];
            }
        }

        max_coeff = std::max(max_coeff, coeff);
        if (dst_layout & (1 << i)) {
            ++out_i;
        }
    }

    if (max_coeff > 1.f) {
        auto const scale = 1.f / max_coeff;
        for (auto const i : xrange(max_channels)) {
            for (auto const j : xrange(max_channels)) {
                out[i][j] *= scale;
            }
        }
    }
}

void mix_generic(float const* src, float* dst, std::size_t n,
                 float const(&matrix)[max_channels][max_channels],
                 uint32 const src_channels, uint32 const dst_channels) noexcept
{
    while (n-- != 0) {
        for (auto const i : xrange(dst_channels)) {
            auto acc = 0.f;
            for (auto const j : xrange(src_channels)) {
                acc = std::fma(src[j], matrix[i][j], acc);
            }
            dst[i] = acc;
        }
        src += src_channels;
        dst += dst_channels;
    }
}


class channel_mixer final :
    public implement_ref_count<channel_mixer, filter>
{
public:
    explicit channel_mixer(audio::format& src, audio::format const& dst) :
        dst_channels(dst.channels),
        dst_channel_layout(dst.channel_layout)
    {
        calibrate(src);
    }

    void calibrate(audio::format& fmt) override
    {
        build_matrix(fmt.channel_layout, dst_channel_layout, matrix);

        fmt.channels = dst_channels;
        fmt.channel_layout = dst_channel_layout;
    }

    void process(audio::packet& pkt) override
    {
        auto const frames = pkt.frames();
        pkt.swap(tmp_pkt);
        pkt.set_bit_rate(tmp_pkt.bit_rate());
        pkt.set_channel_layout(dst_channel_layout, dst_channels);
        pkt.resize(frames * dst_channels, uninitialized);

        mix_generic(tmp_pkt.data(), pkt.data(), frames,
                    matrix, tmp_pkt.channels(), dst_channels);
    }

    void drain(audio::packet&) noexcept override
    {
    }

    void flush() noexcept override
    {
    }

    uint64 get_latency() noexcept override
    {
        return 0;
    }

private:
    audio::packet tmp_pkt;
    uint32 dst_channels{};
    uint32 dst_channel_layout{};
    float matrix[max_channels][max_channels];
};

}}}   // namespace amp::audio::<unnamed>

