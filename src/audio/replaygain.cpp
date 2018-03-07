////////////////////////////////////////////////////////////////////////////////
//
// audio/replaygain.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/packet.hpp>
#include <amp/bitops.hpp>
#include <amp/media/dictionary.hpp>
#include <amp/media/tags.hpp>
#include <amp/numeric.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>

#include "audio/replaygain.hpp"
#include "core/cpu.hpp"

#include <cmath>
#include <cstddef>
#include <cstdlib>

#if defined(AMP_HAS_X86) || defined(AMP_HAS_X64)
# include <immintrin.h>
#endif


namespace amp {
namespace audio {
namespace {

#if defined(AMP_HAS_X86) || defined(AMP_HAS_X64)

AMP_TARGET("sse")
inline void process_sse(float* const data, std::size_t const n,
                        float const scale) noexcept
{
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 4); i != last; i += 4) {
        auto v = _mm_load_ps(&data[i]);
        v = _mm_mul_ps(v, _mm_set1_ps(scale));
        v = _mm_max_ps(v, _mm_set1_ps(-1.f));
        v = _mm_min_ps(v, _mm_set1_ps(+1.f));
        _mm_store_ps(&data[i], v);
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        auto v = _mm_load_ss(&data[i]);
        v = _mm_mul_ss(v, _mm_set_ss(scale));
        v = _mm_max_ss(v, _mm_set_ss(-1.f));
        v = _mm_min_ss(v, _mm_set_ss(+1.f));
        _mm_store_ss(&data[i], v);
    }
}

AMP_TARGET("avx")
inline void process_avx(float* const data, std::size_t const n,
                        float const scale) noexcept
{
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 8); i != last; i += 8) {
        auto v = _mm256_load_ps(&data[i]);
        v = _mm256_mul_ps(v, _mm256_set1_ps(scale));
        v = _mm256_max_ps(v, _mm256_set1_ps(-1.f));
        v = _mm256_min_ps(v, _mm256_set1_ps(+1.f));
        _mm256_store_ps(&data[i], v);
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        auto v = _mm_load_ss(&data[i]);
        v = _mm_mul_ss(v, _mm_set_ss(scale));
        v = _mm_max_ss(v, _mm_set_ss(-1.f));
        v = _mm_min_ss(v, _mm_set_ss(+1.f));
        _mm_store_ss(&data[i], v);
    }
}

#endif  // AMP_HAS_X86 || AMP_HAS_X64

inline void process_generic(float* const data, std::size_t const n,
                            float const scale) noexcept
{
    for (auto const i : xrange(n)) {
        auto const x = data[i] * scale;
        data[i] = (x < -1.f) ? -1.f : (x > +1.f) ? +1.f : x;
    }
}

}     // namespace <unnamed>


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

    auto const data = pkt.data();
    AMP_ASSERT(is_aligned(data, 32));
    AMP_ASSUME(is_aligned(data, 32));

#if defined(AMP_HAS_X86) || defined(AMP_HAS_X64)
    if (cpu::has_avx()) {
        return process_avx(data, pkt.size(), scale_);
    }
    if (cpu::has_sse()) {
        return process_sse(data, pkt.size(), scale_);
    }
#endif
    return process_generic(data, pkt.size(), scale_);
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

