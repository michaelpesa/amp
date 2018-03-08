////////////////////////////////////////////////////////////////////////////////
//
// soxr/resampler.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/filter.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/packet.hpp>
#include <amp/error.hpp>
#include <amp/numeric.hpp>
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
    std::unique_ptr<::soxr> handle_;
    audio::packet in_pkt_;
    uint32 irate_{};
    uint32 orate_{};
    uint32 channels_{};
    uint32 quality_{};
};

void resampler::set_sample_rate(uint32 const x)
{
    orate_ = x;
}

void resampler::set_quality(uint8 const x)
{
    switch (x) {
    case audio::quality_minimum: quality_ = SOXR_QQ;  break;
    case audio::quality_low:     quality_ = SOXR_LQ;  break;
    case audio::quality_medium:  quality_ = SOXR_MQ;  break;
    case audio::quality_high:    quality_ = SOXR_HQ;  break;
    case audio::quality_maximum: quality_ = SOXR_VHQ; break;
    default:                     quality_ = SOXR_MQ;  break;
    }
}

void resampler::calibrate(audio::format& fmt)
{
    irate_ = fmt.sample_rate;
    channels_ = fmt.channels;

    auto const quality_flags = SOXR_ROLLOFF_NONE | SOXR_HI_PREC_CLOCK;
    auto const quality_spec = ::soxr_quality_spec(quality_, quality_flags);
    auto const io_spec = ::soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_I);

    ::soxr_error_t error{};
    handle_.reset(::soxr_create(static_cast<double>(irate_),
                                static_cast<double>(orate_),
                                channels_, &error, &io_spec,
                                &quality_spec, nullptr));
    verify(error);
    fmt.sample_rate = orate_;
}

void resampler::process(audio::packet& out_pkt)
{
    if (handle_ == nullptr) {
        return;
    }

    out_pkt.swap(in_pkt_);
    out_pkt.set_channel_layout(in_pkt_.channel_layout());

    auto const ilen = in_pkt_.frames();
    auto const olen = std::max<std::size_t>(muldiv(ilen, orate_, irate_), 1);
    out_pkt.resize(olen * channels_);

    std::size_t idone, odone;
    verify(::soxr_process(handle_.get(),
                          in_pkt_.data(), ilen, &idone,
                          out_pkt.data(), olen, &odone));
    out_pkt.resize(odone * channels_);
}

void resampler::drain(audio::packet& pkt)
{
    if (handle_ == nullptr) {
        return;
    }

    auto const delay = std::llround(::soxr_delay(handle_.get()));
    if (delay > 0) {
        auto const olen = static_cast<std::size_t>(delay);
        pkt.resize(olen * channels_);

        std::size_t odone;
        verify(::soxr_process(handle_.get(),
                              nullptr, 0, nullptr,
                              pkt.data(), olen, &odone));
        pkt.resize(odone * channels_);
    }
}

void resampler::flush()
{
    if (handle_ != nullptr) {
        ::soxr_clear(handle_.get());
    }
}

uint64 resampler::get_latency() const
{
    if (handle_ != nullptr) {
        auto const delay = std::llround(::soxr_delay(handle_.get()));
        if (delay > 0) {
            return static_cast<uint64>(delay);
        }
    }
    return 0;
}

AMP_REGISTER_RESAMPLER(resampler);

}}}   // namespace amp::soxr::<unnamed>

