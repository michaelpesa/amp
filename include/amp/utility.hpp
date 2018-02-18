////////////////////////////////////////////////////////////////////////////////
//
// amp/utility.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_F485BED1_B17F_4A72_96FB_629A36A3644C
#define AMP_INCLUDED_F485BED1_B17F_4A72_96FB_629A36A3644C


#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <cstddef>


#define AMP_DEFINE_ENUM_FLAG_OPERATORS(E)                                   \
    AMP_INLINE constexpr E operator~(E const x) noexcept                    \
    { return E(~::amp::as_underlying(x)); }                                 \
    AMP_INLINE constexpr E operator&(E const x, E const y) noexcept         \
    { return E(::amp::as_underlying(x) & ::amp::as_underlying(y)); }        \
    AMP_INLINE constexpr E operator|(E const x, E const y) noexcept         \
    { return E(::amp::as_underlying(x) | ::amp::as_underlying(y)); }        \
    AMP_INLINE constexpr E operator^(E const x, E const y) noexcept         \
    { return E(::amp::as_underlying(x) ^ ::amp::as_underlying(y)); }        \
    AMP_INLINE E& operator&=(E& x, E const y) noexcept                      \
    { return x = (x & y); }                                                 \
    AMP_INLINE E& operator|=(E& x, E const y) noexcept                      \
    { return x = (x | y); }                                                 \
    AMP_INLINE E& operator^=(E& x, E const y) noexcept                      \
    { return x = (x ^ y); }                                                 \
    static_assert(true, "")


namespace amp {

template<typename T>
AMP_INLINE constexpr auto as_signed(T const x) noexcept
{
    static_assert(is_integral_v<T>, "argument must be integral");
    return static_cast<make_signed_t<T>>(x);
}

template<typename T>
AMP_INLINE constexpr auto as_unsigned(T const x) noexcept
{
    static_assert(is_integral_v<T>, "argument must be integral");
    return static_cast<make_unsigned_t<T>>(x);
}

template<typename T>
AMP_INLINE constexpr auto as_underlying(T const x) noexcept
{
    static_assert(is_enum_v<T>, "argument must be an enum type");
    return static_cast<underlying_type_t<T>>(x);
}


constexpr uint32 operator"" _4cc(char const* const s, std::size_t) noexcept
{
    return (uint32{static_cast<uint8>(s[0])} << 24)
         | (uint32{static_cast<uint8>(s[1])} << 16)
         | (uint32{static_cast<uint8>(s[2])} <<  8)
         | (uint32{static_cast<uint8>(s[3])} <<  0);
}

}     // namespace amp


#endif  // AMP_INCLUDED_F485BED1_B17F_4A72_96FB_629A36A3644C

