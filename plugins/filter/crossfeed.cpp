////////////////////////////////////////////////////////////////////////////////
//
// plugins/filter/crossfeed.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/filter.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/packet.hpp>
#include <amp/audio/utility.hpp>
#include <amp/error.hpp>
#include <amp/numeric.hpp>

#include <array>
#include <cmath>


namespace amp {
namespace audio {
namespace {

class crossfeed
{
public:
    void calibrate(audio::format&);
    void process(audio::packet&) noexcept;
    void drain(audio::packet&) noexcept;
    void flush() noexcept;
    uint64 get_latency() const noexcept;

private:
    double a0_lo;
    double b1_lo;
    double a0_hi;
    double b1_hi;
    double gain;

    struct {
        uint16 fcut{700};
        uint16 feed{45};
        uint32 rate{};
    } params;

    struct {
        std::array<double, 2> asis;
        std::array<double, 2> lo;
        std::array<double, 2> hi;
    } lfs;
};


void crossfeed::calibrate(audio::format& fmt)
{
    if (fmt.channel_layout != audio::channel_layout_stereo) {
        raise(errc::unsupported_format,
              "cannot convert non-stereo audio to binaural");
    }

    if (params.rate == fmt.sample_rate) {
        return;
    }

    flush();
    params.rate = fmt.sample_rate;

    auto const level = static_cast<double>(params.feed) / 10.0;
    auto const rate  = static_cast<double>(params.rate);

    auto const GB_lo = level * -5.0 / 6.0 - 3.0;
    auto const GB_hi = level        / 6.0 - 3.0;

    auto const G_lo  =       audio::to_amplitude(GB_lo);
    auto const G_hi  = 1.0 - audio::to_amplitude(GB_hi);

    auto const Fc_lo = static_cast<double>(params.fcut);
    auto const Fc_hi = Fc_lo * std::exp2((GB_lo - to_decibels(G_hi)) / 12.0);

    auto const x_lo = std::exp(-2.0 * pi<double> * Fc_lo / rate);
    auto const x_hi = std::exp(-2.0 * pi<double> * Fc_hi / rate);

    a0_lo = G_lo * (1.0 - x_lo);
    b1_lo = x_lo;
    a0_hi = 1.0 - G_hi * (1.0 - x_hi);
    b1_hi = x_hi;
    gain  = 1.0 / (1.0 - G_hi + G_lo);
}

void crossfeed::process(audio::packet& pkt) noexcept
{
    auto const samples = pkt.samples();
    for (auto i = 0_sz; i != samples; i += 2) {
        auto const L = static_cast<double>(pkt[i]);
        auto const R = static_cast<double>(pkt[i + 1]);

        lfs.lo[0] = (a0_lo * L) + (b1_lo * lfs.lo[0]);
        lfs.lo[1] = (a0_lo * R) + (b1_lo * lfs.lo[1]);

        lfs.hi[0] = (a0_hi * L) + (b1_hi * lfs.hi[0]) - (b1_hi * lfs.asis[0]);
        lfs.hi[1] = (a0_hi * R) + (b1_hi * lfs.hi[1]) - (b1_hi * lfs.asis[1]);

        lfs.asis[0] = L;
        lfs.asis[1] = R;

        pkt[i+0] = static_cast<float>((lfs.hi[0] + lfs.lo[1]) * gain);
        pkt[i+1] = static_cast<float>((lfs.hi[1] + lfs.lo[0]) * gain);
    }
}

void crossfeed::drain(audio::packet&) noexcept
{
}

void crossfeed::flush() noexcept
{
    lfs = {};
}

uint64 crossfeed::get_latency() const noexcept
{
    return 0;
}

AMP_REGISTER_FILTER(
    crossfeed,
    "amp.filter.crossfeed",
    "Crossfeed");

}}}   // namespace amp::audio::<unnamed>

