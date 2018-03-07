////////////////////////////////////////////////////////////////////////////////
//
// core/cpu.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/stddef.hpp>

#include "core/cpu.hpp"

#if defined(AMP_CPU_FEATURE_LEVEL)
# include <array>
# include <tuple>
# include <utility>
# if defined(_MSC_VER)
#  include <intrin.h>
# elif defined(__clang__) || defined(__GNUC__)
#  include <x86intrin.h>
# else
#  error "unsupported compiler for target x86(_64)"
# endif
#endif


#if defined(AMP_CPU_FEATURE_LEVEL)

namespace amp {
namespace cpu {
namespace {

AMP_INLINE uint64 xgetbv(uint32 const xcr) noexcept
{
#if defined(_MSC_VER)
    return _xgetbv(xcr);
#else
    uint32 lo, hi;
    asm volatile(".byte 0x0f, 0x01, 0xd0"
                 : "=a"(lo), "=d"(hi)
                 : "c"(xcr));
    return (uint64{hi} << 32) | lo;
#endif
}

AMP_INLINE auto cpuid(uint32 const func) noexcept
{
    std::array<uint32, 4> r;
#if defined(_MSC_VER)
    __cpuidex(reinterpret_cast<int*>(r.data()), static_cast<int>(func), 0);
#else
    asm("cpuid"
        : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3])
        : "%0"(func), "2"(0));
#endif
    return r;
}

AMP_INLINE uint32 get_max_cpuid_standard_function() noexcept
{
#if !defined(AMP_HAS_SSE)
    auto const a = __readeflags();
    __writeeflags(a ^ 0x00200000);
    if (a == __readeflags()) {
        return 0;
    }
#endif
    return cpuid(0x00000000)[0];
}

AMP_INLINE uint32 get_max_cpuid_extended_function() noexcept
{
    return cpuid(0x80000000)[0];
}

AMP_INLINE bool have_avx_support() noexcept
{
    return (xgetbv(0) & 6) == 6;
}

AMP_INLINE auto detect_features() noexcept
{
    auto ret = feature::none;

    auto const max_std_func = get_max_cpuid_standard_function();
    if (max_std_func == 0) { goto done; }       // no CPUID

    uint32 eax, ebx, ecx, edx;
    std::tie(eax, ebx, ecx, edx) = cpuid(1);

    if (!(edx & (1U <<  0))) { goto done; }     // no FPU
    if (!(edx & (1U << 23))) { goto done; }     // no MMX
    if (!(edx & (1U << 24))) { goto done; }     // no FXSAVE / FXRSTOR
    if (!(edx & (1U << 25))) { goto done; }     // no SSE
    ret |= feature::sse;

    if (!(edx & (1U << 26))) { goto done; }     // no SSE2
    ret |= feature::sse2;

    if (!(ecx & (1U <<  0))) { goto done; }     // no SSE3
    ret |= feature::sse3;

    if (!(ecx & (1U <<  9))) { goto done; }     // no SSSE3
    ret |= feature::ssse3;

    if (!(ecx & (1U << 19))) { goto done; }     // no SSE4.1
    ret |= feature::sse4_1;

    if (!(ecx & (1U << 20))) { goto done; }     // no SSE4.2
    ret |= feature::sse4_2;

    if (!(ecx & (1U << 27))) { goto done; }     // no OSXSAVE
    if (!(ecx & (1U << 28))) { goto done; }     // no AVX
    if (!have_avx_support()) { goto done; }     // no AVX support in OS
    ret |= feature::avx;

    // Check for optional instruction sets that depend on AVX
    if (ecx & (1U << 12)) { ret |= feature::fma3; }

    if (get_max_cpuid_extended_function() >= 0x80000001) {
        std::tie(eax, ebx, ecx, edx) = cpuid(0x80000001);

        if (ecx & (1U << 11)) { ret |= feature::xop;  }
        if (ecx & (1U << 16)) { ret |= feature::fma4; }
    }

    if (max_std_func < 7) { goto done; }
    std::tie(eax, ebx, ecx, edx) = cpuid(7);

    if (!(ebx & (1U <<  5))) { goto done; }     // no AVX2
    ret |= feature::avx2;

    if (!(ebx & (1U << 16))) { goto done; }     // no AVX512F
    ret |= feature::avx512f;

    // Check for individual AVX-512 instruction sets.
    if (ebx & (1U << 28)) { ret |= feature::avx512cd;   }
    if (ebx & (1U << 27)) { ret |= feature::avx512er;   }
    if (ebx & (1U << 26)) { ret |= feature::avx512pf;   }
    if (ebx & (1U << 30)) { ret |= feature::avx512bw;   }
    if (ebx & (1U << 17)) { ret |= feature::avx512dq;   }
    if (ebx & (1U << 31)) { ret |= feature::avx512vl;   }
    if (ebx & (1U << 21)) { ret |= feature::avx512ifma; }
    if (ecx & (1U <<  1)) { ret |= feature::avx512vbmi; }

done:
    return ret;
}

}     // namespace <unnamed>

cpu::feature const detected_features = cpu::detect_features();

}}    // namespace amp::cpu

#endif  // AMP_CPU_FEATURE_LEVEL

