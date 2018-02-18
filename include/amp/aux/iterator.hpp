////////////////////////////////////////////////////////////////////////////////
//
// amp/aux/iterator.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_38E4DEEF_3018_4D99_A507_6F9C37241ED3
#define AMP_INCLUDED_38E4DEEF_3018_4D99_A507_6F9C37241ED3


#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <iterator>
#include <type_traits>
#include <utility>


namespace amp {
namespace adl {

using ::std::begin;
using ::std::end;

template<typename T>
AMP_INLINE constexpr auto adl_begin(T&& t)
AMP_RETURNS(begin(std::forward<T>(t)));

template<typename T>
AMP_INLINE constexpr auto adl_end(T&& t)
AMP_RETURNS(end(std::forward<T>(t)));

}     // namespace adl

using adl::adl_begin;
using adl::adl_end;


template<typename T> using iterator_category_t =
    typename std::iterator_traits<T>::iterator_category;

template<typename T> using iterator_value_t =
    typename std::iterator_traits<T>::value_type;

template<typename T> using iterator_difference_t =
    typename std::iterator_traits<T>::difference_type;

template<typename T> using iterator_pointer_t =
    typename std::iterator_traits<T>::pointer;

template<typename T> using iterator_reference_t =
    typename std::iterator_traits<T>::reference;


template<typename T> constexpr bool is_output_iterator_v =
    is_convertible_v<iterator_category_t<T>, std::output_iterator_tag>;

template<typename T> constexpr bool is_input_iterator_v =
    is_convertible_v<iterator_category_t<T>, std::input_iterator_tag>;

template<typename T> constexpr bool is_forward_iterator_v =
    is_convertible_v<iterator_category_t<T>, std::forward_iterator_tag>;

template<typename T> constexpr bool is_bidirectional_iterator_v =
    is_convertible_v<iterator_category_t<T>, std::bidirectional_iterator_tag>;

template<typename T> constexpr bool is_random_access_iterator_v =
    is_convertible_v<iterator_category_t<T>, std::random_access_iterator_tag>;


namespace aux {

template<typename T, typename Enable = void>
struct is_iterable_ : std::false_type {};

template<typename T>
struct is_iterable_<
    T,
    void_t<
        decltype(adl_begin(std::declval<T>())),
        decltype(adl_end(std::declval<T>()))
    >
> : std::true_type {};


template<typename T, typename Enable = void>
struct iterator_type_ {};

template<typename T>
struct iterator_type_<T, enable_if_t<is_iterable_<T>::value>>
{
    using type = common_type_t<
        decltype(adl_begin(std::declval<T>())),
        decltype(adl_end(std::declval<T>()))
    >;
};

}     // namespace aux

template<typename T>
struct iterator_type : aux::iterator_type_<T> {};

template<typename T>
using iterator_type_t = typename iterator_type<T>::type;

}     // namespace amp


#endif  // AMP_INCLUDED_38E4DEEF_3018_4D99_A507_6F9C37241ED3

