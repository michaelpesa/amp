////////////////////////////////////////////////////////////////////////////////
//
// amp/aux/flat_tree.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_8ECBD409_53F8_4C48_A169_2BE2DDEFE822
#define AMP_INCLUDED_8ECBD409_53F8_4C48_A169_2BE2DDEFE822


#include <amp/aux/compressed_pair.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <algorithm>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>


namespace amp {
namespace aux {

template<typename T, typename Compare, typename Allocator>
class flat_tree
{
    using base_type = std::vector<T, Allocator>;

public:
    using allocator_type         = Allocator;
    using value_type             = T;
    using value_compare          = Compare;
    using pointer                = typename base_type::pointer;
    using const_pointer          = typename base_type::const_pointer;
    using size_type              = typename base_type::size_type;
    using difference_type        = typename base_type::difference_type;
    using iterator               = typename base_type::iterator;
    using const_iterator         = typename base_type::const_iterator;
    using reverse_iterator       = typename base_type::reverse_iterator;
    using const_reverse_iterator = typename base_type::const_reverse_iterator;

    flat_tree() = default;
    flat_tree(flat_tree&&) = default;
    flat_tree(flat_tree const&) = default;

    explicit flat_tree(value_compare const& comp,
                       allocator_type const& a = allocator_type())
    noexcept(is_nothrow_default_constructible_v<allocator_type> &&
             is_nothrow_copy_constructible_v<allocator_type> &&
             is_nothrow_copy_constructible_v<value_compare>) :
        base_and_comp(std::piecewise_construct,
                      std::forward_as_tuple(a),
                      std::forward_as_tuple(comp))
    {}

    explicit flat_tree(allocator_type const& a)
    noexcept(is_nothrow_copy_constructible_v<allocator_type> &&
             is_nothrow_copy_constructible_v<value_compare> &&
             is_nothrow_default_constructible_v<value_compare>) :
        flat_tree(value_compare(), a)
    {}

    flat_tree(flat_tree&& x, allocator_type const& a)
    noexcept(is_nothrow_copy_constructible_v<allocator_type> &&
             is_nothrow_move_constructible_v<value_compare>) :
        base_and_comp(std::piecewise_construct,
                      std::forward_as_tuple(std::move(x.base()), a),
                      std::forward_as_tuple(std::move(x.value_comp())))
    {}

    flat_tree(flat_tree const& x, allocator_type const& a) :
        base_and_comp(std::piecewise_construct,
                      std::forward_as_tuple(x.base(), a),
                      std::forward_as_tuple(x.value_comp()))
    {}

    flat_tree& operator=(flat_tree&&) & = default;
    flat_tree& operator=(flat_tree const&) & = default;

    allocator_type get_allocator() const
    { return base().get_allocator(); }

    AMP_INLINE decltype(auto) value_comp() const& noexcept
    { return base_and_comp.second(); }

    AMP_INLINE decltype(auto) value_comp() & noexcept
    { return base_and_comp.second(); }

    AMP_INLINE decltype(auto) value_comp() && noexcept
    { return std::move(base_and_comp).second(); }

    auto begin()        noexcept { return base().begin(); }
    auto end()          noexcept { return base().end(); }
    auto begin()  const noexcept { return base().begin(); }
    auto end()    const noexcept { return base().end(); }
    auto cbegin() const noexcept { return base().cbegin(); }
    auto cend()   const noexcept { return base().cend(); }

    auto rbegin()        noexcept { return base().rbegin(); }
    auto rend()          noexcept { return base().rend(); }
    auto rbegin()  const noexcept { return base().rbegin(); }
    auto rend()    const noexcept { return base().rend(); }
    auto crbegin() const noexcept { return base().crbegin(); }
    auto crend()   const noexcept { return base().crend(); }

    auto empty()    const noexcept { return base().empty(); }
    auto size()     const noexcept { return base().size(); }
    auto max_size() const noexcept { return base().max_size(); }

    void reserve(size_type n)
    { base().reserve(n); }

    void shrink_to_fit()
    { base().shrink_to_fit(); }

    template<
        typename V,
        typename = enable_if_t<is_convertible_v<V, value_type>>
    >
    std::pair<iterator, bool> insert_unique(V&& v)
    {
        return try_insert_unique(lower_bound(v), std::forward<V>(v));
    }

    std::pair<iterator, bool> insert_unique(value_type const& v)
    {
        return insert_unique<value_type const&>(v);
    }

    std::pair<iterator, bool> insert_unique(value_type&& v)
    {
        return insert_unique<value_type&&>(std::move(v));
    }

    template<
        typename V,
        typename = enable_if_t<is_convertible_v<V, value_type>>
    >
    iterator insert_unique(const_iterator hint, V&& v)
    {
        auto commit = [this, &v](const_iterator const pos) {
            return base().insert(pos, std::forward<V>(v));
        };

        if (hint == cend() || value_comp()(v, *hint)) {
            if (hint == cbegin()) {
                return commit(hint);
            }
            if (value_comp()(hint[-1], v)) {
                return commit(hint);
            }
            if (!value_comp()(v, hint[-1])) {
                auto const start = base().begin();
                return start + ((hint - start) - 1);
            }
            hint = std::lower_bound(cbegin(), hint - 1, v, value_comp());
        }
        else {
            hint = std::lower_bound(hint, cend(), v, value_comp());
        }

        return try_insert_unique(hint, std::forward<V>(v)).first;
    }

    iterator insert_unique(const_iterator hint, value_type const& v)
    {
        return insert_unique<value_type const&>(hint, v);
    }

    iterator insert_unique(const_iterator hint, value_type&& v)
    {
        return insert_unique<value_type&&>(hint, std::move(v));
    }

    template<typename InIt>
    void insert_unique(InIt first, InIt last)
    {
        if (!start_insert_range(first, last)) return;
        for (; first != last; ++first) insert_unique(*first);
    }

    template<typename InIt>
    void assign_unique(InIt first, InIt last)
    {
        if (!start_insert_range(first, last)) return;
        for (; first != last; ++first) insert_unique(cend(), *first);
    }

    template<typename V>
    iterator insert_multi(V&& v)
    {
        return base().insert(upper_bound(v), std::forward<V>(v));
    }

    iterator insert_multi(value_type const& v)
    {
        return insert_multi<value_type const&>(v);
    }

    iterator insert_multi(value_type&& v)
    {
        return insert_multi<value_type&&>(std::move(v));
    }

    template<typename V>
    iterator insert_multi(const_iterator hint, V&& v)
    {
        if (hint == cend() || !value_comp()(*hint, v)) {
            if (hint != cbegin() && value_comp()(v, hint[-1])) {
                hint = std::upper_bound(cbegin(), hint, v, value_comp());
            }
        }
        else {
            hint = std::lower_bound(hint, cend(), v, value_comp());
        }

        return base().insert(hint, std::forward<V>(v));
    }

    iterator insert_multi(const_iterator hint, value_type const& v)
    {
        return insert_multi<value_type const&>(hint, v);
    }

    iterator insert_multi(const_iterator hint, value_type&& v)
    {
        return insert_multi<value_type&&>(hint, std::move(v));
    }

    template<typename InIt>
    void insert_multi(InIt first, InIt last)
    {
        if (!start_insert_range(first, last)) return;
        for (; first != last; ++first) insert_multi(*first);
    }

    template<typename InIt>
    void assign_multi(InIt first, InIt last)
    {
        if (!start_insert_range(first, last)) return;
        for (; first != last; ++first) insert_multi(cend(), *first);
    }

    template<typename... Args>
    std::pair<iterator, bool> emplace_unique(Args&&... args)
    {
        return insert_unique(value_type(std::forward<Args>(args)...));
    }

    template<typename... Args>
    iterator emplace_hint_unique(const_iterator hint, Args&&... args)
    {
        return insert_unique(hint, value_type(std::forward<Args>(args)...));
    }

    template<typename... Args>
    iterator emplace_multi(Args&&... args)
    {
        return insert_multi(value_type(std::forward<Args>(args)...));
    }

    template<typename... Args>
    iterator emplace_hint_multi(const_iterator hint, Args&&... args)
    {
        return insert_multi(hint, value_type(std::forward<Args>(args)...));
    }

    void clear() noexcept
    {
        base().clear();
    }

    void swap(flat_tree& x)
    noexcept(is_nothrow_swappable_v<value_compare> &&
             is_nothrow_swappable_v<allocator_type>)
    {
        base_and_comp.swap(x.base_and_comp);
    }

    template<
        typename K,
        typename = enable_if_t<
            !is_same_v<K, iterator> &&
            !is_same_v<K, const_iterator>
        >
    >
    size_type erase(K const& k)
    {
        auto const eq = equal_range(k);
        if (eq.first != eq.second) {
            erase(eq.first, eq.second);
        }
        return static_cast<size_type>(eq.second - eq.first);
    }

    iterator erase(const_iterator const pos)
    {
        AMP_ASSERT(pos >= cbegin());
        AMP_ASSERT(pos <  cend());

        return base().erase(pos);
    }

    iterator erase(const_iterator const first, const_iterator const last)
    {
        AMP_ASSERT(first >= cbegin());
        AMP_ASSERT(last  <= cend());
        AMP_ASSERT(last  >= first);

        return base().erase(first, last);
    }

    template<typename K>
    size_type count(K const& k) const
    {
        auto const eq = equal_range(k);
        return static_cast<size_type>(std::distance(eq.first, eq.second));
    }

    template<typename K>
    iterator find(K const& k)
    {
        auto const it = lower_bound(k);
        return it != end() && !value_comp()(k, *it) ? it : end();
    }

    template<typename K>
    const_iterator find(K const& k) const
    {
        auto const it = lower_bound(k);
        return it != end() && !value_comp()(k, *it) ? it : end();
    }

    template<typename K>
    auto lower_bound(K const& k)
    { return std::lower_bound(begin(), end(), k, value_comp()); }

    template<typename K>
    auto lower_bound(K const& k) const
    { return std::lower_bound(begin(), end(), k, value_comp()); }

    template<typename K>
    auto upper_bound(K const& k)
    { return std::upper_bound(begin(), end(), k, value_comp()); }

    template<typename K>
    auto upper_bound(K const& k) const
    { return std::upper_bound(begin(), end(), k, value_comp()); }

    template<typename K>
    auto equal_range(K const& k)
    { return std::equal_range(begin(), end(), k, value_comp()); }

    template<typename K>
    auto equal_range(K const& k) const
    { return std::equal_range(begin(), end(), k, value_comp()); }

private:
    template<typename It>
    bool start_insert_range(It first, It last)
    {
        using Category = typename std::iterator_traits<It>::iterator_category;
        return start_insert_range(first, last, Category());
    }

    template<typename FwdIt>
    bool start_insert_range(FwdIt first, FwdIt last, std::forward_iterator_tag)
    {
        auto const extra = std::distance(first, last);
        if (extra == 0) return false;

        reserve(size() + static_cast<size_type>(extra));
        return true;
    }

    template<typename InIt>
    bool start_insert_range(InIt first, InIt last, std::input_iterator_tag)
    {
        return first != last;
    }

    template<typename V>
    auto try_insert_unique(const_iterator pos, V&& v)
    {
        return pos == cend() || value_comp()(v, *pos)
             ? std::make_pair(base().insert(pos, std::forward<V>(v)), true)
             : std::make_pair(begin() + std::distance(cbegin(), pos), false);
    }

    AMP_INLINE base_type const& base() const& noexcept
    { return base_and_comp.first(); }

    AMP_INLINE base_type& base() & noexcept
    { return base_and_comp.first(); }

    AMP_INLINE base_type&& base() && noexcept
    { return std::move(base_and_comp).first(); }

    compressed_pair<base_type, value_compare> base_and_comp;
};

}}    // namespace amp::aux


#endif  // AMP_INCLUDED_8ECBD409_53F8_4C48_A169_2BE2DDEFE822

