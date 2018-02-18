////////////////////////////////////////////////////////////////////////////////
//
// amp/flat_map.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_08420FF9_FB3E_45CF_A73D_052CEF9E17D3
#define AMP_INCLUDED_08420FF9_FB3E_45CF_A73D_052CEF9E17D3


#include <amp/aux/flat_tree.hpp>
#include <amp/aux/map_value_compare.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>


namespace amp {

template<
    typename Key,
    typename T,
    typename Compare = std::less<>,
    typename Allocator = std::allocator<std::pair<Key, T>>
>
class flat_map
{
    using Tree = aux::flat_tree<
        std::pair<Key, T>,
        map_value_compare<Key, T, Compare>,
        Allocator
    >;
    Tree tree;

public:
    using mapped_type            = T;
    using key_type               = Key;
    using key_compare            = Compare;
    using value_type             = typename Tree::value_type;
    using value_compare          = typename Tree::value_compare;
    using allocator_type         = typename Tree::allocator_type;
    using pointer                = typename Tree::pointer;
    using const_pointer          = typename Tree::const_pointer;
    using size_type              = typename Tree::size_type;
    using difference_type        = typename Tree::difference_type;
    using iterator               = typename Tree::iterator;
    using const_iterator         = typename Tree::const_iterator;
    using reverse_iterator       = typename Tree::reverse_iterator;
    using const_reverse_iterator = typename Tree::const_reverse_iterator;

    flat_map() = default;
    flat_map(flat_map&&) = default;
    flat_map(flat_map const&) = default;

    explicit flat_map(key_compare const& comp,
                      allocator_type const& a = allocator_type())
    noexcept(is_nothrow_default_constructible_v<allocator_type> &&
             is_nothrow_copy_constructible_v<allocator_type> &&
             is_nothrow_copy_constructible_v<key_compare>) :
        tree(value_compare(comp), a)
    {}

    explicit flat_map(allocator_type const& a)
    noexcept(is_nothrow_copy_constructible_v<allocator_type> &&
             is_nothrow_copy_constructible_v<key_compare> &&
             is_nothrow_default_constructible_v<key_compare>) :
        tree(value_compare(), a)
    {}

    template<typename InIt>
    flat_map(InIt first, InIt last,
             key_compare const& comp = key_compare(),
             allocator_type const& a = allocator_type()) :
        tree(value_compare(comp), a)
    {
        tree.assign_unique(first, last);
    }

    template<typename InIt>
    flat_map(InIt first, InIt last, allocator_type const& a) :
        flat_map(first, last, key_compare(), a)
    {}

    flat_map(std::initializer_list<value_type> init,
             key_compare const& comp = key_compare(),
             allocator_type const& a = allocator_type()) :
        flat_map(init.begin(), init.end(), comp, a)
    {}

    flat_map(std::initializer_list<value_type> init,
             allocator_type const& a) :
        flat_map(init.begin(), init.end(), key_compare(), a)
    {}

    flat_map(flat_map&& x, allocator_type const& a)
    noexcept(is_nothrow_copy_constructible_v<allocator_type> &&
             is_nothrow_move_constructible_v<key_compare>) :
        tree(std::move(x.tree), a)
    {}

    flat_map(flat_map const& x, allocator_type const& a) :
        tree(x.tree, a)
    {}

    flat_map& operator=(flat_map&&) & = default;
    flat_map& operator=(flat_map const&) & = default;

    flat_map& operator=(std::initializer_list<value_type> init) &
    {
        tree.clear();
        tree.assign_unique(init.begin(), init.end());
        return *this;
    }

    allocator_type get_allocator() const
    { return tree.get_allocator(); }

    key_compare key_comp() const
    { return tree.value_comp().key_comp(); }

    value_compare value_comp() const
    { return tree.value_comp(); }

    auto begin()        noexcept { return tree.begin(); }
    auto end()          noexcept { return tree.end(); }
    auto begin()  const noexcept { return tree.begin(); }
    auto end()    const noexcept { return tree.end(); }
    auto cbegin() const noexcept { return tree.cbegin(); }
    auto cend()   const noexcept { return tree.cend(); }

    auto rbegin()        noexcept { return tree.rbegin(); }
    auto rend()          noexcept { return tree.rend(); }
    auto rbegin()  const noexcept { return tree.rbegin(); }
    auto rend()    const noexcept { return tree.rend(); }
    auto crbegin() const noexcept { return tree.crbegin(); }
    auto crend()   const noexcept { return tree.crend(); }

    auto empty()    const noexcept { return tree.empty(); }
    auto size()     const noexcept { return tree.size(); }
    auto max_size() const noexcept { return tree.max_size(); }

    auto reserve(size_type n) { tree.reserve(n); }
    auto shrink_to_fit()      { tree.shrink_to_fit(); }

    template<
        typename V,
        typename = enable_if_t<is_convertible_v<V, value_type>>
    >
    std::pair<iterator, bool> insert(V&& v)
    { return tree.insert_unique(std::forward<V>(v)); }

    std::pair<iterator, bool> insert(value_type&& v)
    { return tree.insert_unique(std::move(v)); }

    std::pair<iterator, bool> insert(value_type const& v)
    { return tree.insert_unique(v); }

    template<typename InIt>
    void insert(InIt first, InIt last)
    { return tree.insert_unique(first, last); }

    void insert(std::initializer_list<value_type> init)
    { insert(init.begin(), init.end()); }

    template<
        typename V,
        typename = enable_if_t<is_convertible_v<V, value_type>>
    >
    iterator insert(const_iterator hint, V&& v)
    { return tree.insert_unique(hint, v); }

    iterator insert(const_iterator hint, value_type&& v)
    { return tree.insert_unique(hint, std::move(v)); }

    iterator insert(const_iterator hint, value_type const& v)
    { return tree.insert_unique(hint, v); }

    template<
        typename... Args,
        typename = enable_if_t<is_constructible_v<value_type, Args...>>
    >
    std::pair<iterator, bool> emplace(Args&&... args)
    { return insert(value_type(std::forward<Args>(args)...)); }

    template<
        typename... Args,
        typename = enable_if_t<is_constructible_v<value_type, Args...>>
    >
    iterator emplace_hint(const_iterator hint, Args&&... args)
    { return insert(hint, value_type(std::forward<Args>(args)...)); }

    template<typename K, typename... M>
    auto try_emplace(K&& k, M&&... m)
    { return try_emplace_aux(std::forward<K>(k), std::forward<M>(m)...); }

    template<typename... M>
    auto try_emplace(key_type&& k, M&&... m)
    { return try_emplace_aux(std::move(k), std::forward<M>(m)...); }

    template<typename... M>
    auto try_emplace(key_type const& k, M&&... m)
    { return try_emplace_aux(k, std::forward<M>(m)...); }

    template<typename K, typename M>
    auto insert_or_assign(K&& k, M&& m)
    { return insert_or_assign_aux(std::forward<K>(k), std::forward<M>(m)); }

    template<typename M>
    auto insert_or_assign(key_type&& k, M&& m)
    { return insert_or_assign_aux(std::move(k), std::forward<M>(m)); }

    template<typename M>
    auto insert_or_assign(key_type const& k, M&& m)
    { return insert_or_assign_aux(k, std::forward<M>(m)); }

    void clear() noexcept
    { tree.clear(); }

    size_type erase(key_type const& k)
    { return tree.erase(k); }

    iterator erase(const_iterator pos)
    { return tree.erase(pos); }

    iterator erase(const_iterator first, const_iterator last)
    { return tree.erase(first, last); }

    void swap(flat_map& x)
    noexcept(is_nothrow_swappable_v<Tree>)
    { tree.swap(x.tree); }

    mapped_type& at(key_type const& k)
    {
        auto found = find(k);
        if (found != end()) {
            return std::get<1>(*found);
        }
        throw std::out_of_range{"flat_map::at"};
    }

    mapped_type const& at(key_type const& k) const
    {
        auto found = find(k);
        if (found != end()) {
            return std::get<1>(*found);
        }
        throw std::out_of_range{"flat_map::at"};
    }

    mapped_type& operator[](key_type&& k)
    {
        static_assert(is_default_constructible_v<mapped_type>,
                      "flat_map::operator[]: concept check failed: "
                      "flat_map::mapped_type must be DefaultConstructible");

        auto found = lower_bound(k);
        if (found == cend() || value_comp()(k, *found)) {
            found = emplace_hint(found,
                                 std::piecewise_construct,
                                 std::forward_as_tuple(std::move(k)),
                                 std::forward_as_tuple());
        }
        return std::get<1>(*found);
    }

    mapped_type& operator[](key_type const& k)
    {
        static_assert(is_default_constructible_v<mapped_type>,
                      "flat_map::operator[]: concept check failed: "
                      "flat_map::mapped_type must be DefaultConstructible");

        auto found = lower_bound(k);
        if (found == cend() || value_comp()(k, *found)) {
            found = emplace_hint(found,
                                 std::piecewise_construct,
                                 std::forward_as_tuple(k),
                                 std::forward_as_tuple());
        }
        return std::get<1>(*found);
    }

    template<typename K>
    size_type count(K const& k) const
    { return tree.count(k); }

    template<typename K>
    iterator find(K const& k)
    { return tree.find(k); }

    template<typename K>
    const_iterator find(K const& k) const
    { return tree.find(k); }

    template<typename K>
    auto lower_bound(K const& k)
    { return tree.lower_bound(k); }

    template<typename K>
    auto lower_bound(K const& k) const
    { return tree.lower_bound(k); }

    template<typename K>
    auto upper_bound(K const& k)
    { return tree.upper_bound(k); }

    template<typename K>
    auto upper_bound(K const& k) const
    { return tree.upper_bound(k); }

    template<typename K>
    auto equal_range(K const& k)
    { return tree.equal_range(k); }

    template<typename K>
    auto equal_range(K const& k) const
    { return tree.equal_range(k); }

private:
    template<typename K, typename... M>
    std::pair<iterator, bool> try_emplace_aux(K&& k, M&&... m)
    {
        auto found = lower_bound(k);
        if (found == cend() || value_comp()(k, *found)) {
            return std::make_pair(
                emplace_hint(found,
                             std::piecewise_construct,
                             std::forward_as_tuple(std::forward<K>(k)),
                             std::forward_as_tuple(std::forward<M>(m)...)),
                true);
        }
        return std::make_pair(found, false);
    }

    template<typename K, typename M>
    std::pair<iterator, bool> insert_or_assign_aux(K&& k, M&& m)
    {
        auto found = lower_bound(k);
        if (found == cend() || value_comp()(k, *found)) {
            return std::make_pair(
                emplace_hint(found,
                             std::piecewise_construct,
                             std::forward_as_tuple(std::forward<K>(k)),
                             std::forward_as_tuple(std::forward<M>(m))),
                true);
        }
        std::get<1>(*found) = std::forward<M>(m);
        return std::make_pair(found, false);
    }

    friend void swap(flat_map& x, flat_map& y)
    noexcept(noexcept(x.swap(y)))
    {
        x.swap(y);
    }
};


template<
    typename Key,
    typename T,
    typename Compare = std::less<>,
    typename Allocator = std::allocator<std::pair<Key, T>>
>
class flat_multimap
{
    using Tree = aux::flat_tree<
        std::pair<Key, T>,
        map_value_compare<Key, T, Compare>,
        Allocator
    >;
    Tree tree;

public:
    using mapped_type            = T;
    using key_type               = Key;
    using key_compare            = Compare;
    using value_type             = typename Tree::value_type;
    using value_compare          = typename Tree::value_compare;
    using allocator_type         = typename Tree::allocator_type;
    using pointer                = typename Tree::pointer;
    using const_pointer          = typename Tree::const_pointer;
    using size_type              = typename Tree::size_type;
    using difference_type        = typename Tree::difference_type;
    using iterator               = typename Tree::iterator;
    using const_iterator         = typename Tree::const_iterator;
    using reverse_iterator       = typename Tree::reverse_iterator;
    using const_reverse_iterator = typename Tree::const_reverse_iterator;

    flat_multimap() = default;
    flat_multimap(flat_multimap&&) = default;
    flat_multimap(flat_multimap const&) = default;

    explicit flat_multimap(key_compare const& comp,
                           allocator_type const& a = allocator_type())
    noexcept(is_nothrow_default_constructible_v<allocator_type> &&
             is_nothrow_copy_constructible_v<allocator_type> &&
             is_nothrow_copy_constructible_v<key_compare>) :
        tree(value_compare(comp), a)
    {}

    explicit flat_multimap(allocator_type const& a)
    noexcept(is_nothrow_copy_constructible_v<allocator_type> &&
             is_nothrow_copy_constructible_v<key_compare> &&
             is_nothrow_default_constructible_v<key_compare>) :
        tree(value_compare(), a)
    {}

    template<typename InIt>
    flat_multimap(InIt first, InIt last,
                  key_compare const& comp = key_compare(),
                  allocator_type const& a = allocator_type()) :
        tree(value_compare(comp), a)
    {
        tree.assign_multi(first, last);
    }

    template<typename InIt>
    flat_multimap(InIt first, InIt last, allocator_type const& a) :
        flat_multimap(first, last, key_compare(), a)
    {}

    flat_multimap(std::initializer_list<value_type> init,
                  key_compare const& comp = key_compare(),
                  allocator_type const& a = allocator_type()) :
        flat_multimap(init.begin(), init.end(), comp, a)
    {}

    flat_multimap(std::initializer_list<value_type> init,
                  allocator_type const& a) :
        flat_multimap(init.begin(), init.end(), key_compare(), a)
    {}

    flat_multimap(flat_multimap&& x, allocator_type const& a)
    noexcept(is_nothrow_copy_constructible_v<allocator_type> &&
             is_nothrow_move_constructible_v<key_compare>) :
        tree(std::move(x.tree), a)
    {}

    flat_multimap(flat_multimap const& x, allocator_type const& a) :
        tree(x.tree, a)
    {}

    flat_multimap& operator=(flat_multimap&&) & = default;
    flat_multimap& operator=(flat_multimap const&) & = default;

    flat_multimap& operator=(std::initializer_list<value_type> init) &
    {
        tree.clear();
        tree.assign_multi(init.begin(), init.end());
        return *this;
    }

    allocator_type get_allocator() const
    { return tree.get_allocator(); }

    key_compare key_comp() const
    { return tree.value_comp().key_comp(); }

    value_compare value_comp() const
    { return tree.value_comp(); }

    auto begin()        noexcept { return tree.begin(); }
    auto end()          noexcept { return tree.end(); }
    auto begin()  const noexcept { return tree.begin(); }
    auto end()    const noexcept { return tree.end(); }
    auto cbegin() const noexcept { return tree.cbegin(); }
    auto cend()   const noexcept { return tree.cend(); }

    auto rbegin()        noexcept { return tree.rbegin(); }
    auto rend()          noexcept { return tree.rend(); }
    auto rbegin()  const noexcept { return tree.rbegin(); }
    auto rend()    const noexcept { return tree.rend(); }
    auto crbegin() const noexcept { return tree.crbegin(); }
    auto crend()   const noexcept { return tree.crend(); }

    auto empty()    const noexcept { return tree.empty(); }
    auto size()     const noexcept { return tree.size(); }
    auto max_size() const noexcept { return tree.max_size(); }

    auto reserve(size_type n) { tree.reserve(n); }
    auto shrink_to_fit()      { tree.shrink_to_fit(); }

    template<
        typename V,
        typename = enable_if_t<is_convertible_v<V, value_type>>
    >
    iterator insert(V&& v)
    { return tree.insert_multi(std::forward<V>(v)); }

    iterator insert(value_type&& v)
    { return tree.insert_multi(std::move(v)); }

    iterator insert(value_type const& v)
    { return tree.insert_multi(v); }

    template<typename InIt>
    void insert(InIt first, InIt last)
    { return tree.insert_multi(first, last); }

    void insert(std::initializer_list<value_type> init)
    { insert(init.begin(), init.end()); }

    template<
        typename V,
        typename = enable_if_t<is_convertible_v<V, value_type>>
    >
    iterator insert(const_iterator hint, V&& v)
    { return tree.insert_multi(hint, v); }

    iterator insert(const_iterator hint, value_type&& v)
    { return tree.insert_multi(hint, std::move(v)); }

    iterator insert(const_iterator hint, value_type const& v)
    { return tree.insert_multi(hint, v); }

    template<
        typename... Args,
        typename = enable_if_t<is_constructible_v<value_type, Args...>>
    >
    iterator emplace(Args&&... args)
    { return insert(value_type(std::forward<Args>(args)...)); }

    template<
        typename... Args,
        typename = enable_if_t<is_constructible_v<value_type, Args...>>
    >
    iterator emplace_hint(const_iterator hint, Args&&... args)
    { return insert(hint, value_type(std::forward<Args>(args)...)); }

    void clear() noexcept
    { tree.clear(); }

    size_type erase(key_type const& k)
    { return tree.erase(k); }

    iterator erase(const_iterator pos)
    { return tree.erase(pos); }

    iterator erase(const_iterator first, const_iterator last)
    { return tree.erase(first, last); }

    void swap(flat_multimap& x)
    noexcept(is_nothrow_swappable_v<Tree>)
    { tree.swap(x.tree); }

    template<typename K>
    size_type count(K const& k) const
    { return tree.count(k); }

    template<typename K>
    iterator find(K const& k)
    { return tree.find(k); }

    template<typename K>
    const_iterator find(K const& k) const
    { return tree.find(k); }

    template<typename K>
    auto lower_bound(K const& k)
    { return tree.lower_bound(k); }

    template<typename K>
    auto lower_bound(K const& k) const
    { return tree.lower_bound(k); }

    template<typename K>
    auto upper_bound(K const& k)
    { return tree.upper_bound(k); }

    template<typename K>
    auto upper_bound(K const& k) const
    { return tree.upper_bound(k); }

    template<typename K>
    auto equal_range(K const& k)
    { return tree.equal_range(k); }

    template<typename K>
    auto equal_range(K const& k) const
    { return tree.equal_range(k); }

private:
    friend void swap(flat_multimap& x, flat_multimap& y)
    noexcept(noexcept(x.swap(y)))
    {
        x.swap(y);
    }
};

}     // namespace amp


#endif  // AMP_INCLUDED_08420FF9_FB3E_45CF_A73D_052CEF9E17D3

