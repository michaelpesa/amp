////////////////////////////////////////////////////////////////////////////////
//
// amp/stddef.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_6737BC1C_6866_4C55_B578_D087B2BEEF29
#define AMP_INCLUDED_6737BC1C_6866_4C55_B578_D087B2BEEF29


#include <amp/aux/features.hpp>

#include <cstdint>


#define AMP_RETURNS(...) noexcept(                                          \
    noexcept(__VA_ARGS__)) ->                                               \
    decltype(__VA_ARGS__)                                                   \
    { return __VA_ARGS__; }                                                 \
    static_assert(true, "")


namespace amp {

using schar   =   signed char;
using uchar   = unsigned char;
using ushort  = unsigned short;
using uint    = unsigned int;
using ulong   = unsigned long;
using ullong  = unsigned long long;

using char16  = char16_t;
using char32  = char32_t;

using int8    = ::std::int8_t;
using int16   = ::std::int16_t;
using int32   = ::std::int32_t;
using int64   = ::std::int64_t;
using intmax  = ::std::intmax_t;
using intptr  = ::std::intptr_t;

using uint8   = ::std::uint8_t;
using uint16  = ::std::uint16_t;
using uint32  = ::std::uint32_t;
using uint64  = ::std::uint64_t;
using uintmax = ::std::uintmax_t;
using uintptr = ::std::uintptr_t;


// FIXME: find a better header to put this in.
constexpr struct uninitialized_t {} uninitialized{};


inline namespace literals {

AMP_INLINE constexpr auto operator"" _sz(ullong const x) noexcept
{
    return static_cast<decltype(sizeof(x))>(x);
}

}}    // inline namespace amp::literals


#endif  // AMP_INCLUDED_6737BC1C_6866_4C55_B578_D087B2BEEF29

