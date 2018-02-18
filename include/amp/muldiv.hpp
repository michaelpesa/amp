////////////////////////////////////////////////////////////////////////////////
//
// amp/muldiv.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_02E8E03A_EE2B_4704_ADD7_B95D0B3E328E
#define AMP_INCLUDED_02E8E03A_EE2B_4704_ADD7_B95D0B3E328E


#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>


namespace amp {
namespace aux {

AMP_EXPORT AMP_READNONE int32 imuldiv32_(int32, int32, int32) noexcept;
AMP_EXPORT AMP_READNONE int64 imuldiv64_(int64, int64, int64) noexcept;

AMP_EXPORT AMP_READNONE uint32 umuldiv32_(uint32, uint32, uint32) noexcept;
AMP_EXPORT AMP_READNONE uint64 umuldiv64_(uint64, uint64, uint64) noexcept;

}     // namespace aux


template<typename T>
[[nodiscard]] AMP_READNONE
AMP_INLINE auto muldiv(T const a,
                       common_type_t<T> const b,
                       common_type_t<T> const c) noexcept ->
    enable_if_t<is_signed_v<T> && (sizeof(T) <= sizeof(int32)), int32>
{
    return aux::imuldiv32_(a, b, c);
}

template<typename T>
[[nodiscard]] AMP_READNONE
AMP_INLINE auto muldiv(T const a,
                       common_type_t<T> const b,
                       common_type_t<T> const c) noexcept ->
    enable_if_t<is_signed_v<T> && (sizeof(T) == sizeof(int64)), int64>
{
    return aux::imuldiv64_(a, b, c);
}

template<typename T>
[[nodiscard]] AMP_READNONE
AMP_INLINE auto muldiv(T const a,
                       common_type_t<T> const b,
                       common_type_t<T> const c) noexcept ->
    enable_if_t<is_unsigned_v<T> && (sizeof(T) <= sizeof(uint32)), uint32>
{
    return aux::umuldiv32_(a, b, c);
}

template<typename T>
[[nodiscard]] AMP_READNONE
AMP_INLINE auto muldiv(T const a,
                       common_type_t<T> const b,
                       common_type_t<T> const c) noexcept ->
    enable_if_t<is_unsigned_v<T> && (sizeof(T) == sizeof(uint64)), uint64>
{
    return aux::umuldiv64_(a, b, c);
}

}     // namespace amp


#endif  // AMP_INCLUDED_02E8E03A_EE2B_4704_ADD7_B95D0B3E328E

