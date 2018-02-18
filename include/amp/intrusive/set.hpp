////////////////////////////////////////////////////////////////////////////////
//
// amp/intrusive/set.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_62F84B2D_B58A_4D93_8EFC_57976F05F7D6
#define AMP_INCLUDED_62F84B2D_B58A_4D93_8EFC_57976F05F7D6


#include <amp/intrusive/rbtree.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <cstddef>
#include <functional>
#include <utility>


namespace amp {
namespace intrusive {

template<typename Tag = void>
using set_link = rbtree_link<Tag>;


template<
    typename T,
    typename Compare = std::less<>,
    typename Tag = void
>
class set
{
private:
    using tree_type = intrusive::rbtree<T, Compare, Tag>;

    tree_type tree_;

public:
    using value_type             = typename tree_type::value_type;
    using value_compare          = typename tree_type::value_compare;
    using pointer                = typename tree_type::pointer;
    using const_pointer          = typename tree_type::const_pointer;
    using iterator               = typename tree_type::iterator;
    using const_iterator         = typename tree_type::const_iterator;
    using reverse_iterator       = typename tree_type::reverse_iterator;
    using const_reverse_iterator = typename tree_type::const_reverse_iterator;
    using size_type              = typename tree_type::size_type;
    using difference_type        = typename tree_type::difference_type;

    set() = default;
    set(set&&) = default;
    set& operator=(set&&) & = default;

    constexpr explicit set(value_compare const& comp)
    noexcept(is_nothrow_copy_constructible_v<value_compare>) :
        tree_(comp)
    {}

    void swap(set& x)
    noexcept(is_nothrow_swappable_v<tree_type>)
    { tree_.swap(x.tree_); }

    constexpr auto size() const noexcept
    { return tree_.size(); }

    constexpr auto empty() const noexcept
    { return tree_.empty(); }

    constexpr auto value_comp() const
    noexcept(is_nothrow_copy_constructible_v<value_compare>)
    { return tree_.value_comp(); }

    auto begin() noexcept
    { return tree_.begin(); }

    auto end() noexcept
    { return tree_.end(); }

    constexpr auto begin() const noexcept
    { return tree_.begin(); }

    constexpr auto end() const noexcept
    { return tree_.end(); }

    constexpr auto cbegin() const noexcept
    { return tree_.cbegin(); }

    constexpr auto cend() const noexcept
    { return tree_.cend(); }

    auto rbegin() noexcept
    { return tree_.rbegin(); }

    auto rend() noexcept
    { return tree_.rend(); }

    auto rbegin() const noexcept
    { return tree_.rbegin(); }

    auto rend() const noexcept
    { return tree_.rend(); }

    auto crbegin() const noexcept
    { return tree_.crbegin(); }

    auto crend() const noexcept
    { return tree_.crend(); }

    template<typename K>
    auto lower_bound(K const& k) noexcept
    { return tree_.lower_bound(k); }

    template<typename K>
    auto lower_bound(K const& k) const noexcept
    { return tree_.lower_bound(k); }

    template<typename K>
    auto upper_bound(K const& k) noexcept
    { return tree_.upper_bound(k); }

    template<typename K>
    auto upper_bound(K const& k) const noexcept
    { return tree_.upper_bound(k); }

    template<typename K>
    auto equal_range(K const& k) noexcept
    { return tree_.equal_range_unique(k); }

    template<typename K>
    auto equal_range(K const& k) const noexcept
    { return tree_.equal_range_unique(k); }

    template<typename K>
    auto count(K const& k) noexcept
    { return tree_.count_unique(k); }

    template<typename K>
    auto find(K const& k) noexcept
    { return tree_.find(k); }

    template<typename K>
    auto find(K const& k) const noexcept
    { return tree_.find(k); }

    auto erase(const_iterator pos) noexcept
    { return tree_.erase(pos); }

    auto erase(const_iterator first, const_iterator last) noexcept
    { return tree_.erase(first, last); }

    auto clear() noexcept
    { return tree_.clear(); }

    template<typename Dispose>
    auto clear_and_dispose(Dispose dispose) noexcept
    { return tree_.clear_and_dispose(std::move(dispose)); }

    auto insert(value_type& v) noexcept
    { return tree_.insert_unique(v); }

    auto insert(const_iterator pos, value_type& v) noexcept
    { return tree_.insert_unique(pos, v); }

    template<typename InIt>
    auto insert(InIt first, InIt last)
    { return tree_.insert_unique(first, last); }

    static auto iterator_to(pointer const x) noexcept
    { return tree_type::iterator_to(x); }

    static constexpr auto iterator_to(const_pointer const x) noexcept
    { return tree_type::iterator_to(x); }

    static constexpr auto const_iterator_to(const_pointer const x) noexcept
    { return tree_type::const_iterator_to(x); }
};


template<
    typename T,
    typename Compare = std::less<>,
    typename Tag = void
>
class multiset
{
private:
    using tree_type = intrusive::rbtree<T, Compare, Tag>;

    tree_type tree_;

public:
    using value_type             = typename tree_type::value_type;
    using value_compare          = typename tree_type::value_compare;
    using pointer                = typename tree_type::pointer;
    using const_pointer          = typename tree_type::const_pointer;
    using iterator               = typename tree_type::iterator;
    using const_iterator         = typename tree_type::const_iterator;
    using reverse_iterator       = typename tree_type::reverse_iterator;
    using const_reverse_iterator = typename tree_type::const_reverse_iterator;
    using size_type              = typename tree_type::size_type;
    using difference_type        = typename tree_type::difference_type;

    multiset() = default;
    multiset(multiset&&) = default;
    multiset& operator=(multiset&&) & = default;

    constexpr explicit multiset(value_compare const& comp)
    noexcept(is_nothrow_copy_constructible_v<value_compare>) :
        tree_(comp)
    {}

    void swap(multiset& x)
    noexcept(is_nothrow_swappable_v<tree_type>)
    { tree_.swap(x.tree_); }

    constexpr auto size() const noexcept
    { return tree_.size(); }

    constexpr auto empty() const noexcept
    { return tree_.empty(); }

    constexpr auto value_comp() const
    noexcept(is_nothrow_copy_constructible_v<value_compare>)
    { return tree_.value_comp(); }

    auto begin() noexcept
    { return tree_.begin(); }

    auto end() noexcept
    { return tree_.end(); }

    constexpr auto begin() const noexcept
    { return tree_.begin(); }

    constexpr auto end() const noexcept
    { return tree_.end(); }

    constexpr auto cbegin() const noexcept
    { return tree_.cbegin(); }

    constexpr auto cend() const noexcept
    { return tree_.cend(); }

    auto rbegin() noexcept
    { return tree_.rbegin(); }

    auto rend() noexcept
    { return tree_.rend(); }

    auto rbegin() const noexcept
    { return tree_.rbegin(); }

    auto rend() const noexcept
    { return tree_.rend(); }

    auto crbegin() const noexcept
    { return tree_.crbegin(); }

    auto crend() const noexcept
    { return tree_.crend(); }

    template<typename K>
    auto lower_bound(K const& k) noexcept
    { return tree_.lower_bound(k); }

    template<typename K>
    auto lower_bound(K const& k) const noexcept
    { return tree_.lower_bound(k); }

    template<typename K>
    auto upper_bound(K const& k) noexcept
    { return tree_.upper_bound(k); }

    template<typename K>
    auto upper_bound(K const& k) const noexcept
    { return tree_.upper_bound(k); }

    template<typename K>
    auto equal_range(K const& k) noexcept
    { return tree_.equal_range_multi(k); }

    template<typename K>
    auto equal_range(K const& k) const noexcept
    { return tree_.equal_range_multi(k); }

    template<typename K>
    auto count(K const& k) noexcept
    { return tree_.count_multi(k); }

    template<typename K>
    auto find(K const& k) noexcept
    { return tree_.find(k); }

    template<typename K>
    auto find(K const& k) const noexcept
    { return tree_.find(k); }

    auto erase(const_iterator pos) noexcept
    { return tree_.erase(pos); }

    auto erase(const_iterator first, const_iterator last) noexcept
    { return tree_.erase(first, last); }

    auto clear() noexcept
    { return tree_.clear(); }

    template<typename Dispose>
    auto clear_and_dispose(Dispose dispose) noexcept
    { return tree_.clear_and_dispose(std::move(dispose)); }

    auto insert(value_type& v) noexcept
    { return tree_.insert_multi(v); }

    auto insert(const_iterator pos, value_type& v) noexcept
    { return tree_.insert_multi(pos, v); }

    template<typename InIt>
    auto insert(InIt first, InIt last)
    { return tree_.insert_multi(first, last); }

    static auto iterator_to(pointer const x) noexcept
    { return tree_type::iterator_to(x); }

    static constexpr auto iterator_to(const_pointer const x) noexcept
    { return tree_type::iterator_to(x); }

    static constexpr auto const_iterator_to(const_pointer const x) noexcept
    { return tree_type::const_iterator_to(x); }
};


template<typename T, typename Compare, typename Tag>
inline void swap(set<T, Compare, Tag>& x, set<T, Compare, Tag>& y)
noexcept(noexcept(x.swap(y)))
{
    x.swap(y);
}

template<typename T, typename Compare, typename Tag>
inline void swap(multiset<T, Compare, Tag>& x, multiset<T, Compare, Tag>& y)
noexcept(noexcept(x.swap(y)))
{
    x.swap(y);
}

}}    // namespace amp::intrusive


#endif  // AMP_INCLUDED_62F84B2D_B58A_4D93_8EFC_57976F05F7D6

