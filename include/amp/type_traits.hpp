////////////////////////////////////////////////////////////////////////////////
//
// amp/type_traits.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_873F9441_5938_4929_80BB_E4D3E3FF0B51
#define AMP_INCLUDED_873F9441_5938_4929_80BB_E4D3E3FF0B51


#include <amp/stddef.hpp>

#include <cstddef>
#include <type_traits>
#include <utility>


namespace amp {

////////////////////////////////////////////////////////////////////////////////
// C++14 additions to <type_traits>
////////////////////////////////////////////////////////////////////////////////

#define AMP_ALIAS_TT(X) \
    template<typename T> \
    using X##_t = typename std::X<T>::type

AMP_ALIAS_TT(remove_const);
AMP_ALIAS_TT(remove_volatile);
AMP_ALIAS_TT(remove_cv);
AMP_ALIAS_TT(add_const);
AMP_ALIAS_TT(add_volatile);
AMP_ALIAS_TT(add_cv);
AMP_ALIAS_TT(remove_reference);
AMP_ALIAS_TT(add_lvalue_reference);
AMP_ALIAS_TT(add_rvalue_reference);
AMP_ALIAS_TT(remove_pointer);
AMP_ALIAS_TT(add_pointer);
AMP_ALIAS_TT(make_signed);
AMP_ALIAS_TT(make_unsigned);
AMP_ALIAS_TT(remove_extent);
AMP_ALIAS_TT(remove_all_extents);
AMP_ALIAS_TT(decay);
AMP_ALIAS_TT(underlying_type);
AMP_ALIAS_TT(result_of);

#undef AMP_ALIAS_TT

template<bool Cond, typename T = void>
using enable_if_t = typename std::enable_if<Cond, T>::type;
template<bool Cond, typename T, typename F>
using conditional_t = typename std::conditional<Cond, T, F>::type;
template<typename... T>
using common_type_t = typename std::common_type<T...>::type;
template<std::size_t... Args>
using aligned_storage_t = typename std::aligned_storage<Args...>::type;
template<std::size_t Len, typename... T>
using aligned_union_t = typename std::aligned_union<Len, T...>::type;


////////////////////////////////////////////////////////////////////////////////
// C++17 additions to <type_traits>
////////////////////////////////////////////////////////////////////////////////

template<typename...>
using void_t = void;
template<bool B>
using bool_constant = std::integral_constant<bool, B>;

#define AMP_VAR_TT_1(X) \
    template<typename T> \
    constexpr bool X##_v = std::X<T>::value
#define AMP_VAR_TT_2(X) \
    template<typename T, typename U> \
    constexpr bool X##_v = std::X<T, U>::value
#define AMP_VAR_TT_N(X) \
    template<typename T, typename... Args> \
    constexpr bool X##_v = std::X<T, Args...>::value

AMP_VAR_TT_1(is_final);
AMP_VAR_TT_1(is_void);
AMP_VAR_TT_1(is_null_pointer);
AMP_VAR_TT_1(is_integral);
AMP_VAR_TT_1(is_floating_point);
AMP_VAR_TT_1(is_array);
AMP_VAR_TT_1(is_enum);
AMP_VAR_TT_1(is_union);
AMP_VAR_TT_1(is_class);
AMP_VAR_TT_1(is_function);
AMP_VAR_TT_1(is_pointer);
AMP_VAR_TT_1(is_lvalue_reference);
AMP_VAR_TT_1(is_rvalue_reference);
AMP_VAR_TT_1(is_member_object_pointer);
AMP_VAR_TT_1(is_member_function_pointer);
AMP_VAR_TT_1(is_fundamental);
AMP_VAR_TT_1(is_arithmetic);
AMP_VAR_TT_1(is_scalar);
AMP_VAR_TT_1(is_object);
AMP_VAR_TT_1(is_compound);
AMP_VAR_TT_1(is_reference);
AMP_VAR_TT_1(is_member_pointer);
AMP_VAR_TT_1(is_const);
AMP_VAR_TT_1(is_volatile);
AMP_VAR_TT_1(is_trivial);
AMP_VAR_TT_1(is_trivially_copyable);
AMP_VAR_TT_1(is_standard_layout);
AMP_VAR_TT_1(is_pod);
AMP_VAR_TT_1(is_literal_type);
AMP_VAR_TT_1(is_empty);
AMP_VAR_TT_1(is_polymorphic);
AMP_VAR_TT_1(is_abstract);
AMP_VAR_TT_1(is_signed);
AMP_VAR_TT_1(is_unsigned);
AMP_VAR_TT_N(is_constructible);
AMP_VAR_TT_N(is_trivially_constructible);
AMP_VAR_TT_N(is_nothrow_constructible);
AMP_VAR_TT_1(is_default_constructible);
AMP_VAR_TT_1(is_trivially_default_constructible);
AMP_VAR_TT_1(is_nothrow_default_constructible);
AMP_VAR_TT_1(is_copy_constructible);
AMP_VAR_TT_1(is_trivially_copy_constructible);
AMP_VAR_TT_1(is_nothrow_copy_constructible);
AMP_VAR_TT_1(is_move_constructible);
AMP_VAR_TT_1(is_trivially_move_constructible);
AMP_VAR_TT_1(is_nothrow_move_constructible);
AMP_VAR_TT_2(is_assignable);
AMP_VAR_TT_2(is_trivially_assignable);
AMP_VAR_TT_2(is_nothrow_assignable);
AMP_VAR_TT_1(is_copy_assignable);
AMP_VAR_TT_1(is_trivially_copy_assignable);
AMP_VAR_TT_1(is_nothrow_copy_assignable);
AMP_VAR_TT_1(is_move_assignable);
AMP_VAR_TT_1(is_trivially_move_assignable);
AMP_VAR_TT_1(is_nothrow_move_assignable);
AMP_VAR_TT_1(is_destructible);
AMP_VAR_TT_1(is_trivially_destructible);
AMP_VAR_TT_1(is_nothrow_destructible);
AMP_VAR_TT_1(has_virtual_destructor);
AMP_VAR_TT_2(is_same);
AMP_VAR_TT_2(is_base_of);
AMP_VAR_TT_2(is_convertible);

#undef AMP_VAR_TT_1
#undef AMP_VAR_TT_2
#undef AMP_VAR_TT_N

template<typename T, uint N = 0>
constexpr std::size_t extent_v = std::extent<T, N>::value;
template<typename T>
constexpr std::size_t rank_v = std::rank<T>::value;


namespace aux {

using std::swap;

template<
    typename T,
    typename = decltype(swap(std::declval<T&>(), std::declval<T&>()))
>
std::true_type is_swappable_(int);

template<typename>
std::false_type is_swappable_(...);

template<typename T>
auto is_nothrow_swappable_(int) ->
    bool_constant<noexcept(swap(std::declval<T&>(), std::declval<T&>()))>;

template<typename>
std::false_type is_nothrow_swappable_(...);

}     // namespace aux

template<typename T>
using is_swappable = decltype(aux::is_swappable_<T>(0));
template<typename T>
using is_nothrow_swappable = decltype(aux::is_nothrow_swappable_<T>(0));

template<typename T>
constexpr bool is_swappable_v = is_swappable<T>::value;
template<typename T>
constexpr bool is_nothrow_swappable_v = is_nothrow_swappable<T>::value;


////////////////////////////////////////////////////////////////////////////////
// Miscellaneous type trait utilities.
////////////////////////////////////////////////////////////////////////////////

template<typename T> constexpr bool is_byte_v = false;
template<> constexpr bool is_byte_v<char> = true;
template<> constexpr bool is_byte_v<schar> = true;
template<> constexpr bool is_byte_v<uchar> = true;

}     // namespace amp


#endif  // AMP_INCLUDED_873F9441_5938_4929_80BB_E4D3E3FF0B51

