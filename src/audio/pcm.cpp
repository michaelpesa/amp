////////////////////////////////////////////////////////////////////////////////
//
// audio/pcm.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/codec.hpp>
#include <amp/audio/decoder.hpp>
#include <amp/audio/format.hpp>
#include <amp/audio/packet.hpp>
#include <amp/audio/pcm.hpp>
#include <amp/bitops.hpp>
#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/memory.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>

#include "core/cpu.hpp"

#include <array>
#include <cinttypes>
#include <cstddef>
#include <utility>

#if defined(AMP_HAS_X86) || defined(AMP_HAS_X64)
# include <immintrin.h>
#endif


namespace amp {
namespace audio {
namespace pcm {
namespace {

enum : uint32 {
    I8    = 0b0000,
    I16LE = 0b0010,
    I16BE = 0b0011,
    I24LE = 0b0100,
    I24BE = 0b0101,
    I32LE = 0b0110,
    I32BE = 0b0111,
    F32LE = 0b1000,
    F32BE = 0b1001,
    F64LE = 0b1010,
    F64BE = 0b1011,

    I16NE = I16LE | AMP_BIG_ENDIAN,
    I24NE = I24LE | AMP_BIG_ENDIAN,
    I32NE = I32LE | AMP_BIG_ENDIAN,
    F32NE = F32LE | AMP_BIG_ENDIAN,
    F64NE = F64LE | AMP_BIG_ENDIAN,
};

constexpr bool is_float(uint32 const enc) noexcept
{ return (enc & 0b1000) != 0; }

constexpr uint32 sample_size(uint32 const enc) noexcept
{ return (((enc & 0b0110) >> 1) + 1) << ((enc & 0b1000) >> 2); }

constexpr endian byte_order(uint32 const enc) noexcept
{ return (enc & 0b0001) ? BE : LE; }


struct state
{
    float scale;
    uint32 sign;
    uint32 enc;

    template<uint32 Enc>
    enable_if_t<is_float(Enc), float> read(void const*) const noexcept;
    template<uint32 Enc>
    enable_if_t<!is_float(Enc), float> read(void const*) const noexcept;
};


template<uint32 Enc>
AMP_INLINE auto state::read(void const* const src) const noexcept ->
    enable_if_t<is_float(Enc), float>
{
    using T = conditional_t<(sample_size(Enc) == 4), float,
              conditional_t<(sample_size(Enc) == 8), double, void>>;

    auto const x = io::load_aligned<T, byte_order(Enc)>(src);
    return static_cast<float>(x);
}

template<uint32 Enc>
AMP_INLINE auto state::read(void const* const src) const noexcept ->
    enable_if_t<!is_float(Enc), float>
{
    using T = conditional_t<(sample_size(Enc) == 1), int8,
              conditional_t<(sample_size(Enc) == 2), int16,
              conditional_t<(sample_size(Enc) == 4), int32, void>>>;

    auto x = io::load_aligned<T, byte_order(Enc)>(src);
    x ^= sign;
    return static_cast<float>(x) * scale;
}

template<>
AMP_INLINE float state::read<I24BE>(void const* const src) const noexcept
{
    auto x = lsl(int32{static_cast<uint8 const*>(src)[0]}, 24)
           | lsl(int32{static_cast<uint8 const*>(src)[1]}, 16)
           | lsl(int32{static_cast<uint8 const*>(src)[2]},  8);
    x ^= sign;
    return static_cast<float>(x >> 8) * scale;
}

template<>
AMP_INLINE float state::read<I24LE>(void const* const src) const noexcept
{
    auto x = lsl(int32{static_cast<uint8 const*>(src)[0]},  8)
           | lsl(int32{static_cast<uint8 const*>(src)[1]}, 16)
           | lsl(int32{static_cast<uint8 const*>(src)[2]}, 24);
    x ^= sign;
    return static_cast<float>(x >> 8) * scale;
}


template<uint32 Enc>
inline void convert_generic(uint8 const* const src, std::size_t const n,
                            float* const dst, pcm::state const& st) noexcept
{
    for (auto const i : xrange(n)) {
        dst[i] = st.read<Enc>(&src[i * sample_size(Enc)]);
    }
}

template<uint32 Enc>
inline void convert(void const* const src, std::size_t const n,
                    float* const dst, pcm::state const& st) noexcept
{
    convert_generic<Enc>(static_cast<uint8 const*>(src), n, dst, st);
}

template<>
inline void convert<F32NE>(void const* const src, std::size_t const n,
                           float* const dst, pcm::state const&) noexcept
{
    std::copy_n(static_cast<float const*>(src), n, dst);
}


#if defined(AMP_HAS_X86) || defined(AMP_HAS_X64)

#if __has_warning("-Wcast-align")
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wcast-align"
# pragma clang diagnostic ignored "-Wold-style-cast"
#endif

// ----------------------------------------------------------------------------
// SIMD utility functions.
// ----------------------------------------------------------------------------

AMP_TARGET("sse2")
AMP_INLINE __m128i I24_shuffle_mask(uint32 const enc) noexcept
{
    return (byte_order(enc) == BE)
         ? _mm_setr_epi8(2, 2, 1, 0, 5, 5, 4, 3, 8, 8, 7, 6, 11, 11, 10, 9)
         : _mm_setr_epi8(0, 0, 1, 2, 3, 3, 4, 5, 6, 6, 7, 8, 9, 9, 10, 11);
}

AMP_TARGET("sse")
AMP_INLINE void butterfly(__m128& a, __m128& b) noexcept
{
    __m128 c;
    c = _mm_unpacklo_ps(a, b);
    b = _mm_unpackhi_ps(a, b);
    a = c;
}

AMP_TARGET("avx2")
AMP_INLINE void butterfly(__m256& a, __m256& b) noexcept
{
    __m256 c;
    c = _mm256_unpacklo_ps(a, b);
    b = _mm256_unpackhi_ps(a, b);
    a = _mm256_permute2f128_ps(c, b, 0x20);
    b = _mm256_permute2f128_ps(c, b, 0x31);
}

AMP_TARGET("sse2")
AMP_INLINE auto bswap16(__m128i const x) noexcept
{
    return _mm_or_si128(_mm_slli_epi16(x, 8), _mm_srli_epi16(x, 8));
}

AMP_TARGET("sse2")
AMP_INLINE auto bswap32(__m128i x) noexcept
{
    x = _mm_or_si128(_mm_slli_epi16(x, 8), _mm_srli_epi16(x, 8));
    x = _mm_shufflelo_epi16(x, _MM_SHUFFLE(2, 3, 0, 1));
    x = _mm_shufflehi_epi16(x, _MM_SHUFFLE(2, 3, 0, 1));
    return x;
}

AMP_TARGET("sse2")
AMP_INLINE auto bswap64(__m128i x) noexcept
{
    x = _mm_or_si128(_mm_slli_epi16(x, 8), _mm_srli_epi16(x, 8));
    x = _mm_shufflelo_epi16(x, _MM_SHUFFLE(0, 1, 2, 3));
    x = _mm_shufflehi_epi16(x, _MM_SHUFFLE(0, 1, 2, 3));
    return x;
}

AMP_TARGET("avx2")
AMP_INLINE auto bswap32(__m256i const x) noexcept
{
    return _mm256_shuffle_epi8(
        x, _mm256_broadcastsi128_si256(_mm_setr_epi8(
                 3,  2,  1,  0,
                 7,  6,  5,  4,
                11, 10,  9,  8,
                15, 14, 13, 12)));
}

AMP_TARGET("avx2")
AMP_INLINE auto bswap64(__m256i const x) noexcept
{
    return _mm256_shuffle_epi8(
        x, _mm256_broadcastsi128_si256(_mm_setr_epi8(
                 7,  6,  5,  4,  3,  2,  1,  0,
                15, 14, 13, 12, 11, 10,  9,  8)));
}


// ----------------------------------------------------------------------------
// Vectorized planar conversions.
// ----------------------------------------------------------------------------

AMP_TARGET("sse2")
void pack_2ch_I16LE_sse2(void const* const src, std::size_t const n,
                         float* const dst, pcm::state const& st) noexcept
{
    auto const scale = _mm_set1_ps(st.scale);
    auto const sign = _mm_set1_epi16(static_cast<int16>(st.sign));

    auto L = static_cast<int16 const* const*>(src)[0];
    auto R = static_cast<int16 const* const*>(src)[1];
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 8); i != last; i += 8) {
        auto w0 = _mm_loadu_si128((__m128i const*)&L[i]);
        auto w1 = _mm_loadu_si128((__m128i const*)&R[i]);

        w0 = _mm_xor_si128(w0, sign);
        w1 = _mm_xor_si128(w1, sign);

        auto i0 = _mm_unpacklo_epi16(w0, w0);
        auto i1 = _mm_unpacklo_epi16(w1, w1);
        auto i2 = _mm_unpackhi_epi16(w0, w0);
        auto i3 = _mm_unpackhi_epi16(w1, w1);

        auto f0 = _mm_cvtepi32_ps(_mm_srai_epi32(i0, 16));
        auto f1 = _mm_cvtepi32_ps(_mm_srai_epi32(i1, 16));
        auto f2 = _mm_cvtepi32_ps(_mm_srai_epi32(i2, 16));
        auto f3 = _mm_cvtepi32_ps(_mm_srai_epi32(i3, 16));

        butterfly(f0, f1);
        butterfly(f2, f3);

        _mm_storeu_ps(&dst[i*2 +  0], _mm_mul_ps(f0, scale));
        _mm_storeu_ps(&dst[i*2 +  4], _mm_mul_ps(f1, scale));
        _mm_storeu_ps(&dst[i*2 +  8], _mm_mul_ps(f2, scale));
        _mm_storeu_ps(&dst[i*2 + 12], _mm_mul_ps(f3, scale));
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i*2 + 0] = st.read<I16LE>(&L[i]);
        dst[i*2 + 1] = st.read<I16LE>(&R[i]);
    }
}

AMP_TARGET("avx2")
void pack_2ch_I16LE_avx2(void const* const src, std::size_t const n,
                         float* const dst, pcm::state const& st) noexcept
{
    auto const scale = _mm256_set1_ps(st.scale);
    auto const sign = _mm_set1_epi16(static_cast<int16>(st.sign));

    auto L = static_cast<int16 const* const*>(src)[0];
    auto R = static_cast<int16 const* const*>(src)[1];
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 8); i != last; i += 8) {
        auto w0 = _mm_loadu_si128((__m128i const*)&L[i]);
        auto w1 = _mm_loadu_si128((__m128i const*)&R[i]);

        auto i0 = _mm256_cvtepi16_epi32(_mm_xor_si128(w0, sign));
        auto i1 = _mm256_cvtepi16_epi32(_mm_xor_si128(w1, sign));

        auto f0 = _mm256_cvtepi32_ps(i0);
        auto f1 = _mm256_cvtepi32_ps(i1);

        butterfly(f0, f1);

        _mm256_storeu_ps(&dst[i*2 + 0], _mm256_mul_ps(f0, scale));
        _mm256_storeu_ps(&dst[i*2 + 8], _mm256_mul_ps(f1, scale));
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i*2 + 0] = st.read<I16LE>(&L[i]);
        dst[i*2 + 1] = st.read<I16LE>(&R[i]);
    }
}

AMP_TARGET("sse2")
void pack_2ch_I32LE_sse2(void const* const src, std::size_t const n,
                         float* const dst, pcm::state const& st) noexcept
{
    auto const scale = _mm_set1_ps(st.scale);
    auto const sign = _mm_set1_epi32(static_cast<int32>(st.sign));

    auto L = static_cast<int32 const* const*>(src)[0];
    auto R = static_cast<int32 const* const*>(src)[1];
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 4); i != last; i += 4) {
        auto i0 = _mm_loadu_si128((__m128i const*)&L[i]);
        auto i1 = _mm_loadu_si128((__m128i const*)&R[i]);

        auto f0 = _mm_cvtepi32_ps(_mm_xor_si128(i0, sign));
        auto f1 = _mm_cvtepi32_ps(_mm_xor_si128(i1, sign));

        butterfly(f0, f1);

        _mm_storeu_ps(&dst[i*2 + 0], _mm_mul_ps(f0, scale));
        _mm_storeu_ps(&dst[i*2 + 4], _mm_mul_ps(f1, scale));
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i*2 + 0] = st.read<I32LE>(&L[i]);
        dst[i*2 + 1] = st.read<I32LE>(&R[i]);
    }
}

AMP_TARGET("avx2")
void pack_2ch_I32LE_avx2(void const* const src, std::size_t const n,
                         float* const dst, pcm::state const& st) noexcept
{
    auto const scale = _mm256_set1_ps(st.scale);
    auto const sign = _mm256_set1_epi32(static_cast<int32>(st.sign));

    auto R = static_cast<int32 const* const*>(src)[1];
    auto L = static_cast<int32 const* const*>(src)[0];
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 8); i != last; i += 8) {
        auto i0 = _mm256_loadu_si256((__m256i const*)&L[i]);
        auto i1 = _mm256_loadu_si256((__m256i const*)&R[i]);

        auto f0 = _mm256_cvtepi32_ps(_mm256_xor_si256(i0, sign));
        auto f1 = _mm256_cvtepi32_ps(_mm256_xor_si256(i1, sign));

        butterfly(f0, f1);

        _mm256_storeu_ps(&dst[i*2 + 0], _mm256_mul_ps(f0, scale));
        _mm256_storeu_ps(&dst[i*2 + 8], _mm256_mul_ps(f1, scale));
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i*2 + 0] = st.read<I32LE>(&L[i]);
        dst[i*2 + 1] = st.read<I32LE>(&R[i]);
    }
}

AMP_TARGET("sse")
void pack_2ch_F32LE_sse(void const* const src, std::size_t const n,
                        float* const dst, pcm::state const& st) noexcept
{
    auto L = static_cast<float const* const*>(src)[0];
    auto R = static_cast<float const* const*>(src)[1];
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 4); i != last; i += 4) {
        auto f0 = _mm_loadu_ps(&L[i]);
        auto f1 = _mm_loadu_ps(&R[i]);
        butterfly(f0, f1);

        _mm_storeu_ps(&dst[i*2 + 0], f0);
        _mm_storeu_ps(&dst[i*2 + 4], f1);
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i*2 + 0] = st.read<F32LE>(&L[i]);
        dst[i*2 + 1] = st.read<F32LE>(&R[i]);
    }
}

AMP_TARGET("avx2")
void pack_2ch_F32LE_avx2(void const* const src, std::size_t const n,
                         float* const dst, pcm::state const& st) noexcept
{
    auto R = static_cast<float const* const*>(src)[1];
    auto L = static_cast<float const* const*>(src)[0];
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 8); i != last; i += 8) {
        auto f0 = _mm256_loadu_ps(&L[i]);
        auto f1 = _mm256_loadu_ps(&R[i]);
        butterfly(f0, f1);

        _mm256_storeu_ps(&dst[i*2 + 0], f0);
        _mm256_storeu_ps(&dst[i*2 + 8], f1);
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i*2 + 0] = st.read<F32LE>(&L[i]);
        dst[i*2 + 1] = st.read<F32LE>(&R[i]);
    }
}


inline bool pack_2ch(void const* const src, std::size_t const n,
                     float* const dst, pcm::state const& st) noexcept
{
    AMP_ASSERT(is_aligned(dst, 32));
    AMP_ASSUME(is_aligned(dst, 32));

    if (st.enc == I16LE) {
        if (cpu::has_avx2()) {
            pack_2ch_I16LE_avx2(src, n, dst, st);
            return true;
        }
        if (cpu::has_sse2()) {
            pack_2ch_I16LE_sse2(src, n, dst, st);
            return true;
        }
    }
    else if (st.enc == I32LE) {
        if (cpu::has_avx2()) {
            pack_2ch_I32LE_avx2(src, n, dst, st);
            return true;
        }
        if (cpu::has_sse2()) {
            pack_2ch_I32LE_sse2(src, n, dst, st);
            return true;
        }
    }
    else if (st.enc == F32LE) {
        if (cpu::has_avx2()) {
            pack_2ch_F32LE_avx2(src, n, dst, st);
            return true;
        }
        if (cpu::has_sse()) {
            pack_2ch_F32LE_sse(src, n, dst, st);
            return true;
        }
    }
    return false;
}


// ----------------------------------------------------------------------------
// Vectorized interleaved conversions.
// ----------------------------------------------------------------------------

template<uint32 Enc>
AMP_TARGET("sse2")
void convert_I8_sse2(int8 const* const src, std::size_t const n,
                     float* const dst, pcm::state const& st) noexcept
{
    auto const scale = _mm_set1_ps(st.scale);
    auto const sign = _mm_set1_epi8(static_cast<int8>(st.sign));
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 16); i != last; i += 16) {
        auto b0 = _mm_loadu_si128((__m128i const*)&src[i]);
        b0 = _mm_xor_si128(b0, sign);

        auto w0 = _mm_unpacklo_epi8(b0, b0);
        auto w1 = _mm_unpackhi_epi8(b0, b0);

        auto i0 = _mm_unpacklo_epi16(w0, w0);
        auto i1 = _mm_unpackhi_epi16(w0, w0);
        auto i2 = _mm_unpacklo_epi16(w1, w1);
        auto i3 = _mm_unpackhi_epi16(w1, w1);

        auto f0 = _mm_cvtepi32_ps(_mm_srai_epi32(i0, 24));
        auto f1 = _mm_cvtepi32_ps(_mm_srai_epi32(i1, 24));
        auto f2 = _mm_cvtepi32_ps(_mm_srai_epi32(i2, 24));
        auto f3 = _mm_cvtepi32_ps(_mm_srai_epi32(i3, 24));

        _mm_storeu_ps(&dst[i +  0], _mm_mul_ps(f0, scale));
        _mm_storeu_ps(&dst[i +  4], _mm_mul_ps(f1, scale));
        _mm_storeu_ps(&dst[i +  8], _mm_mul_ps(f2, scale));
        _mm_storeu_ps(&dst[i + 12], _mm_mul_ps(f3, scale));
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i] = st.read<Enc>(&src[i]);
    }
}

template<uint32 Enc>
AMP_TARGET("avx2")
void convert_I8_avx2(int8 const* const src, std::size_t const n,
                     float* const dst, pcm::state const& st) noexcept
{
    auto const scale = _mm256_set1_ps(st.scale);
    auto const sign = _mm_set1_epi8(static_cast<int8>(st.sign));
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 16); i != last; i += 16) {
        auto b0 = _mm_loadu_si128((__m128i const*)&src[i]);
        b0 = _mm_xor_si128(b0, sign);

        auto i0 = _mm256_cvtepi8_epi32(b0);
        auto i1 = _mm256_cvtepi8_epi32(_mm_srli_si128(b0, 8));

        auto f0 = _mm256_cvtepi32_ps(i0);
        auto f1 = _mm256_cvtepi32_ps(i1);

        _mm256_storeu_ps(&dst[i + 0], _mm256_mul_ps(f0, scale));
        _mm256_storeu_ps(&dst[i + 8], _mm256_mul_ps(f1, scale));
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i] = st.read<Enc>(&src[i]);
    }
}

template<uint32 Enc>
AMP_TARGET("sse2")
void convert_I16_sse2(int16 const* const src, std::size_t const n,
                      float* const dst, pcm::state const& st) noexcept
{
    auto const scale = _mm_set1_ps(st.scale);
    auto const sign = _mm_set1_epi16(static_cast<int16>(st.sign));
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 8); i != last; i += 8) {
        auto w0 = _mm_loadu_si128((__m128i const*)&src[i]);
        if (byte_order(Enc) == BE) {
            w0 = bswap16(w0);
        }
        w0 = _mm_xor_si128(w0, sign);

        auto i0 = _mm_unpacklo_epi16(w0, w0);
        auto i1 = _mm_unpackhi_epi16(w0, w0);

        auto f0 = _mm_cvtepi32_ps(_mm_srai_epi32(i0, 16));
        auto f1 = _mm_cvtepi32_ps(_mm_srai_epi32(i1, 16));

        _mm_storeu_ps(&dst[i + 0], _mm_mul_ps(f0, scale));
        _mm_storeu_ps(&dst[i + 4], _mm_mul_ps(f1, scale));
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i] = st.read<Enc>(&src[i]);
    }
}

template<uint32 Enc>
AMP_TARGET("avx2")
void convert_I16_avx2(int16 const* const src, std::size_t const n,
                      float* const dst, pcm::state const& st) noexcept
{
    auto const scale = _mm256_set1_ps(st.scale);
    auto const sign = _mm_set1_epi16(static_cast<int16>(st.sign));
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 8); i != last; i += 8) {
        auto w0 = _mm_loadu_si128((__m128i const*)&src[i]);
        if (byte_order(Enc) == BE) {
            w0 = bswap16(w0);
        }
        auto i0 = _mm256_cvtepi16_epi32(_mm_xor_si128(w0, sign));
        auto f0 = _mm256_cvtepi32_ps(i0);
        _mm256_storeu_ps(&dst[i], _mm256_mul_ps(f0, scale));
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i] = st.read<Enc>(&src[i]);
    }
}

template<uint32 Enc>
AMP_TARGET("ssse3")
void convert_I24_ssse3(uint8 const* const src, std::size_t const n,
                       float* const dst, pcm::state const& st) noexcept
{
    auto const scale = _mm_set1_ps(st.scale);
    auto const sign = _mm_set1_epi32(static_cast<int32>(st.sign));
    auto const mask = I24_shuffle_mask(Enc);
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; (n - i) >= 6; i += 4) {
        auto i0 = _mm_loadu_si128((__m128i const*)&src[i*3]);
        i0 = _mm_shuffle_epi8(i0, mask);
        i0 = _mm_xor_si128(i0, sign);

        auto f0 = _mm_cvtepi32_ps(_mm_srai_epi32(i0, 8));
        _mm_storeu_ps(&dst[i], _mm_mul_ps(f0, scale));
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i] = st.read<Enc>(&src[i*3]);
    }
}

template<uint32 Enc>
AMP_TARGET("avx2")
void convert_I24_avx2(uint8 const* const src, std::size_t const n,
                      float* const dst, pcm::state const& st) noexcept
{
    auto const scale = _mm256_set1_ps(st.scale);
    auto const sign = _mm256_set1_epi32(static_cast<int32>(st.sign));
    auto const mask = _mm256_broadcastsi128_si256(I24_shuffle_mask(Enc));
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; (n - i) >= 11; i += 8) {
        auto i0 = _mm256_loadu2_m128i((__m128i const*)&src[i*3 + 12],
                                      (__m128i const*)&src[i*3 +  0]);
        i0 = _mm256_shuffle_epi8(i0, mask);
        i0 = _mm256_xor_si256(i0, sign);

        auto f0 = _mm256_cvtepi32_ps(_mm256_srai_epi32(i0, 8));
        _mm256_storeu_ps(&dst[i], _mm256_mul_ps(f0, scale));
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i] = st.read<Enc>(&src[i*3]);
    }
}

template<uint32 Enc>
AMP_TARGET("sse2")
void convert_I32_sse2(int32 const* const src, std::size_t const n,
                      float* const dst, pcm::state const& st) noexcept
{
    auto const scale = _mm_set1_ps(st.scale);
    auto const sign = _mm_set1_epi32(static_cast<int32>(st.sign));
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 4); i != last; i += 4) {
        auto i0 = _mm_loadu_si128((__m128i const*)&src[i]);
        if (byte_order(Enc) == BE) {
            i0 = bswap32(i0);
        }
        auto f0 = _mm_cvtepi32_ps(_mm_xor_si128(i0, sign));
        _mm_storeu_ps(&dst[i], _mm_mul_ps(f0, scale));
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i] = st.read<Enc>(&src[i]);
    }
}

template<uint32 Enc>
AMP_TARGET("avx2")
void convert_I32_avx2(int32 const* const src, std::size_t const n,
                      float* const dst, pcm::state const& st) noexcept
{
    auto const scale = _mm256_set1_ps(st.scale);
    auto const sign = _mm256_set1_epi32(static_cast<int32>(st.sign));
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 8); i != last; i += 8) {
        auto i0 = _mm256_loadu_si256((__m256i const*)&src[i]);
        if (byte_order(Enc) == BE) {
            i0 = bswap32(i0);
        }
        auto f0 = _mm256_cvtepi32_ps(_mm256_xor_si256(i0, sign));
        _mm256_storeu_ps(&dst[i], _mm256_mul_ps(f0, scale));
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i] = st.read<Enc>(&src[i]);
    }
}

template<uint32 Enc>
AMP_TARGET("sse2")
void convert_F32_sse2(float const* const src, std::size_t const n,
                      float* const dst, pcm::state const& st) noexcept
{
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 4); i != last; i += 4) {
        auto f0 = _mm_loadu_ps(&src[i]);
        if (byte_order(Enc) == BE) {
            f0 = _mm_castsi128_ps(bswap32(_mm_castps_si128(f0)));
        }
        _mm_storeu_ps(&dst[i], f0);
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i] = st.read<Enc>(&src[i]);
    }
}

template<uint32 Enc>
AMP_TARGET("avx2")
void convert_F32_avx2(float const* const src, std::size_t const n,
                      float* const dst, pcm::state const& st) noexcept
{
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 8); i != last; i += 8) {
        auto f0 = _mm256_loadu_ps(&src[i]);
        if (byte_order(Enc) == BE) {
            f0 = _mm256_castsi256_ps(bswap32(_mm256_castps_si256(f0)));
        }
        _mm256_storeu_ps(&dst[i], f0);
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i] = st.read<Enc>(&src[i]);
    }
}

template<uint32 Enc>
AMP_TARGET("sse2")
void convert_F64_sse2(double const* const src, std::size_t const n,
                      float* const dst, pcm::state const& st) noexcept
{
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 4); i != last; i += 4) {
        auto d0 = _mm_loadu_pd(&src[i + 0]);
        auto d1 = _mm_loadu_pd(&src[i + 2]);
        if (byte_order(Enc) == BE) {
            d0 = _mm_castsi128_pd(bswap64(_mm_castpd_si128(d0)));
            d1 = _mm_castsi128_pd(bswap64(_mm_castpd_si128(d1)));
        }
        auto f0 = _mm_cvtpd_ps(d0);
        auto f1 = _mm_cvtpd_ps(d1);
        _mm_storeu_ps(&dst[i], _mm_movelh_ps(f0, f1));
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i] = st.read<Enc>(&src[i]);
    }
}

template<uint32 Enc>
AMP_TARGET("avx2")
void convert_F64_avx2(double const* const src, std::size_t const n,
                      float* const dst, pcm::state const& st) noexcept
{
    auto i = 0_sz;

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (auto const last = align_down(n, 4); i != last; i += 4) {
        auto d0 = _mm256_loadu_pd(&src[i]);
        if (byte_order(Enc) == BE) {
            d0 = _mm256_castsi256_pd(bswap64(_mm256_castpd_si256(d0)));
        }
        _mm_storeu_ps(&dst[i], _mm256_cvtpd_ps(d0));
    }

    AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
    for (; i != n; ++i) {
        dst[i] = st.read<Enc>(&src[i]);
    }
}


#define SPECIALIZE_CONVERT(Enc, Rep, F, A, B)                               \
template<>                                                                  \
inline void convert<Enc>(void const* const src, std::size_t const n,        \
                         float* const dst, pcm::state const& st) noexcept   \
{                                                                           \
    if (cpu::has_##A()) {                                                   \
        F##_##A<Enc>((Rep const*)src, n, dst, st);                          \
    }                                                                       \
    else if (cpu::has_##B()) {                                              \
        F##_##B<Enc>((Rep const*)src, n, dst, st);                          \
    }                                                                       \
    else {                                                                  \
        convert_generic<Enc>((uint8 const*)src, n, dst, st);                \
    }                                                                       \
}

SPECIALIZE_CONVERT(I8,    int8,   convert_I8,  avx2, sse2)
SPECIALIZE_CONVERT(I16LE, int16,  convert_I16, avx2, sse2)
SPECIALIZE_CONVERT(I16BE, int16,  convert_I16, avx2, sse2)
SPECIALIZE_CONVERT(I24LE, uint8,  convert_I24, avx2, ssse3)
SPECIALIZE_CONVERT(I24BE, uint8,  convert_I24, avx2, ssse3)
SPECIALIZE_CONVERT(I32LE, int32,  convert_I32, avx2, sse2)
SPECIALIZE_CONVERT(I32BE, int32,  convert_I32, avx2, sse2)
SPECIALIZE_CONVERT(F32BE, float,  convert_F32, avx2, sse2)
SPECIALIZE_CONVERT(F64LE, double, convert_F64, avx2, sse2)
SPECIALIZE_CONVERT(F64BE, double, convert_F64, avx2, sse2)

#if __has_warning("-Wcast-align")
# pragma clang diagnostic pop
#endif

#endif  // AMP_HAS_X86 || AMP_HAS_X64


AMP_NOINLINE
void convert(void const* const src, std::size_t const n,
             float* const dst, pcm::state const& st) noexcept
{
    switch (st.enc) {
    case I8:    return pcm::convert<I8   >(src, n, dst, st);
    case I16LE: return pcm::convert<I16LE>(src, n, dst, st);
    case I16BE: return pcm::convert<I16BE>(src, n, dst, st);
    case I24LE: return pcm::convert<I24LE>(src, n, dst, st);
    case I24BE: return pcm::convert<I24BE>(src, n, dst, st);
    case I32LE: return pcm::convert<I32LE>(src, n, dst, st);
    case I32BE: return pcm::convert<I32BE>(src, n, dst, st);
    case F32LE: return pcm::convert<F32LE>(src, n, dst, st);
    case F32BE: return pcm::convert<F32BE>(src, n, dst, st);
    case F64LE: return pcm::convert<F64LE>(src, n, dst, st);
    case F64BE: return pcm::convert<F64BE>(src, n, dst, st);
    }
    AMP_UNREACHABLE();
}


constexpr float compute_scale(uint32 const bps) noexcept
{
    return 1.f / static_cast<float>(uint32{1} << (bps - 1));
}

pcm::state make_state(pcm::spec spec)
{
    if (spec.channels < audio::min_channels ||
        spec.channels > audio::max_channels) {
        raise(errc::unsupported_format, "invalid channel count: %" PRIu32,
              spec.channels);
    }

    if (spec.flags & pcm::ieee_float) {
        if (spec.bytes_per_sample != 4 && spec.bytes_per_sample != 8) {
            raise(errc::unsupported_format, "invalid PCM spec");
        }
    }
    else {
        if (spec.bytes_per_sample == 1) {
            spec.flags &= ~pcm::big_endian;
        }
        else if (spec.bytes_per_sample < 2 || spec.bytes_per_sample > 4) {
            raise(errc::unsupported_format, "invalid PCM spec");
        }
    }

    if (spec.bits_per_sample == 0) {
        spec.bits_per_sample = spec.bytes_per_sample * 8;
    }
    else if (spec.bits_per_sample > (spec.bytes_per_sample * 8)) {
        raise(errc::unsupported_format, "invalid PCM spec");
    }

    pcm::state st;

    st.enc = uint32{!!(spec.flags & pcm::big_endian)};
    if (spec.flags & pcm::ieee_float) {
        st.enc |= (1 << 3);
        st.enc |= (((spec.bytes_per_sample >> 2) - 1) << 1);
    }
    else {
        st.enc |= ((spec.bytes_per_sample - 1) << 1);
        st.sign = (spec.flags & pcm::signed_int)
                ? uint32{0}
                : uint32{1} << (ceil_pow2(spec.bytes_per_sample * 8) - 1);
        st.scale = (spec.flags & pcm::aligned_high)
                 ? compute_scale(spec.bytes_per_sample * 8)
                 : compute_scale(spec.bits_per_sample);
    }
    return st;
}


class blitter_impl final :
    public blitter
{
public:
    explicit blitter_impl(pcm::spec const& spec) :
        channels(spec.channels),
        st(pcm::make_state(spec)),
        interleaved(!(spec.flags & pcm::non_interleaved))
    {}

    void convert(void const* const src, std::size_t const frames,
                 audio::packet& pkt) override
    {
        if (AMP_LIKELY(frames != 0)) {
            AMP_ASSERT(src != nullptr);

            auto const samples = frames * channels;
            pkt.resize(samples, uninitialized);

            if (interleaved) {
                pcm::convert(src, samples, pkt.data(), st);
            }
            else {
                auto const planes = static_cast<void const* const*>(src);
                convert_planar(planes, frames, pkt.data());
            }
        }
    }

private:
    void convert_planar(void const* const* const src, std::size_t const frames,
                        float* const dst)
    {
#if defined(AMP_HAS_X86) || defined(AMP_HAS_X64)
        if (channels == 2 && pack_2ch(src, frames, dst, st)) {
            return;
        }
#endif
        if (channels == 1) {
            return pcm::convert(src[0], frames, dst, st);
        }

        if (st.enc != F32NE) {
            tmpbuf.resize(frames, uninitialized);
        }

        for (auto const c : xrange(channels)) {
            float const* plane;
            if (st.enc == F32NE) {
                plane = reinterpret_cast<float const*>(src[c]);
            }
            else {
                pcm::convert(src[c], frames, tmpbuf.data(), st);
                plane = tmpbuf.data();
            }

            auto out = dst + c;
            for (auto const i : xrange(frames)) {
                *out = plane[i];
                out += channels;
            }
        }
    }

    audio::packet_buffer tmpbuf;
    uint32 channels;
    pcm::state st;
    bool interleaved;
};

}     // namespace <unnamed>


std::unique_ptr<blitter> blitter::create(pcm::spec const& spec)
{
    return std::make_unique<blitter_impl>(spec);
}

}     // namespace pcm


namespace {

constexpr int16 alaw_to_lpcm(uint8 v) noexcept
{
    v ^= 0xd5;
    auto x = ((v & 0x0f) << 4) + 8;
    if (v & 0x70) {
        x = (x + 256) << (((v & 0x70) >> 4) - 1);
    }
    return static_cast<int16>((v & 0x80) ? -x : x);
}

constexpr int16 ulaw_to_lpcm(uint8 v) noexcept
{
    v = ~v;
    auto x = (((v & 0x0f) | 0x10) << 1) + 1;
    x = x << (((v & 0x70) >> 4) + 2);
    x = x - 0x84;
    return static_cast<int16>((v & 0x80) ? -x : x);
}


template<int16 F(uint8), std::size_t... I>
constexpr auto make_decode_table(std::index_sequence<I...>) noexcept
{ return std::array<int16, 256> {{ F(uint8{I})... }}; }

template<int16 F(uint8)>
constexpr auto make_decode_table() noexcept
{ return make_decode_table<F>(std::make_index_sequence<256>()); }

constexpr auto alaw_table = make_decode_table<alaw_to_lpcm>();
constexpr auto ulaw_table = make_decode_table<ulaw_to_lpcm>();


class g711_decoder
{
public:
    explicit g711_decoder(audio::codec_format const& fmt) noexcept :
        decode_table_(fmt.codec_id == codec::alaw ? alaw_table : ulaw_table)
    {}

    void send(io::buffer& buf) noexcept
    {
        source_data_ = buf.data();
        source_size_ = buf.size();
    }

    auto recv(audio::packet& pkt)
    {
        constexpr auto scale = pcm::compute_scale(16);

        pkt.resize(source_size_, uninitialized);
        for (auto const i : xrange(source_size_)) {
            pkt[i] = decode_table_[source_data_[i]] * scale;
        }
        return audio::decode_status::none;
    }

    void flush() noexcept
    {
        source_data_ = nullptr;
        source_size_ = 0;
    }

    uint32 get_decoder_delay() const noexcept
    {
        return 0;
    }

private:
    std::array<int16, 256> const& decode_table_;
    uint8 const* source_data_{};
    std::size_t source_size_{};
};

AMP_REGISTER_DECODER(
    g711_decoder,
    audio::codec::alaw,
    audio::codec::ulaw);


class lpcm_decoder
{
public:
    explicit lpcm_decoder(audio::codec_format const& fmt) :
        blitter_([&]{
            pcm::spec spec;
            spec.bytes_per_sample = fmt.bytes_per_packet / fmt.channels;
            spec.bits_per_sample  = fmt.bits_per_sample;
            spec.channels         = fmt.channels;
            spec.flags            = fmt.flags;
            return spec;
        }()),
        bytes_per_frame_(fmt.bytes_per_packet)
    {}

    void send(io::buffer& buf) noexcept
    {
        source_data_ = buf.data();
        source_size_ = buf.size();
    }

    auto recv(audio::packet& pkt)
    {
        blitter_.convert(source_data_, source_size_ / bytes_per_frame_, pkt);
        return audio::decode_status::none;
    }

    void flush() noexcept
    {
        source_data_ = nullptr;
        source_size_ = 0;
    }

    uint32 get_decoder_delay() const noexcept
    {
        return 0;
    }

private:
    pcm::blitter_impl blitter_;
    uint32 const bytes_per_frame_;
    uint8 const* source_data_{};
    std::size_t source_size_{};
};

AMP_REGISTER_DECODER(
    lpcm_decoder,
    audio::codec::lpcm);

}}}   // namespace amp::audio::<unnamed>

