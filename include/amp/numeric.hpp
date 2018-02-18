////////////////////////////////////////////////////////////////////////////////
//
// amp/numeric.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_EAAFB9C9_B515_4590_9528_13479E7F3854
#define AMP_INCLUDED_EAAFB9C9_B515_4590_9528_13479E7F3854


#include <amp/error.hpp>
#include <amp/optional.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <limits>
#include <type_traits>


namespace amp {

template<typename T> constexpr T pi      = T(3.14159265358979323846e-0L);
template<typename T> constexpr T ln2     = T(6.93147180559945309417e-1L);
template<typename T> constexpr T ln10    = T(2.30258509299404568402e-0L);
template<typename T> constexpr T sqrt2   = T(1.41421356237309504880e-0L);
template<typename T> constexpr T sqrt1_2 = T(7.07106781186547524401e-1L);
template<typename T> constexpr T inf     = std::numeric_limits<T>::infinity();


namespace aux {

#if __has_warning("-Wsign-compare")
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wsign-compare"
#endif

template<typename T, typename U>
AMP_INLINE constexpr auto would_overflow_(U const x) noexcept ->
    enable_if_t<is_integral_v<T> && is_signed_v<U>, bool>
{
    return (x < std::numeric_limits<T>::min())
        || (x > std::numeric_limits<T>::max());
}

template<typename T, typename U>
AMP_INLINE constexpr auto would_overflow_(U const x) noexcept ->
    enable_if_t<is_integral_v<T> && is_unsigned_v<U>, bool>
{
    return (x > std::numeric_limits<T>::max());
}

template<typename T, typename U>
AMP_INLINE constexpr auto would_overflow_(U const x) noexcept ->
    enable_if_t<is_floating_point_v<T>, bool>
{
    return (x != static_cast<U>(static_cast<T>(x)));
}

#if __has_warning("-Wsign-compare")
# pragma clang diagnostic pop
#endif


template<typename T, typename U>
constexpr bool is_lossless_conversion_v_ =
    !is_floating_point_v<T> &&
    !is_floating_point_v<U> &&
    ((sizeof(T) >= sizeof(U) && is_signed_v<T> == is_signed_v<U>) ||
     (sizeof(T) > sizeof(U) && is_signed_v<T> && is_unsigned_v<U>));

template<typename T>
constexpr bool is_lossless_conversion_v_<T, T> = true;

}     // namespace aux


template<typename T, typename U>
AMP_INLINE constexpr auto numeric_cast(U const x) ->
    enable_if_t<!aux::is_lossless_conversion_v_<T, U>, T>
{
    static_assert(is_arithmetic_v<T> && is_arithmetic_v<U>, "");
    if (!aux::would_overflow_<T>(x)) {
        return static_cast<T>(x);
    }
    raise(errc::arithmetic_overflow);
}

template<typename T, typename U>
AMP_INLINE constexpr auto numeric_cast(U const x) noexcept ->
    enable_if_t<aux::is_lossless_conversion_v_<T, U>, T>
{
    static_assert(is_arithmetic_v<T> && is_arithmetic_v<U>, "");
    return static_cast<T>(x);
}


template<typename T, typename U>
AMP_INLINE constexpr auto numeric_try_cast(U const x) noexcept ->
    enable_if_t<!aux::is_lossless_conversion_v_<T, U>, optional<T>>
{
    static_assert(is_arithmetic_v<T> && is_arithmetic_v<U>, "");
    if (!aux::would_overflow_<T>(x)) {
        return make_optional(static_cast<T>(x));
    }
    return nullopt;
}

template<typename T, typename U>
AMP_INLINE constexpr auto numeric_try_cast(U const x) noexcept ->
    enable_if_t<aux::is_lossless_conversion_v_<T, U>, optional<T>>
{
    static_assert(is_arithmetic_v<T> && is_arithmetic_v<U>, "");
    return optional<T>{static_cast<T>(x)};
}

}     // namespace amp


#endif  // AMP_INCLUDED_EAAFB9C9_B515_4590_9528_13479E7F3854

