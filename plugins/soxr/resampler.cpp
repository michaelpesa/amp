////////////////////////////////////////////////////////////////////////////////
//
// soxr/resampler.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/filter.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/packet.hpp>
#include <amp/error.hpp>
#include <amp/muldiv.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>

#include <soxr.h>


namespace std {

template<>
struct default_delete<::soxr>
{
    void operator()(::soxr* const handle) const noexcept
    { ::soxr_delete(handle); }
};

}     // namespace std


namespace amp {
namespace soxr {
namespace {

inline void verify(::soxr_error_t const error)
{
    if (AMP_LIKELY(error == nullptr)) {
        return;
    }
    raise(errc::failure, "SoX resampler error: %s", error);
}


class resampler
{
public:
    void calibrate(audio::format&);
    void process(audio::packet&);
    void drain(audio::packet&);
    void flush();
    uint64 get_latency() const;

    void set_sample_rate(uint32);
    void set_quality(uint8);

private:
    std::unique_ptr<::soxr> handle;
    audio::packet src_pkt;
    uint32 irate{};
    uint32 orate{};
    uint32 channels{};
    uint32 quality_recipe{};
};

void resampler::set_sample_rate(uint32 const rate)
{
    orate = rate;
}

void resampler::set_quality(uint8 const quality)
{
    switch (quality) {
    case audio::quality_minimum: quality_recipe = SOXR_QQ;  break;
    case audio::quality_low:     quality_recipe = SOXR_LQ;  break;
    case audio::quality_medium:  quality_recipe = SOXR_MQ;  break;
    case audio::quality_high:    quality_recipe = SOXR_HQ;  break;
    case audio::quality_maximum: quality_recipe = SOXR_VHQ; break;
    default:                     quality_recipe = SOXR_MQ;  break;
    }
}

void resampler::calibrate(audio::format& fmt)
{
    irate = fmt.sample_rate;
    channels = fmt.channels;

    auto const quality_flags = SOXR_ROLLOFF_NONE | SOXR_HI_PREC_CLOCK;
    auto const quality = ::soxr_quality_spec(quality_recipe, quality_flags);
    auto const io = ::soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_I);

    ::soxr_error_t error{};
    handle.reset(::soxr_create(static_cast<double>(irate),
                               static_cast<double>(orate),
                               channels, &error, &io,
                               &quality, nullptr));
    verify(error);
    fmt.sample_rate = orate;
}

void resampler::process(audio::packet& dst_pkt)
{
    if (handle == nullptr) {
        return;
    }

    dst_pkt.swap(src_pkt);
    dst_pkt.set_channel_layout(src_pkt.channel_layout());

    auto const ilen = src_pkt.frames();
    auto const olen = std::max<std::size_t>(muldiv(ilen, orate, irate), 1);
    dst_pkt.resize(olen * channels);

    std::size_t idone, odone;
    verify(::soxr_process(handle.get(),
                          src_pkt.data(), ilen, &idone,
                          dst_pkt.data(), olen, &odone));

    dst_pkt.resize(odone * channels);
}

void resampler::drain(audio::packet& pkt)
{
    if (handle == nullptr) {
        return;
    }

    auto const delay = std::llround(::soxr_delay(handle.get()));
    if (delay > 0) {
        auto const olen = static_cast<std::size_t>(delay);
        pkt.resize(olen * channels);

        std::size_t odone;
        verify(::soxr_process(handle.get(),
                              nullptr, 0, nullptr,
                              pkt.data(), olen, &odone));

        pkt.resize(odone * channels);
    }
}

void resampler::flush()
{
    if (handle != nullptr) {
        ::soxr_clear(handle.get());
    }
}

uint64 resampler::get_latency() const
{
    if (handle != nullptr) {
        auto const delay = std::llround(::soxr_delay(handle.get()));
        if (delay > 0) {
            return static_cast<uint64>(delay);
        }
    }
    return 0;
}

AMP_REGISTER_RESAMPLER(resampler);

}}}   // namespace amp::soxr::<unnamed>

