////////////////////////////////////////////////////////////////////////////////
//
// audio/replaygain.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/packet.hpp>
#include <amp/media/dictionary.hpp>
#include <amp/media/tags.hpp>
#include <amp/numeric.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>

#include "audio/replaygain.hpp"

#include <cmath>
#include <cstddef>
#include <cstdlib>


namespace amp {
namespace audio {

void replaygain_filter::process(audio::packet& pkt) const noexcept
{
#if __has_warning("-Wfloat-equal")
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wfloat-equal"
#endif
    if (scale_ == 1.f) {
        return;
    }
#if __has_warning("-Wfloat-equal")
# pragma clang diagnostic pop
#endif

    auto const scale = scale_;
    for (auto const i : xrange(pkt.size())) {
        auto const x = pkt[i] * scale;
        pkt[i] = (x < -1.f) ? -1.f : (x > 1.f) ? 1.f : x;
    }
}

void replaygain_info::reset(media::dictionary const& dict)
{
    auto parse_float = [&](auto const key) {
        auto found = dict.find(key);
        if (found != dict.end()) {
            char* end;
            auto const s = found->second.c_str();
            auto const x = std::strtof(s, &end);
            if (std::isfinite(x) && end != s) {
                return x;
            }
        }
        return inf<float>;
    };

    album_gain_ = parse_float(tags::rg_album_gain);
    album_peak_ = parse_float(tags::rg_album_peak);
    track_gain_ = parse_float(tags::rg_track_gain);
    track_peak_ = parse_float(tags::rg_track_peak);
}

}}    // namespace amp::audio

