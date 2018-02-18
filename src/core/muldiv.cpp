////////////////////////////////////////////////////////////////////////////////
//
// core/muldiv.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/bitops.hpp>
#include <amp/muldiv.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>
#include <amp/utility.hpp>

#include <limits>

#if defined(_MSC_VER) && defined(_M_X64)
# include <intrin.h>
# pragma intrinsic(_umul128)
#endif


namespace amp {
namespace aux {
namespace {

AMP_INLINE bool muldiv_overflow_(uint64 const a, uint64 const b,
                                 uint64 const c, uint64& ret) noexcept
{
    uint64 u0, u1;
    {
#if defined(__SIZEOF_INT128__)
        auto const u = __uint128_t{a} * b;
        u0 = static_cast<uint64>(u);
        u1 = static_cast<uint64>(u >> 64);
#elif defined(_MSC_VER) && defined(_M_X64)
        u0 = _umul128(a, b, &u1);
#else
        auto const a1 = a >> 32;
        auto const a0 = a & 0xffffffff;
        auto const b1 = b >> 32;
        auto const b0 = b & 0xffffffff;
        auto const t1 = a0 * b1 + a1 * b0;

        u0 = a0 * b0 + (t1 << 32);
        u1 = a1 * b1 + (t1 >> 32) + (u0 < (t1 << 32));
#endif
    }

    u0 += (c >> 1);
    u1 += (u0 < (c >> 1));
    if (AMP_UNLIKELY(u1 >= c)) {
        return true;
    }

#if defined(__GNUC__) && defined(__x86_64__)
    asm("divq %3"
        : "=a"(ret)
        : "%0"(u0), "d"(u1), "r"(c));
#else
    auto v = c;
    auto un64 = u1;
    auto un10 = u0;

    auto const s = lzcnt(v);
    if (s != 0) {
        v    = (v  << s);
        un64 = (u1 << s) | (u0 >> (64 - s));
        un10 = (u0 << s);
    }

    auto const vn1 = v >> 32;
    auto const vn0 = v & 0xffffffff;

    auto const un1 = un10 >> 32;
    auto const un0 = un10 & 0xffffffff;

    auto q1 = un64 / vn1;
    auto rhat = un64 - (q1 * vn1);

    constexpr auto base = uint64{1} << 32;

again1:
    if (q1 >= base || q1 * vn0 > base * rhat + un1) {
        q1 -= 1;
        rhat += vn1;
        if (rhat < base) {
            goto again1;
        }
    }

    auto un21 = un64 * base + un1 - q1 * v;
    auto q0 = un21 / vn1;
    rhat = un21 - q0 * vn1;

again2:
    if (q0 >= base || q0 * vn0 > base * rhat + un0) {
        q0 -= 1;
        rhat += vn1;
        if (rhat < base) {
            goto again2;
        }
    }
    ret = q1 * base + q0;
#endif
    return false;
}

AMP_INLINE bool muldiv_overflow_(uint32 const a, uint32 const b,
                                 uint32 const c, uint32& ret) noexcept
{
    auto const u = uint64{a} * b + (c >> 1);
    if (AMP_LIKELY((u >> 32) < c)) {
        ret = static_cast<uint32>(u / c);
        return false;
    }
    return true;
}


template<typename T>
AMP_INLINE T umuldiv_(T const a, T const b, T const c) noexcept
{
    T ret;
    if (AMP_UNLIKELY(muldiv_overflow_(a, b, c, ret))) {
        ret = std::numeric_limits<T>::max();
    }
    return ret;
}

template<typename T>
AMP_INLINE T imuldiv_(T const a, T const b, T const c) noexcept
{
    auto s_a = as_unsigned(a >> (bitsof<T> - 1));
    auto s_b = as_unsigned(b >> (bitsof<T> - 1));
    auto s_c = as_unsigned(c >> (bitsof<T> - 1));
    auto ret = as_unsigned(std::numeric_limits<T>::max());

    auto const overflow = muldiv_overflow_((as_unsigned(a) ^ s_a) - s_a,
                                           (as_unsigned(b) ^ s_b) - s_b,
                                           (as_unsigned(c) ^ s_c) - s_c,
                                           ret);
    s_a ^= s_b;
    s_a ^= s_c;
    if (AMP_LIKELY(!overflow)) {
        ret ^= s_a;
    }
    return as_signed(ret - s_a);
}

}     // namespace <unnamed>


int32 imuldiv32_(int32 const a, int32 const b, int32 const c) noexcept
{ return imuldiv_(a, b, c); }

int64 imuldiv64_(int64 const a, int64 const b, int64 const c) noexcept
{ return imuldiv_(a, b, c); }

uint32 umuldiv32_(uint32 const a, uint32 const b, uint32 const c) noexcept
{ return umuldiv_(a, b, c); }

uint64 umuldiv64_(uint64 const a, uint64 const b, uint64 const c) noexcept
{ return umuldiv_(a, b, c); }

}}    // namespace amp::aux

