////////////////////////////////////////////////////////////////////////////////
//
// amp/bitops.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_E656CFDD_20B5_4047_9930_E6A02809EBC8
#define AMP_INCLUDED_E656CFDD_20B5_4047_9930_E6A02809EBC8


#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>
#include <amp/utility.hpp>

#include <climits>
#include <cstddef>


namespace amp {

template<typename T>
constexpr std::size_t bitsof = sizeof(T) * CHAR_BIT;


template<typename T>
AMP_INLINE constexpr int sgn(T const x) noexcept
{
    static_assert(is_integral_v<T> && is_signed_v<T>, "");
    return (x > 0) - (x < 0);
}

template<typename T>
AMP_INLINE constexpr T lsl(T const x, uint const n) noexcept
{
    AMP_ASSERT(n < bitsof<T>);
    return static_cast<T>(as_unsigned(x) << n);
}

template<typename T>
AMP_INLINE constexpr T lsr(T const x, uint const n) noexcept
{
    AMP_ASSERT(n < bitsof<T>);
    return static_cast<T>(as_unsigned(x) >> n);
}

template<typename T>
AMP_INLINE constexpr T asr(T const x, uint const n) noexcept
{
    AMP_ASSERT(n < bitsof<T>);
    return static_cast<T>(as_signed(x) >> n);
}

template<typename T>
AMP_INLINE constexpr T rol(T const x, uint const n) noexcept
{
    AMP_ASSERT(n < bitsof<T>);
    return static_cast<T>(lsl(x, n) | lsr(x, -n & (bitsof<T> - 1)));
}

template<typename T>
AMP_INLINE constexpr T ror(T const x, uint const n) noexcept
{
    AMP_ASSERT(n < bitsof<T>);
    return static_cast<T>(lsr(x, n) | lsl(x, -n & (bitsof<T> - 1)));
}


template<typename T>
AMP_INLINE constexpr auto popcnt(T x) noexcept ->
    enable_if_t<is_unsigned_v<T>, uint>
{
    constexpr auto k1 = static_cast<T>(-1ULL / 3);
    constexpr auto k2 = static_cast<T>(-1ULL / 5);
    constexpr auto k4 = static_cast<T>(-1ULL / 17);
    constexpr auto kf = static_cast<T>(-1ULL / 255);

    x =  x       - ((x >> 1)  & k1);
    x = (x & k2) + ((x >> 2)  & k2);
    x = (x       +  (x >> 4)) & k4;
    x = (x * kf) >> (bitsof<T> - bitsof<char>);
    return static_cast<uint>(x);
}

template<typename T>
AMP_INLINE constexpr auto popcnt(T const x) noexcept ->
    enable_if_t<is_signed_v<T>, uint>
{
    return popcnt(as_unsigned(x));
}

#if __has_builtin(__builtin_popcount) || AMP_GCC_PREREQ(4, 0)
template<> AMP_INLINE constexpr uint popcnt(uchar const x) noexcept
{ return static_cast<uint>(__builtin_popcount(x)); }

template<> AMP_INLINE constexpr uint popcnt(ushort const x) noexcept
{ return static_cast<uint>(__builtin_popcount(x)); }

template<> AMP_INLINE constexpr uint popcnt(uint const x) noexcept
{ return static_cast<uint>(__builtin_popcount(x)); }
#endif

#if __has_builtin(__builtin_popcountl) || AMP_GCC_PREREQ(4, 0)
template<> AMP_INLINE constexpr uint popcnt(ulong const x) noexcept
{ return static_cast<uint>(__builtin_popcountl(x)); }
#endif

#if __has_builtin(__builtin_popcountll) || AMP_GCC_PREREQ(4, 0)
template<> AMP_INLINE constexpr uint popcnt(ullong const x) noexcept
{ return static_cast<uint>(__builtin_popcountll(x)); }
#endif


template<typename T>
AMP_INLINE constexpr auto lzcnt(T x) noexcept ->
    enable_if_t<is_unsigned_v<T>, uint>
{
    if (x == 0) {
        return bitsof<T>;
    }

    auto n = uint{0};
    for (auto s = uint{bitsof<T> / 2}; s != 0; s /= 2) {
        if (x <= (T(-1) >> s)) {
            n += s;
            x <<= s;
        }
    }
    return n;
}

template<typename T>
AMP_INLINE constexpr auto lzcnt(T const x) noexcept ->
    enable_if_t<is_signed_v<T>, uint>
{
    return lzcnt(as_unsigned(x));
}

#if __has_builtin(__builtin_clz) || AMP_GCC_PREREQ(4, 0)
template<> AMP_INLINE constexpr uint lzcnt(uint const x) noexcept
{ return x != 0 ? static_cast<uint>(__builtin_clz(x)) : bitsof<uint>; }

template<> AMP_INLINE constexpr uint lzcnt(ushort const x) noexcept
{ return lzcnt<uint>(x) - (bitsof<uint> - bitsof<ushort>); }

template<> AMP_INLINE constexpr uint lzcnt(uchar const x) noexcept
{ return lzcnt<uint>(x) - (bitsof<uint> - bitsof<uchar>); }
#endif

#if __has_builtin(__builtin_clzl) || AMP_GCC_PREREQ(4, 0)
template<> AMP_INLINE constexpr uint lzcnt(ulong const x) noexcept
{ return x != 0 ? static_cast<uint>(__builtin_clzl(x)) : bitsof<ulong>; }
#endif

#if __has_builtin(__builtin_clzll) || AMP_GCC_PREREQ(4, 0)
template<> AMP_INLINE constexpr uint lzcnt(ullong const x) noexcept
{ return x != 0 ? static_cast<uint>(__builtin_clzll(x)) : bitsof<ullong>; }
#endif


template<typename T>
AMP_INLINE constexpr uint tzcnt(T const x) noexcept
{ return popcnt(static_cast<T>((x & -x) - 1)); }

#if __has_builtin(__builtin_ctz) || AMP_GCC_PREREQ(4, 0)
template<> AMP_INLINE constexpr uint tzcnt(uchar const x) noexcept
{ return x != 0 ? static_cast<uint>(__builtin_ctz(x)) : bitsof<uchar>; }

template<> AMP_INLINE constexpr uint tzcnt(ushort const x) noexcept
{ return x != 0 ? static_cast<uint>(__builtin_ctz(x)) : bitsof<ushort>; }

template<> AMP_INLINE constexpr uint tzcnt(uint const x) noexcept
{ return x != 0 ? static_cast<uint>(__builtin_ctz(x)) : bitsof<uint>; }
#endif

#if __has_builtin(__builtin_ctzl) || AMP_GCC_PREREQ(4, 0)
template<> AMP_INLINE constexpr uint tzcnt(ulong const x) noexcept
{ return x != 0 ? static_cast<uint>(__builtin_ctzl(x)) : bitsof<ulong>; }
#endif

#if __has_builtin(__builtin_ctzll) || AMP_GCC_PREREQ(4, 0)
template<> AMP_INLINE constexpr uint tzcnt(ullong const x) noexcept
{ return x != 0 ? static_cast<uint>(__builtin_ctzll(x)) : bitsof<ullong>; }
#endif


template<typename T>
AMP_INLINE constexpr bool is_pow2(T const x) noexcept
{
    return (x > 0) && ((x & (x - 1)) == 0);
}

// Returns the nearest power of two greater than or equal to x.
template<typename T>
AMP_INLINE constexpr T ceil_pow2(T const x) noexcept
{
    return (x != 0) ? lsl(T{1}, (-lzcnt<T>(x - 1) & (bitsof<T> - 1))) : 0;
}

// Returns the nearest power of two less than or equal to x.
template<typename T>
AMP_INLINE constexpr T floor_pow2(T const x) noexcept
{
    return (x != 0) ? lsl(T{1}, (bitsof<T> - 1 - lzcnt(x))) : 0;
}

// Integral floor(log2(x)).
template<typename T>
AMP_INLINE constexpr T ilog2(T const x) noexcept
{
    AMP_ASSERT(x > 0);
    return static_cast<T>(bitsof<T> - 1 - lzcnt(x));
}


// ----------------------------------------------------------------------------
// Integral alignment operations
// ----------------------------------------------------------------------------

template<typename T>
AMP_INLINE constexpr bool is_aligned(T const x, std::size_t const a) noexcept
{
    AMP_ASSERT(is_pow2(a));
    return (x & (a - 1)) == 0;
}

template<typename T>
AMP_INLINE constexpr T align_up(T const x, std::size_t const a) noexcept
{
    AMP_ASSERT(is_pow2(a));
    return static_cast<T>((x + (a - 1)) & -a);
}

template<typename T>
AMP_INLINE constexpr T align_down(T const x, std::size_t const a) noexcept
{
    AMP_ASSERT(is_pow2(a));
    return static_cast<T>(x & static_cast<T>(-a));
}


// ----------------------------------------------------------------------------
// Address alignment operations
// ----------------------------------------------------------------------------

template<typename T>
AMP_READNONE
AMP_INLINE bool is_aligned(T* const p, std::size_t const a) noexcept
{ return is_aligned(reinterpret_cast<uintptr>(p), a); }

template<typename T>
AMP_READNONE
AMP_INLINE T* align_up(T* const p, std::size_t const a) noexcept
{ return reinterpret_cast<T*>(align_up(reinterpret_cast<uintptr>(p), a)); }

template<typename T>
AMP_READNONE
AMP_INLINE T* align_down(T* const p, std::size_t const a) noexcept
{ return reinterpret_cast<T*>(align_down(reinterpret_cast<uintptr>(p), a)); }

}     // namespace amp


#endif  // AMP_INCLUDED_E656CFDD_20B5_4047_9930_E6A02809EBC8

