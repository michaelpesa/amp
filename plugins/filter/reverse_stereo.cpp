////////////////////////////////////////////////////////////////////////////////
//
// plugins/filter/reverse_stereo.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/filter.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/packet.hpp>
#include <amp/error.hpp>
#include <amp/range.hpp>

#include <utility>


namespace amp {
namespace audio {
namespace {

class reverse_stereo
{
public:
    void calibrate(audio::format&);
    void process(audio::packet&) noexcept;
    void drain(audio::packet&) noexcept;
    void flush() noexcept;
    uint64 get_latency() const noexcept;
};

void reverse_stereo::calibrate(audio::format& fmt)
{
    if (fmt.channel_layout != audio::channel_layout_stereo) {
        raise(errc::unsupported_format,
              "cannot convert non-stereo audio to binaural");
    }
}

void reverse_stereo::process(audio::packet& pkt) noexcept
{
    auto const samples = pkt.samples();
    for (auto i = 0_sz; i != samples; i += 2) {
        std::swap(pkt[i], pkt[i + 1]);
    }
}

void reverse_stereo::drain(audio::packet&) noexcept
{
}

void reverse_stereo::flush() noexcept
{
}

uint64 reverse_stereo::get_latency() const noexcept
{
    return 0;
}

AMP_REGISTER_FILTER(
    reverse_stereo,
    "amp.filter.reversestereo",
    "Reverse stereo");

}}}   // namespace amp::audio::<unnamed>


