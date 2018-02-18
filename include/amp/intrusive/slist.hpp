////////////////////////////////////////////////////////////////////////////////
//
// amp/intrusive/slist.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_114266F8_9BFA_4436_AB3B_777BDBE0C15A
#define AMP_INCLUDED_114266F8_9BFA_4436_AB3B_777BDBE0C15A


#include <amp/aux/operators.hpp>
#include <amp/functional.hpp>
#include <amp/stddef.hpp>
#include <amp/utility.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <utility>


namespace amp {
namespace intrusive {

struct slist_node
{
    slist_node* next;
};


template<typename Tag = void>
struct slist_link : slist_node {};


template<typename, typename = void>
class slist;

template<typename, typename>
class slist_const_iterator;


template<typename T, typename Tag>
class slist_iterator :
    private forward_iteratable<slist_iterator<T, Tag>>
{
public:
    using value_type        = T;
    using reference         = T&;
    using pointer           = T*;
    using difference_type   = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;

    constexpr slist_iterator() noexcept :
        node_{nullptr}
    {}

    constexpr reference operator*() const noexcept
    { return static_cast<reference>(static_cast<slist_link<Tag>&>(*node_)); }

    constexpr pointer operator->() const noexcept
    { return std::addressof(**this); }

    slist_iterator& operator++() noexcept
    { return (node_ = node_->next, *this); }

private:
    template<typename, typename>
    friend class slist;
    template<typename, typename>
    friend class slist_const_iterator;

    constexpr slist_iterator(slist_node* const x) noexcept :
        node_{x}
    {}

    constexpr friend bool operator==(slist_iterator const& x,
                                     slist_iterator const& y) noexcept
    { return (x.node_ == y.node_); }

    slist_node* node_;
};

template<typename T, typename Tag>
class slist_const_iterator :
    private forward_iteratable<slist_const_iterator<T, Tag>>
{
public:
    using value_type        = T;
    using reference         = T const&;
    using pointer           = T const*;
    using difference_type   = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;

    constexpr slist_const_iterator() noexcept :
        node_{nullptr}
    {}

    constexpr slist_const_iterator(slist_iterator<T, Tag> const& x) noexcept :
        node_{x.node_}
    {}

    constexpr reference operator*() const noexcept
    {
        return static_cast<reference>(
            static_cast<slist_link<Tag> const&>(*node_));
    }

    constexpr pointer operator->() const noexcept
    { return std::addressof(**this); }

    slist_const_iterator& operator++() noexcept
    { return (node_ = node_->next, *this); }

private:
    template<typename, typename>
    friend class slist;

    constexpr friend bool operator==(slist_const_iterator const& x,
                                     slist_const_iterator const& y) noexcept
    { return (x.node_ == y.node_); }

    constexpr slist_const_iterator(slist_node const* const x) noexcept :
        node_{const_cast<slist_node*>(x)}
    {}

    constexpr auto mut() const noexcept
    { return slist_iterator<T, Tag>{node_}; }

    slist_node* node_;
};


template<typename T, typename Tag>
class slist
{
private:
    AMP_INLINE static constexpr auto& to_node(T& x) noexcept
    { return static_cast<slist_node&>(static_cast<slist_link<Tag>&>(x)); }

    AMP_INLINE static constexpr auto& to_value(slist_node& x) noexcept
    { return static_cast<T&>(static_cast<slist_link<Tag>&>(x)); }

    AMP_INLINE static constexpr auto& to_value(slist_node const& x) noexcept
    { return static_cast<T const&>(static_cast<slist_link<Tag> const&>(x)); }

public:
    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = T const*;
    using iterator        = intrusive::slist_iterator<T, Tag>;
    using const_iterator  = intrusive::slist_const_iterator<T, Tag>;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

    constexpr slist() noexcept = default;

    slist(slist&& x) noexcept :
        head_{std::exchange(x.head_, slist_node{})},
        tail_{std::exchange(x.tail_, &x.head_)},
        size_{std::exchange(x.size_, size_type{})}
    {
        if (empty()) {
            tail_ = &head_;
        }
    }

    slist& operator=(slist&& x) & noexcept
    {
        slist{std::move(x)}.swap(*this);
        return *this;
    }

    void swap(slist& x) noexcept
    {
        using std::swap;
        swap(head_, x.head_);
        swap(tail_, x.tail_);
        swap(size_, x.size_);

        if (empty()) {
            tail_ = &head_;
        }
        if (x.empty()) {
            x.tail_ = &x.head_;
        }
    }

    constexpr size_type size() const noexcept
    { return size_; }

    constexpr bool empty() const noexcept
    { return (size() == 0); }

    iterator before_begin() noexcept
    { return iterator{&head_}; }

    iterator begin() noexcept
    { return iterator{head_.next}; }

    iterator end() noexcept
    { return iterator{}; }

    constexpr const_iterator before_begin() const noexcept
    { return cbefore_begin(); }

    constexpr const_iterator begin() const noexcept
    { return cbegin(); }

    constexpr const_iterator end() const noexcept
    { return cend(); }

    constexpr const_iterator cbefore_begin() const noexcept
    { return const_iterator{&head_}; }

    constexpr const_iterator cbegin() const noexcept
    { return const_iterator{head_.next}; }

    constexpr const_iterator cend() const noexcept
    { return const_iterator{}; }

    value_type& front() noexcept
    { return to_value(*head_.next); }

    value_type& back() noexcept
    { return to_value(*tail_); }

    constexpr value_type const& front() const noexcept
    { return to_value(*head_.next); }

    constexpr value_type const& back() const noexcept
    { return to_value(*tail_); }

    void pop_front() noexcept
    {
        unlink_after_(&head_);
        if (empty()) {
            tail_ = &head_;
        }
        size_ -= 1;
    }

    void push_front(value_type& v) noexcept
    {
        auto const n = &to_node(v);
        if (empty()) {
            tail_ = n;
        }
        link_after_(&head_, n);
        ++size_;
    }

    void push_back(value_type& v) noexcept
    {
        auto const n = &to_node(v);
        link_after_(tail_, n);
        tail_ = n;
        ++size_;
    }

    iterator insert_after(const_iterator prev, value_type& v) noexcept
    {
        auto const n = &to_node(v);
        link_after_(prev.node_, n);

        if (tail_ == prev.node_) {
            tail_ = n;
        }
        ++size_;
        return iterator{n};
    }

    template<typename Dispose>
    void clear_and_dispose(Dispose dispose) noexcept
    {
        for (auto it = begin(); it != end(); ) {
            std::invoke(dispose, std::addressof(*it++));
        }
        clear();
    }

    void clear() noexcept
    {
        head_ = {};
        tail_ = &head_;
        size_ = {};
    }

    iterator erase_after(const_iterator before) noexcept
    {
        return erase_after_and_dispose(before, [](T*) noexcept {});
    }

    iterator erase_after(const_iterator const prev, const_iterator const last,
                         size_type const n) noexcept
    {
        AMP_ASSERT(std::distance(std::next(prev), last) == as_signed(n));

        if (last == cend()) {
            tail_ = prev.node_;
        }

        prev.node_->next = last.node_;
        size_ -= n;
        return last.mut();
    }

    template<typename Dispose>
    iterator erase_after_and_dispose(const_iterator prev, Dispose dispose)
    noexcept(is_nothrow_invocable_v<Dispose, T*>)
    {
        auto it = std::next(prev).mut();
        unlink_after_(prev.node_);

        if (tail_ == it.node_) {
            tail_ = prev.node_;
        }
        std::invoke(dispose, std::addressof(*it++));
        --size_;
        return it;
    }

    void splice_after(const_iterator const prev, slist& x) noexcept
    {
        if (!x.empty()) {
            auto const last = std::exchange(x.tail_, &x.head_);
            if (tail_ == prev.node_) {
                tail_ = last;
            }
            transfer_after_(prev.node_, &x.head_, last);
            size_ += std::exchange(x.size_, 0);
        }
    }

    void splice_after(const_iterator const prev, slist& x,
                      const_iterator const pos) noexcept
    {
        splice_after(prev, x, pos, std::next(pos), 1);
    }

    void splice_after(const_iterator const pos, slist& x,
                      const_iterator const first, const_iterator const last,
                      size_type const n) noexcept
    {
        AMP_ASSERT(std::distance(first, last) == as_signed(n));

        if (first != last) {
            if (tail_ == pos.node_) {
                tail_ = last.node_;
            }
            if (x.tail_ == last.node_) {
                x.tail_ = first.node_;
            }
            transfer_after_(pos.node_, first.node_, last.node_);
            size_ += n;
            x.size_ -= n;
        }
    }

    void splice_after(const_iterator prev, slist&& x) noexcept
    {
        splice_after(prev, static_cast<slist&>(x));
    }

    void splice_after(const_iterator prev, slist&& x,
                      const_iterator it) noexcept
    {
        splice_after(prev, static_cast<slist&>(x), it);
    }

    void splice_after(const_iterator prev, slist&& x,
                      const_iterator head, const_iterator tail,
                      size_type n) noexcept
    {
        splice_after(prev, static_cast<slist&>(x), head, tail, n);
    }

    template<typename Predicate, typename Dispose>
    void remove_and_dispose_if(Predicate pred, Dispose dispose)
    noexcept(is_nothrow_invocable_v<Predicate, T const&> &&
             is_nothrow_invocable_v<Dispose, T*>)
    {
        auto prev = before_begin();
        auto curr = begin();

        while (curr != cend()) {
            if (std::invoke(pred, *curr)) {
                curr = erase_after_and_dispose(prev, dispose);
            }
            else {
                prev = curr++;
            }
        }

        tail_ = prev.node_;
    }

    template<typename Predicate>
    void remove_if(Predicate pred)
    noexcept(is_nothrow_invocable_v<Predicate, T const&, T const&>)
    {
        remove_and_dispose_if(std::move(pred), [](T*) noexcept {});
    }

    void remove(value_type const& v)
    {
        remove_if([&v](T const& x) { return (v == x); });
    }

private:
    slist_node  head_{};
    slist_node* tail_{&head_};
    size_type   size_{};

    static void transfer_after_(slist_node* const p, slist_node* const b,
                                slist_node* const e) noexcept
    {
        if (p != b && p != e && b != e) {
            auto const next_b = b->next;
            auto const next_e = e->next;
            auto const next_p = p->next;

            b->next = next_e;
            e->next = next_p;
            p->next = next_b;
        }
    }

    static void link_after_(slist_node* const p, slist_node* const n) noexcept
    {
        AMP_ASSERT(p != nullptr);
        n->next = std::exchange(p->next, n);
    }

    static void unlink_after_(slist_node* const p) noexcept
    {
        AMP_ASSERT(p->next != nullptr);

        auto const n = p->next;
        p->next = n->next;
    }
};

}}    // namespace amp::intrusive


#endif  // AMP_INCLUDED_114266F8_9BFA_4436_AB3B_777BDBE0C15A

