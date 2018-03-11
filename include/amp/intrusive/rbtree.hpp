////////////////////////////////////////////////////////////////////////////////
//
// amp/intrusive/rbtree.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_43E91639_60AA_42B2_8C9F_EE2536BC51BC
#define AMP_INCLUDED_43E91639_60AA_42B2_8C9F_EE2536BC51BC


#include <amp/aux/compressed_pair.hpp>
#include <amp/aux/operators.hpp>
#include <amp/functional.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <utility>


namespace amp {
namespace intrusive {

enum class rbtree_color : uint8 {
    red   = 0,
    black = 1,
};

struct rbtree_node
{
    rbtree_node* left;
    rbtree_node* right;
    uintptr parent_and_color;
};

// Ensure necessary alignment for embedding the node's color in the lowest bit
// of its parent node pointer.
static_assert(alignof(rbtree_node) > 1, "");


AMP_INLINE auto parent(rbtree_node const* const x) noexcept
{ return reinterpret_cast<rbtree_node*>(x->parent_and_color & ~uintptr{1}); }

AMP_INLINE auto color(rbtree_node const* const x) noexcept
{ return static_cast<rbtree_color>(x->parent_and_color & uintptr{1}); }

AMP_INLINE void set_parent(rbtree_node* const x, rbtree_node* const p) noexcept
{
    AMP_ASSERT((reinterpret_cast<uintptr>(p) & 1) == 0);
    AMP_ASSUME((reinterpret_cast<uintptr>(p) & 1) == 0);

    x->parent_and_color &= uintptr{1};
    x->parent_and_color |= reinterpret_cast<uintptr>(p);
}

AMP_INLINE void set_color(rbtree_node* const x, rbtree_color const c) noexcept
{
    x->parent_and_color &= ~uintptr{1};
    x->parent_and_color |= static_cast<uintptr>(c);
}

AMP_INLINE constexpr rbtree_node* minimum(rbtree_node* const x) noexcept
{ return x->left ? minimum(x->left) : x; }

AMP_INLINE constexpr rbtree_node* maximum(rbtree_node* const x) noexcept
{ return x->right ? maximum(x->right) : x; }


namespace aux {

AMP_EXPORT AMP_READONLY
rbtree_node* iterator_next(rbtree_node*) noexcept;

AMP_EXPORT AMP_READONLY
rbtree_node* iterator_prev(rbtree_node*) noexcept;

AMP_EXPORT
void insert_and_rebalance(rbtree_node*, rbtree_node*,
                          rbtree_node&, bool) noexcept;
AMP_EXPORT
void erase_and_rebalance(rbtree_node*, rbtree_node&) noexcept;

}     // namespace aux


template<typename Tag = void>
struct rbtree_link : rbtree_node {};


template<typename, typename = std::less<>, typename = void>
class rbtree;

template<typename, typename>
class rbtree_const_iterator;


template<typename T, typename Tag>
class rbtree_iterator :
    private bidirection_iteratable<rbtree_iterator<T, Tag>>
{
public:
    using value_type        = T;
    using reference         = T&;
    using pointer           = T*;
    using difference_type   = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;

    constexpr rbtree_iterator() noexcept :
        node_{nullptr}
    {}

    constexpr reference operator*() const noexcept
    {
        return static_cast<reference>(
            static_cast<rbtree_link<Tag>&>(*node_));
    }

    constexpr pointer operator->() const noexcept
    { return std::addressof(**this); }

    rbtree_iterator& operator++() noexcept
    { return (node_ = aux::iterator_next(node_), *this); }

    rbtree_iterator& operator--() noexcept
    { return (node_ = aux::iterator_prev(node_), *this); }

private:
    template<typename, typename, typename>
    friend class rbtree;
    template<typename, typename>
    friend class rbtree_const_iterator;

    constexpr friend bool operator==(rbtree_iterator const& x,
                                     rbtree_iterator const& y) noexcept
    { return (x.node_ == y.node_); }

    constexpr explicit rbtree_iterator(rbtree_node* const x) noexcept :
        node_{x}
    {}

    rbtree_node* node_;
};


template<typename T, typename Tag>
class rbtree_const_iterator :
    private bidirection_iteratable<rbtree_const_iterator<T, Tag>>
{
public:
    using value_type        = T;
    using reference         = T const&;
    using pointer           = T const*;
    using difference_type   = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;

    constexpr rbtree_const_iterator() noexcept :
        node_{nullptr}
    {}

    constexpr rbtree_const_iterator(rbtree_iterator<T, Tag> x) noexcept :
        node_{x.node_}
    {}

    constexpr reference operator*() const noexcept
    {
        return static_cast<reference>(
            static_cast<rbtree_link<Tag> const&>(*node_));
    }

    constexpr pointer operator->() const noexcept
    { return std::addressof(**this); }

    rbtree_const_iterator& operator++() noexcept
    { return (node_ = aux::iterator_next(node_), *this); }

    rbtree_const_iterator& operator--() noexcept
    { return (node_ = aux::iterator_prev(node_), *this); }

private:
    template<typename, typename, typename>
    friend class rbtree;

    constexpr friend bool operator==(rbtree_const_iterator const& x,
                                     rbtree_const_iterator const& y) noexcept
    { return (x.node_ == y.node_); }

    constexpr explicit rbtree_const_iterator(rbtree_node* const x) noexcept :
        node_{x}
    {}

    constexpr auto mut() const noexcept
    { return rbtree_iterator<T, Tag>(node_); }

    rbtree_node* node_;
};


template<typename T, typename Compare, typename Tag>
class rbtree
{
private:
    AMP_INLINE static constexpr auto& to_node(T& x) noexcept
    { return static_cast<rbtree_node&>(static_cast<rbtree_link<Tag>&>(x)); }

    AMP_INLINE static constexpr auto& to_value(rbtree_node& x) noexcept
    { return static_cast<T&>(static_cast<rbtree_link<Tag>&>(x)); }

    AMP_INLINE static constexpr auto& to_value(rbtree_node const& x) noexcept
    { return static_cast<T const&>(static_cast<rbtree_link<Tag> const&>(x)); }

public:
    using value_compare          = Compare;
    using value_type             = T;
    using pointer                = T*;
    using const_pointer          = T const*;
    using iterator               = intrusive::rbtree_iterator<T, Tag>;
    using const_iterator         = intrusive::rbtree_const_iterator<T, Tag>;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;

    constexpr rbtree()
    noexcept(is_nothrow_copy_constructible_v<value_compare> &&
             is_nothrow_default_constructible_v<value_compare>) :
        comp_and_size_(value_compare())
    {}

    constexpr explicit rbtree(value_compare const& comp)
    noexcept(is_nothrow_copy_constructible_v<value_compare>) :
        comp_and_size_(comp)
    {}

    rbtree(rbtree&& x)
    noexcept(is_nothrow_move_constructible_v<value_compare>) :
        head_(x.head_),
        comp_and_size_(std::move(x.comp_and_size_))
    {
        fix_head_();
        x.clear();

        AMP_ASSERT(valid_());
        AMP_ASSERT(x.valid_());
    }

    rbtree& operator=(rbtree&& x) &
    noexcept(is_nothrow_move_assignable_v<value_compare>)
    {
        if (this != &x) {
            head_ = x.head_;
            comp_and_size_ = std::move(x.comp_and_size_);

            fix_head_();
            x.clear();
        }

        AMP_ASSERT(valid_());
        AMP_ASSERT(x.valid_());
        return *this;
    }

    void swap(rbtree& x)
    noexcept(is_nothrow_swappable_v<value_compare>)
    {
        if (this != &x) {
            using std::swap;
            swap(head_, x.head_);
            swap(comp_and_size_, x.comp_and_size_);

            fix_head_();
            x.fix_head_();
        }

        AMP_ASSERT(valid_());
        AMP_ASSERT(x.valid_());
    }

    AMP_INLINE constexpr value_compare const& value_comp() const noexcept
    { return comp_and_size_.first(); }

    AMP_INLINE constexpr value_compare& value_comp() noexcept
    { return comp_and_size_.first(); }

    AMP_INLINE constexpr size_type size() const noexcept
    { return comp_and_size_.second(); }

    AMP_INLINE constexpr bool empty() const noexcept
    { return (size() == 0); }

    AMP_INLINE constexpr auto begin() noexcept
    { return iterator(head_.left); }

    AMP_INLINE constexpr auto end() noexcept
    { return iterator(&head_); }

    AMP_INLINE constexpr auto begin() const noexcept
    { return const_iterator(head_.left); }

    AMP_INLINE constexpr auto end() const noexcept
    { return const_iterator(const_cast<rbtree_node*>(&head_)); }

    AMP_INLINE constexpr auto cbegin() const noexcept
    { return const_iterator(head_.left); }

    AMP_INLINE constexpr auto cend() const noexcept
    { return const_iterator(const_cast<rbtree_node*>(&head_)); }

    AMP_INLINE auto rbegin() noexcept
    { return reverse_iterator(end()); }

    AMP_INLINE auto rend() noexcept
    { return reverse_iterator(begin()); }

    AMP_INLINE auto rbegin() const noexcept
    { return const_reverse_iterator(end()); }

    AMP_INLINE auto rend() const noexcept
    { return const_reverse_iterator(begin()); }

    AMP_INLINE auto crbegin() const noexcept
    { return const_reverse_iterator(cend()); }

    AMP_INLINE auto crend() const noexcept
    { return const_reverse_iterator(cbegin()); }


    template<typename K>
    auto lower_bound(K const& k) const noexcept
    { return lower_bound_(k, root_(), const_cast<rbtree_node*>(&head_)); }

    template<typename K>
    auto upper_bound(K const& k) const noexcept
    { return upper_bound_(k, root_(), const_cast<rbtree_node*>(&head_)); }

    template<typename K>
    auto find(K const& k) const noexcept
    {
        auto pos = lower_bound(k);
        if (pos != end() && !value_comp()(k, *pos)) {
            return pos;
        }
        return end();
    }

    template<typename K>
    auto equal_range_unique(K const& k) const noexcept
    {
        auto y = const_cast<rbtree_node*>(&head_);
        for (auto x = root_(); x != nullptr; ) {
            if (value_comp()(k, to_value(*x))) {
                y = x;
                x = x->left;
            }
            else if (value_comp()(to_value(*x), k)) {
                x = x->right;
            }
            else {
                auto const pos = const_iterator{x};
                return std::make_pair(pos, std::next(pos, (pos != cend())));
            }
        }
        return std::make_pair(const_iterator{y}, const_iterator{y});
    }

    template<typename K>
    auto equal_range_multi(K const& k) const noexcept
    {
        auto y = const_cast<rbtree_node*>(&head_);
        for (auto x = root_(); x != nullptr; ) {
            if (value_comp()(k, to_value(*x))) {
                y = x;
                x = x->left;
            }
            else if (value_comp()(to_value(*x), k)) {
                x = x->right;
            }
            else {
                return std::make_pair(lower_bound_(k, x->left,  x),
                                      upper_bound_(k, x->right, y));
            }
        }
        return std::make_pair(const_iterator{y}, const_iterator{y});
    }

    template<typename K>
    auto count_unique(K const& k) const noexcept
    { return static_cast<size_type>(find(k) != end()); }

    template<typename K>
    auto count_multi(K const& k) const noexcept
    {
        auto pos = lower_bound(k);
        auto n = size_type{0};

        for (; pos != end() && !value_comp()(k, *pos++); ++n) {}
        return n;
    }

    template<typename K>
    auto lower_bound(K const& k) noexcept
    { return const_cast<rbtree const&>(*this).lower_bound(k).mut(); }

    template<typename K>
    auto upper_bound(K const& k) noexcept
    { return const_cast<rbtree const&>(*this).upper_bound(k).mut(); }

    template<typename K>
    auto find(K const& k) noexcept
    { return const_cast<rbtree const&>(*this).find(k).mut(); }

    template<typename K>
    auto equal_range_unique(K const& k) noexcept
    {
        auto r = const_cast<rbtree const&>(*this).equal_range_unique(k);
        return std::make_pair(r.first.mut(), r.second.mut());
    }

    template<typename K>
    auto equal_range_multi(K const& k) noexcept
    {
        auto r = const_cast<rbtree const&>(*this).equal_range_multi(k);
        return std::make_pair(r.first.mut(), r.second.mut());
    }


    std::pair<iterator, bool> insert_unique(value_type& v) noexcept
    {
        insert_point point;
        auto ret = insert_unique_prepare_(v, point);
        if (ret.second) {
            ret.first = insert_commit_(v, point);
        }
        return ret;
    }

    iterator insert_unique(const_iterator hint, value_type& v) noexcept
    {
        insert_point point;
        auto ret = insert_unique_prepare_(hint, v, point);
        if (ret.second) {
            ret.first = insert_commit_(v, point);
        }
        return ret.first;
    }

    template<typename InIt>
    void insert_unique(InIt first, InIt const last)
    {
        for (; first != last; ++first) {
            insert_unique(*first);
        }
    }

    iterator insert_multi(value_type& v) noexcept
    {
        insert_point point;
        insert_multi_prepare_(v, point);
        return insert_commit_(v, point);
    }

    iterator insert_multi(const_iterator hint, value_type& v) noexcept
    {
        insert_point point;
        insert_multi_prepare_(hint, v, point);
        return insert_commit_(v, point);
    }

    template<typename InIt>
    void insert_multi(InIt first, InIt const last)
    {
        for (; first != last; ++first) {
            insert_multi(*first);
        }
    }

    AMP_INLINE void clear() noexcept
    {
        head_ = {&head_, &head_, 0};
        comp_and_size_.second() = 0;
        AMP_ASSERT(valid_());
    }

    template<typename Dispose>
    void clear_and_dispose(Dispose dispose) noexcept
    {
        static_assert(noexcept(dispose(std::declval<pointer>())),
                      "disposer must be noexcept");

        auto x = root_();
        while (x != nullptr) {
            auto y = x->left;
            if (y != nullptr) {
                x->left = y->right;
                y->right = x;
            }
            else {
                y = x->right;
                dispose(std::addressof(to_value(*x)));
            }
            x = y;
        }
        clear();
    }

    auto erase(const_iterator pos) noexcept
    {
        aux::erase_and_rebalance((pos++).node_, head_);
        --comp_and_size_.second();

        AMP_ASSERT(valid_());
        return pos.mut();
    }

    auto erase(const_iterator first, const_iterator last) noexcept
    {
        if (first == cbegin() && last == cend()) {
            clear();
        }
        else {
            auto erased = size_type{0};
            while (first != last) {
                aux::erase_and_rebalance((first++).node_, head_);
                ++erased;
            }
            comp_and_size_.second() -= erased;
        }

        AMP_ASSERT(valid_());
        return last.mut();
    }

private:
    struct insert_point
    {
        rbtree_node* node;
        bool link_left;
    };

    std::pair<iterator, bool> insert_unique_prepare_(
        value_type const& v,
        insert_point& point) noexcept
    {
        auto x = root_();
        auto y = const_cast<rbtree_node*>(&head_);
        auto p = static_cast<rbtree_node*>(nullptr);

        auto left_child = true;
        while (x != nullptr) {
            y = x;
            x = (left_child = value_comp()(v, to_value(*x)))
              ? x->left
              : (p = y, x->right);
        }

        auto const vacant = !p || value_comp()(to_value(*p), v);
        if (vacant) {
            point.link_left = left_child;
            point.node = y;
        }
        return {iterator(p), vacant};
    }

    std::pair<iterator, bool> insert_unique_prepare_(
        const_iterator hint,
        value_type const& v,
        insert_point& point) noexcept
    {
        if (hint == cend() || value_comp()(v, *hint)) {
            auto prev = hint;
            if (hint == begin() || value_comp()(*(--prev), v)) {
                point.link_left = !root_() || !hint.node_->left;
                point.node = point.link_left ? hint.node_ : prev.node_;
                return {iterator(), true};
            }
        }
        return insert_unique_prepare_(v, point);
    }

    void insert_multi_prepare_(value_type const& v,
                               insert_point& point) noexcept
    {
        return insert_multi_upper_bound_prepare_(v, point);
    }

    void insert_multi_prepare_(const_iterator hint, value_type const& v,
                               insert_point& point) noexcept
    {
        if (hint == cend() || !value_comp()(*hint, v)) {
            auto prev = hint;
            if (hint == begin() || !value_comp()(v, *(--prev))) {
                point.link_left = !root_() || !hint.node_->left;
                point.node = point.link_left ? hint.node_ : prev.node_;
            }
            else {
                insert_multi_upper_bound_prepare_(v, point);
            }
        }
        else {
            insert_multi_lower_bound_prepare_(v, point);
        }
    }

    void insert_multi_upper_bound_prepare_(value_type const& v,
                                           insert_point& point) noexcept
    {
        auto y = const_cast<rbtree_node*>(&head_);
        for (auto x = root_(); x != nullptr; ) {
            y = x;
            x = value_comp()(v, to_value(*x)) ? x->left : x->right;
        }
        point.link_left = (y == &head_) || value_comp()(v, to_value(*y));
        point.node = y;
    }

    void insert_multi_lower_bound_prepare_(value_type const& v,
                                           insert_point& point) noexcept
    {
        auto y = const_cast<rbtree_node*>(&head_);
        for (auto x = root_(); x != nullptr; ) {
            y = x;
            x = !value_comp()(to_value(*x), v) ? x->left : x->right;
        }
        point.link_left = (y == &head_) || !value_comp()(to_value(*y), v);
        point.node = y;
    }

    iterator insert_commit_(value_type& v, insert_point const point) noexcept
    {
        auto const x = &to_node(v);
        aux::insert_and_rebalance(x, point.node, head_, point.link_left);
        ++comp_and_size_.second();

        AMP_ASSERT(valid_());
        return iterator(x);
    }

    template<typename K>
    const_iterator lower_bound_(K const& k,
                                rbtree_node* x,
                                rbtree_node* y) const noexcept
    {
        while (x != nullptr) {
            if (!value_comp()(to_value(*x), k)) {
                y = x;
                x = x->left;
            }
            else {
                x = x->right;
            }
        }
        return const_iterator(y);
    }

    template<typename K>
    const_iterator upper_bound_(K const& k,
                                rbtree_node* x,
                                rbtree_node* y) const noexcept
    {
        while (x != nullptr) {
            if (value_comp()(k, to_value(*x))) {
                y = x;
                x = x->left;
            }
            else {
                x = x->right;
            }
        }
        return const_iterator(y);
    }

    AMP_INLINE rbtree_node* root_() const noexcept
    { return parent(&head_); }

    void fix_head_() noexcept
    {
        if (root_()) {
            set_parent(root_(), &head_);
        }
        else {
            head_ = {&head_, &head_, 0};
        }
    }

#ifdef AMP_DEBUG
    bool valid_() const noexcept;
#endif

    rbtree_node head_{&head_, &head_, 0};
    compressed_pair<value_compare, size_type> comp_and_size_;
};


template<typename T, typename Compare, typename Tag>
inline void swap(rbtree<T, Compare, Tag>& x, rbtree<T, Compare, Tag>& y)
noexcept(noexcept(x.swap(y)))
{
    x.swap(y);
}


#ifdef AMP_DEBUG
template<typename T, typename Compare, typename Tag>
inline bool rbtree<T, Compare, Tag>::valid_() const noexcept
{
    if (empty()) {
        return (head_.left == &head_) && (head_.right == &head_);
    }

    auto black_node_count = [this](rbtree_node const* x) noexcept {
        auto count = 0_sz;
        for (; x != nullptr; x = parent(x)) {
            count += (color(x) == rbtree_color::black);
            if (x == &head_) {
                break;
            }
        }
        return count;
    };

    if (head_.left != minimum(root_())) {
        return false;
    }
    if (head_.right != maximum(root_())) {
        return false;
    }

    auto const black_height = black_node_count(head_.left);
    auto encountered_size = 0_sz;

    auto const last = end();
    for (auto first = begin(); first != last; ++first, ++encountered_size) {
        auto const x = first.node_;

        if (color(x) == rbtree_color::red) {
            if (x->left && color(x->left) == rbtree_color::red) {
                return false;
            }
            if (x->right && color(x->right) == rbtree_color::red) {
                return false;
            }
        }

        auto const left  = const_iterator(x->left);
        auto const right = const_iterator(x->right);

        if (left == const_iterator() && right == const_iterator()) {
            if (black_node_count(x) != black_height) {
                return false;
            }
        }
        else {
            if (right != const_iterator() && value_comp()(*right, *first)) {
                return false;
            }
            if (left != const_iterator() && value_comp()(*first, *left)) {
                return false;
            }
        }
    }

    return (size() == encountered_size);
}
#endif  // AMP_DEBUG

}}    // namespace amp::intrusive


#endif  // AMP_INCLUDED_43E91639_60AA_42B2_8C9F_EE2536BC51BC

