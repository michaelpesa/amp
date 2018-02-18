////////////////////////////////////////////////////////////////////////////////
//
// amp/flat_set.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_74F32D47_9D6B_417F_8324_A0E56720135C
#define AMP_INCLUDED_74F32D47_9D6B_417F_8324_A0E56720135C


#include <amp/aux/flat_tree.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>


namespace amp {

template<
    typename Key,
    typename Compare = std::less<>,
    typename Allocator = std::allocator<Key>
>
class flat_set
{
    using Tree = aux::flat_tree<Key, Compare, Allocator>;
    Tree tree;

public:
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

    flat_set() = default;
    flat_set(flat_set&&) = default;
    flat_set(flat_set const&) = default;

    explicit flat_set(key_compare const& comp,
                      allocator_type const& a = allocator_type())
    noexcept(is_nothrow_default_constructible_v<allocator_type> &&
             is_nothrow_copy_constructible_v<allocator_type> &&
             is_nothrow_copy_constructible_v<key_compare>) :
        tree(comp, a)
    {}

    explicit flat_set(allocator_type const& a)
    noexcept(is_nothrow_copy_constructible_v<allocator_type> &&
             is_nothrow_copy_constructible_v<key_compare> &&
             is_nothrow_default_constructible_v<key_compare>) :
        tree(value_compare(), a)
    {}

    template<typename InIt>
    flat_set(InIt first, InIt last,
             key_compare const& comp = key_compare(),
             allocator_type const& a = allocator_type()) :
        tree(comp, a)
    {
        tree.assign_unique(first, last);
    }

    template<typename InIt>
    flat_set(InIt first, InIt last, allocator_type const& a) :
        flat_set(first, last, key_compare(), a)
    {}

    flat_set(std::initializer_list<value_type> init,
             key_compare const& comp = key_compare(),
             allocator_type const& a = allocator_type()) :
        flat_set(init.begin(), init.end(), comp, a)
    {}

    flat_set(std::initializer_list<value_type> init,
             allocator_type const& a) :
        flat_set(init.begin(), init.end(), key_compare(), a)
    {}

    flat_set(flat_set&& x, allocator_type const& a)
    noexcept(is_nothrow_copy_constructible_v<allocator_type> &&
             is_nothrow_move_constructible_v<key_compare>) :
        tree(std::move(x.tree), a)
    {}

    flat_set(flat_set const& x, allocator_type const& a) :
        tree(x.tree, a)
    {}

    flat_set& operator=(flat_set&&) & = default;
    flat_set& operator=(flat_set const&) & = default;

    flat_set& operator=(std::initializer_list<value_type> init) &
    {
        tree.clear();
        tree.assign_unique(init.begin(), init.end());
        return *this;
    }

    allocator_type get_allocator() const
    { return tree.get_allocator(); }

    key_compare key_comp() const
    { return tree.value_comp(); }

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

    void reserve(size_type n) { tree.reserve(n); }
    void shrink_to_fit()      { tree.shrink_to_fit(); }

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
    { return tree.insert_unique(hint, std::forward<V>(v)); }

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

    void clear() noexcept
    { tree.clear(); }

    size_type erase(key_type const& k)
    { return tree.erase(k); }

    iterator erase(const_iterator pos)
    { return tree.erase(pos); }

    iterator erase(const_iterator first, const_iterator last)
    { return tree.erase(first, last); }

    void swap(flat_set& x)
    noexcept(is_nothrow_swappable_v<Tree>)
    { tree.swap(x.tree); }

    template<typename K>
    auto count(K const& k) const
    { return tree.count(k); }

    template<typename K>
    auto find(K const& k)
    { return tree.find(k); }

    template<typename K>
    auto find(K const& k) const
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

    auto count(key_type const& k) const
    { return tree.count(k); }

    auto find(key_type const& k)
    { return tree.find(k); }

    auto find(key_type const& k) const
    { return tree.find(k); }

    auto lower_bound(key_type const& k)
    { return tree.lower_bound(k); }

    auto lower_bound(key_type const& k) const
    { return tree.lower_bound(k); }

    auto upper_bound(key_type const& k)
    { return tree.upper_bound(k); }

    auto upper_bound(key_type const& k) const
    { return tree.upper_bound(k); }

    auto equal_range(key_type const& k)
    { return tree.equal_range(k); }

    auto equal_range(key_type const& k) const
    { return tree.equal_range(k); }

private:
    friend void swap(flat_set& x, flat_set& y)
    noexcept(noexcept(x.swap(y)))
    {
        x.swap(y);
    }
};


template<
    typename Key,
    typename Compare = std::less<>,
    typename Allocator = std::allocator<Key>
>
class flat_multiset
{
    using Tree = aux::flat_tree<Key, Compare, Allocator>;
    Tree tree;

public:
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

    flat_multiset() = default;
    flat_multiset(flat_multiset&&) = default;
    flat_multiset(flat_multiset const&) = default;

    explicit flat_multiset(key_compare const& comp,
                           allocator_type const& a = allocator_type())
    noexcept(is_nothrow_default_constructible_v<allocator_type> &&
             is_nothrow_copy_constructible_v<allocator_type> &&
             is_nothrow_copy_constructible_v<key_compare>) :
        tree(comp, a)
    {}

    explicit flat_multiset(allocator_type const& a)
    noexcept(is_nothrow_copy_constructible_v<allocator_type> &&
             is_nothrow_copy_constructible_v<key_compare> &&
             is_nothrow_default_constructible_v<key_compare>) :
        tree(key_compare(), a)
    {}

    template<typename InIt>
    flat_multiset(InIt first, InIt last,
                  key_compare const& comp = key_compare(),
                  allocator_type const& a = allocator_type()) :
        tree(comp, a)
    {
        tree.assign_multi(first, last);
    }

    template<typename InIt>
    flat_multiset(InIt first, InIt last, allocator_type const& a) :
        flat_multiset(first, last, key_compare(), a)
    {}

    flat_multiset(std::initializer_list<value_type> init,
                  key_compare const& comp = key_compare(),
                  allocator_type const& a = allocator_type()) :
        flat_multiset(init.begin(), init.end(), comp, a)
    {}

    flat_multiset(std::initializer_list<value_type> init,
                  allocator_type const& a) :
        flat_multiset(init.begin(), init.end(), key_compare(), a)
    {}

    flat_multiset(flat_multiset&& x, allocator_type const& a)
    noexcept(is_nothrow_copy_constructible_v<allocator_type> &&
             is_nothrow_move_constructible_v<key_compare>) :
        tree(std::move(x.tree), a)
    {}

    flat_multiset(flat_multiset const& x, allocator_type const& a) :
        tree(x.tree, a)
    {}

    flat_multiset& operator=(flat_multiset&&) & = default;
    flat_multiset& operator=(flat_multiset const&) & = default;

    flat_multiset& operator=(std::initializer_list<value_type> init) &
    {
        tree.clear();
        tree.assign_multi(init.begin(), init.end());
        return *this;
    }

    allocator_type get_allocator() const
    { return tree.get_allocator(); }

    key_compare key_comp() const
    { return tree.value_comp(); }

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
    { return tree.insert_multi(hint, std::forward<V>(v)); }

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

    void swap(flat_multiset& x)
    noexcept(is_nothrow_swappable_v<Tree>)
    { tree.swap(x.tree); }

    template<typename K>
    auto count(K const& k) const
    { return tree.count(k); }

    template<typename K>
    auto find(K const& k)
    { return tree.find(k); }

    template<typename K>
    auto find(K const& k) const
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

    auto count(key_type const& k) const
    { return tree.count(k); }

    auto find(key_type const& k)
    { return tree.find(k); }

    auto find(key_type const& k) const
    { return tree.find(k); }

    auto lower_bound(key_type const& k)
    { return tree.lower_bound(k); }

    auto lower_bound(key_type const& k) const
    { return tree.lower_bound(k); }

    auto upper_bound(key_type const& k)
    { return tree.upper_bound(k); }

    auto upper_bound(key_type const& k) const
    { return tree.upper_bound(k); }

    auto equal_range(key_type const& k)
    { return tree.equal_range(k); }

    auto equal_range(key_type const& k) const
    { return tree.equal_range(k); }

private:
    friend void swap(flat_multiset& x, flat_multiset& y)
    noexcept(noexcept(x.swap(y)))
    {
        x.swap(y);
    }
};

}     // namespace amp


#endif  // AMP_INCLUDED_74F32D47_9D6B_417F_8324_A0E56720135C

