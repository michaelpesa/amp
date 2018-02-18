////////////////////////////////////////////////////////////////////////////////
//
// amp/aux/map_value_compare.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_3F429888_FFA1_4E9E_8071_E449D1E8CC5C
#define AMP_INCLUDED_3F429888_FFA1_4E9E_8071_E449D1E8CC5C


#include <amp/aux/compressed_pair.hpp>
#include <amp/stddef.hpp>

#include <functional>
#include <utility>


namespace amp {

template<typename Key, typename T, typename Compare = std::less<>>
class map_value_compare :
    private compressed_base<Compare>
{
public:
    using value_type = std::pair<Key, T>;
    using compressed_base<Compare>::compressed_base;

    constexpr bool operator()(value_type const& x, value_type const& y) const
    { return key_comp()(x.first, y.first); }

    template<typename K>
    constexpr bool operator()(value_type const& x, K const& y) const
    { return key_comp()(x.first, y); }

    template<typename K>
    constexpr bool operator()(K const& x, value_type const& y) const
    { return key_comp()(x, y.first); }

    constexpr decltype(auto) key_comp() const& noexcept
    { return static_cast<compressed_base<Compare> const&>(*this).get(); }

    constexpr decltype(auto) key_comp() & noexcept
    { return static_cast<compressed_base<Compare>&>(*this).get(); }

    constexpr decltype(auto) key_comp() && noexcept
    { return static_cast<compressed_base<Compare>&&>(*this).get(); }

    void swap(map_value_compare& x)
    noexcept(is_nothrow_swappable_v<Compare>)
    {
        using std::swap;
        swap(key_comp(), x.key_comp());
    }
};


template<typename Key, typename T, typename Compare>
void swap(map_value_compare<Key, T, Compare>& x,
          map_value_compare<Key, T, Compare>& y)
noexcept(noexcept(x.swap(y)))
{
    x.swap(y);
}

}     // namespace amp


#endif  // AMP_INCLUDED_3F429888_FFA1_4E9E_8071_E449D1E8CC5C

