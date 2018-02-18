////////////////////////////////////////////////////////////////////////////////
//
// amp/audio/utility.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_C1D6B065_BABE_421A_A71C_D577CDB69D62
#define AMP_INCLUDED_C1D6B065_BABE_421A_A71C_D577CDB69D62


#include <amp/numeric.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <cmath>


namespace amp {
namespace audio {

template<typename T>
AMP_INLINE auto to_amplitude(T const x) noexcept ->
    enable_if_t<is_floating_point_v<T>, T>
{
    // Equivalent to `std::pow(10, x / 20)`
    return std::exp(x * (ln10<T> / T{20}));
}

template<typename T>
AMP_INLINE auto to_amplitude(T const x) noexcept ->
    enable_if_t<is_integral_v<T>, double>
{
    return to_amplitude(static_cast<double>(x));
}


template<typename T>
AMP_INLINE auto to_decibels(T const x) noexcept ->
    enable_if_t<is_floating_point_v<T>, T>
{
    return std::log10(x) * T{20};
}

template<typename T>
AMP_INLINE auto to_decibels(T const x) noexcept ->
    enable_if_t<is_integral_v<T>, double>
{
    return to_decibels(static_cast<double>(x));
}

}}    // namespace amp::audio


#endif  // AMP_INCLUDED_C1D6B065_BABE_421A_A71C_D577CDB69D62

