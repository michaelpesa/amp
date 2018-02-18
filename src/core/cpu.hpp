////////////////////////////////////////////////////////////////////////////////
//
// core/cpu.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_7D650308_46BC_474E_8B2D_58E5A10CF8FF
#define AMP_INCLUDED_7D650308_46BC_474E_8B2D_58E5A10CF8FF


#include <amp/stddef.hpp>
#include <amp/utility.hpp>


#if defined(__clang__)
# define AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION \
   _Pragma("clang loop unroll(disable) vectorize(disable)")
#else
# define AMP_DISABLE_LOOP_UNROLLING_AND_VECTORIZATION
#endif

#if defined(__x86_64__) || defined(_M_X64)
# define AMP_HAS_X64
#elif defined(__i386__) || defined(_M_IX86)
# define AMP_HAS_X86
#endif

#if defined(AMP_HAS_X86) || defined(AMP_HAS_X64)

# define AMP_HAS_X86_FEATURES
# if defined(__AVX2__)
#  define AMP_CPU_FEATURE_LEVEL 8
# elif defined(__AVX__)
#  define AMP_CPU_FEATURE_LEVEL 7
# elif defined(__SSE4_2__)
#  define AMP_CPU_FEATURE_LEVEL 6
# elif defined(__SSE4_1__)
#  define AMP_CPU_FEATURE_LEVEL 5
# elif defined(__SSSE3__)
#  define AMP_CPU_FEATURE_LEVEL 4
# elif defined(__SSE3__)
#  define AMP_CPU_FEATURE_LEVEL 3
# elif defined(__SSE2__) || defined(AMP_HAS_X64)
#  define AMP_CPU_FEATURE_LEVEL 2
# elif defined(__SSE__)
#  define AMP_CPU_FEATURE_LEVEL 1
# elif defined(_M_IX86_FP)
#  define AMP_CPU_FEATURE_LEVEL _M_IX86_FP
# else
#  define AMP_CPU_FEATURE_LEVEL 0
# endif

#if (AMP_CPU_FEATURE_LEVEL >= 1)
# define AMP_HAS_SSE
#  if (AMP_CPU_FEATURE_LEVEL >= 2)
#   define AMP_HAS_SSE2
#  endif
#endif

#elif defined(__linux__) && (defined(__arm__) || defined(__aarch64__))

# define AMP_HAS_ARM_FEATURES
# if defined(__aarch64__)
#  define AMP_HAS_ARM64_FEATURES
#  define AMP_CPU_FEATURE_LEVEL 1
# else
#  define AMP_HAS_ARM32_FEATURES
#  define AMP_CPU_FEATURE_LEVEL 0
# endif

#endif


namespace amp {

#if defined(__zarch__) || defined(__s390x__) || defined(__SYSC_ZARCH__)
constexpr auto cache_line_size = 256_sz;
#elif defined(__aarch64__)
constexpr auto cache_line_size = 128_sz;
#elif defined(__x86_64__) || defined(_M_X64)
constexpr auto cache_line_size = 64_sz;
#elif defined(__arm__) || defined(_M_ARM)
constexpr auto cache_line_size = 64_sz;
#elif defined(__sparc) && defined(__arch64__)
constexpr auto cache_line_size = 64_sz;
#elif defined(__powerpc64__) || defined(__ppc64__)
constexpr auto cache_line_size = 64_sz;
#else
constexpr auto cache_line_size = 32_sz;
#endif


#if defined(AMP_HAS_X86_FEATURES) || defined(AMP_HAS_ARM_FEATURES)

namespace cpu {

enum class feature : uint32 {
    none       = 0,
#if defined(AMP_HAS_X86_FEATURES)
    sse        = (1 <<  0),
    sse2       = (1 <<  1),
    sse3       = (1 <<  2),
    ssse3      = (1 <<  3),
    sse4_1     = (1 <<  4),
    sse4_2     = (1 <<  5),
    avx        = (1 <<  6),
    avx2       = (1 <<  7),
    fma3       = (1 <<  8),
    fma4       = (1 <<  9),
    xop        = (1 << 10),
    avx512f    = (1 << 11),
    avx512cd   = (1 << 12),
    avx512er   = (1 << 13),
    avx512pf   = (1 << 14),
    avx512bw   = (1 << 15),
    avx512dq   = (1 << 16),
    avx512vl   = (1 << 17),
    avx512ifma = (1 << 18),
    avx512vbmi = (1 << 19),
#elif defined(AMP_HAS_ARM_FEATURES)
    neon       = (1 <<  0),
    aes        = (1 <<  1),
    crc32      = (1 <<  2),
    pmull      = (1 <<  3),
    sha1       = (1 <<  4),
    sha2       = (1 <<  5),
#endif
};
AMP_DEFINE_ENUM_FLAG_OPERATORS(feature);

extern cpu::feature const detected_features;

constexpr cpu::feature const compiler_features =
    static_cast<cpu::feature>((1 << AMP_CPU_FEATURE_LEVEL) - 1);


#define AMP_CPU_FEATURE_TEST(Name)                                          \
    AMP_INLINE constexpr bool has_##Name() noexcept                         \
    {                                                                       \
        return ((compiler_features & feature::Name) != feature::none)       \
            || ((detected_features & feature::Name) != feature::none);      \
    }


#if defined(AMP_HAS_X86_FEATURES)

AMP_CPU_FEATURE_TEST(sse)
AMP_CPU_FEATURE_TEST(sse2)
AMP_CPU_FEATURE_TEST(sse3)
AMP_CPU_FEATURE_TEST(ssse3)
AMP_CPU_FEATURE_TEST(sse4_1)
AMP_CPU_FEATURE_TEST(sse4_2)
AMP_CPU_FEATURE_TEST(avx)
AMP_CPU_FEATURE_TEST(avx2)
AMP_CPU_FEATURE_TEST(fma3)
AMP_CPU_FEATURE_TEST(fma4)
AMP_CPU_FEATURE_TEST(xop)
AMP_CPU_FEATURE_TEST(avx512f)
AMP_CPU_FEATURE_TEST(avx512cd)
AMP_CPU_FEATURE_TEST(avx512er)
AMP_CPU_FEATURE_TEST(avx512pf)
AMP_CPU_FEATURE_TEST(avx512bw)
AMP_CPU_FEATURE_TEST(avx512dq)
AMP_CPU_FEATURE_TEST(avx512vl)
AMP_CPU_FEATURE_TEST(avx512ifma)
AMP_CPU_FEATURE_TEST(avx512vbmi)

#elif defined(AMP_HAS_ARM_FEATURES)

AMP_CPU_FEATURE_TEST(neon)
AMP_CPU_FEATURE_TEST(aes)
AMP_CPU_FEATURE_TEST(crc32)
AMP_CPU_FEATURE_TEST(pmull)
AMP_CPU_FEATURE_TEST(sha1)
AMP_CPU_FEATURE_TEST(sha2)

#endif

#undef AMP_CPU_FEATURE_TEST

}     // namespace cpu

#endif  // AMP_HAS_X86_FEATURES || AMP_HAS_ARM_FEATURES

}     // namespace amp


#endif  // AMP_INCLUDED_7D650308_46BC_474E_8B2D_58E5A10CF8FF

