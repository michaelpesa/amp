////////////////////////////////////////////////////////////////////////////////
//
// amp/cxp/map.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_15DC24CB_D503_4568_8A05_7D7FFCE9F162
#define AMP_INCLUDED_15DC24CB_D503_4568_8A05_7D7FFCE9F162


#include <amp/aux/map_value_compare.hpp>
#include <amp/cxp/string.hpp>
#include <amp/stddef.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string_view>


namespace amp {
namespace cxp {

template<
    typename Key,
    typename T,
    std::size_t Size,
    typename Compare = std::less<>
>
class map
{
public:
    using mapped_type     = T;
    using key_type        = Key;
    using key_compare     = Compare;
    using value_type      = std::pair<key_type, mapped_type>;
    using value_compare   = map_value_compare<Key, T, Compare>;
    using reference       = value_type const&;
    using const_reference = value_type const&;
    using pointer         = value_type const*;
    using const_pointer   = value_type const*;
    using iterator        = value_type const*;
    using const_iterator  = value_type const*;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

    constexpr auto key_comp() const
    { return key_compare(); }

    constexpr auto value_comp() const
    { return value_compare(); }

    constexpr size_type size() const noexcept
    { return Size; }

    constexpr const_iterator begin() const noexcept
    { return cbegin(); }

    constexpr const_iterator end() const noexcept
    { return cend(); }

    constexpr const_iterator cbegin() const noexcept
    { return values_; }

    constexpr const_iterator cend() const noexcept
    { return values_ + size(); }

    template<typename K>
    auto find(K const& k) const
    {
        auto const it = lower_bound(k);
        return it != end() && !value_comp()(k, *it) ? it : end();
    }

    template<typename K>
    auto count(K const& k) const
    {
        auto const r = equal_range(k);
        return static_cast<size_type>(r.second - r.first);
    }

    template<typename K>
    auto lower_bound(K const& k) const
    { return std::lower_bound(begin(), end(), k, value_comp()); }

    template<typename K>
    auto upper_bound(K const& k) const
    { return std::upper_bound(begin(), end(), k, value_comp()); }

    template<typename K>
    auto equal_range(K const& k) const
    { return std::equal_range(begin(), end(), k, value_comp()); }

    auto find(key_type const& k) const
    { return find<key_type>(k); }

    auto count(key_type const& k) const
    { return count<key_type>(k); }

    auto lower_bound(key_type const& k) const
    { return lower_bound<key_type>(k); }

    auto upper_bound(key_type const& k) const
    { return upper_bound<key_type>(k); }

    auto equal_range(key_type const& k) const
    { return equal_range<key_type>(k); }

    value_type values_[Size];
};


template<typename InIt, typename Compare = std::less<>>
constexpr bool is_sorted(InIt first, InIt const last, Compare comp = Compare())
{
    if (first != last) {
        for  (auto next = first; ++next != last; first = next) {
            if (comp(*next, *first)) {
                return false;
            }
        }
    }
    return true;
}

template<typename Key, typename T, std::size_t N, typename Compare>
constexpr bool is_sorted(cxp::map<Key, T, N, Compare> const& m)
{
    using value_compare =
        conditional_t<
            is_same_v<Key, char const*> || is_same_v<Key, std::string_view>,
            map_value_compare<Key, T, cxp::strcmp_less>,
            map_value_compare<Key, T, Compare>
        >;

    return cxp::is_sorted(m.begin(), m.end(), value_compare());
}

}}    // namespace amp::cxp


#endif  // AMP_INCLUDED_15DC24CB_D503_4568_8A05_7D7FFCE9F162

