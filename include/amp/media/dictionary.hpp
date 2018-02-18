////////////////////////////////////////////////////////////////////////////////
//
// amp/media/dictionary.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_B3437A79_63A6_4451_981C_F424BCC2249E
#define AMP_INCLUDED_B3437A79_63A6_4451_981C_F424BCC2249E


#include <amp/aux/iterator.hpp>
#include <amp/aux/map_value_compare.hpp>
#include <amp/error.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>
#include <amp/u8string.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>


namespace amp {
namespace media {

class dictionary
{
public:
    using key_type        = u8string;
    using mapped_type     = u8string;
    using value_type      = std::pair<key_type, mapped_type>;
    using value_compare   = map_value_compare<key_type, mapped_type>;
    using pointer         = value_type*;
    using const_pointer   = value_type const*;
    using iterator        = value_type*;
    using const_iterator  = value_type const*;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

    dictionary() = default;

    dictionary(dictionary const& x) :
        size_{x.size()},
        capacity_{size_}
    {
        if (!empty()) {
            auto const tmp = std::malloc(size() * sizeof(value_type));
            if (tmp == nullptr) {
                raise_bad_alloc();
            }
            data_ = static_cast<pointer>(tmp);
            std::uninitialized_copy(x.begin(), x.end(), begin());
        }
    }

    dictionary(dictionary&& x) noexcept :
        data_{std::exchange(x.data_, nullptr)},
        size_{std::exchange(x.size_, 0)},
        capacity_{std::exchange(x.capacity_, 0)}
    {}

    dictionary(std::initializer_list<value_type> ilist)
    {
        insert(ilist);
    }

    dictionary& operator=(dictionary const& x) &
    {
        dictionary{x}.swap(*this);
        return *this;
    }

    dictionary& operator=(dictionary&& x) & noexcept
    {
        dictionary{std::move(x)}.swap(*this);
        return *this;
    }

    dictionary& operator=(std::initializer_list<value_type> ilist) &
    {
        assign(ilist);
        return *this;
    }

    ~dictionary()
    {
        if (data_ != nullptr) {
            for (auto&& v : *this) {
                v.~value_type();
            }
            std::free(data_);
        }
    }

    void swap(dictionary& x) noexcept
    {
        using std::swap;
        swap(data_,     x.data_);
        swap(size_,     x.size_);
        swap(capacity_, x.capacity_);
    }

    constexpr auto value_comp() const noexcept
    { return value_compare{}; }

    size_type capacity() const noexcept
    { return capacity_; }

    size_type size() const noexcept
    { return size_; }

    bool empty() const noexcept
    { return (size() == 0); }

    iterator begin() noexcept
    { return data_; }

    iterator end() noexcept
    { return begin() + size(); }

    const_iterator begin() const noexcept
    { return cbegin(); }

    const_iterator end() const noexcept
    { return cend(); }

    const_iterator cbegin() const noexcept
    { return data_; }

    const_iterator cend() const noexcept
    { return cbegin() + size(); }

    template<typename K>
    auto lower_bound(K const& k) noexcept
    { return std::lower_bound(begin(), end(), k, value_comp()); }

    template<typename K>
    auto upper_bound(K const& k) noexcept
    { return std::upper_bound(begin(), end(), k, value_comp()); }

    template<typename K>
    auto equal_range(K const& k) noexcept
    { return std::equal_range(begin(), end(), k, value_comp()); }

    template<typename K>
    auto find(K const& k) noexcept
    {
        auto const pos = lower_bound(k);
        if (pos != end() && !value_comp()(k, *pos)) {
            return pos;
        }
        return end();
    }

    template<typename K>
    auto lower_bound(K const& k) const noexcept
    { return std::lower_bound(begin(), end(), k, value_comp()); }

    template<typename K>
    auto upper_bound(K const& k) const noexcept
    { return std::upper_bound(begin(), end(), k, value_comp()); }

    template<typename K>
    auto equal_range(K const& k) const noexcept
    { return std::equal_range(begin(), end(), k, value_comp()); }

    template<typename K>
    auto find(K const& k) const noexcept
    {
        auto const pos = lower_bound(k);
        if (pos != end() && !value_comp()(k, *pos)) {
            return pos;
        }
        return end();
    }

    template<typename K>
    bool contains(K const& k) const noexcept
    { return (find(k) != end()); }

    template<typename K>
    size_type count(K const& k) const noexcept
    {
        auto const r = equal_range(k);
        return static_cast<size_type>(std::distance(r.first, r.second));
    }

    template<
        typename V,
        typename = enable_if_t<is_convertible_v<V, value_type>>
    >
    iterator insert(V&& v)
    { return insert_at_(upper_bound(v), std::forward<V>(v)); }

    iterator insert(value_type const& v)
    { return insert<value_type const&>(v); }

    iterator insert(value_type&& v)
    { return insert<value_type&&>(std::move(v)); }

    template<
        typename V,
        typename = enable_if_t<is_convertible_v<V, value_type>>
    >
    iterator insert(const_iterator hint, V&& v)
    {
        if (hint == cend() || !value_comp()(*hint, v)) {
            if (hint != cbegin() && value_comp()(v, hint[-1])) {
                hint = std::upper_bound(cbegin(), hint, v, value_comp());
            }
        }
        else {
            hint = std::lower_bound(hint, cend(), v, value_comp());
        }
        return insert_at_(const_cast<iterator>(hint), std::forward<V>(v));
    }

    iterator insert(const_iterator const hint, value_type const& v)
    { return insert<value_type const&>(hint, v); }

    iterator insert(const_iterator const hint, value_type&& v)
    { return insert<value_type&&>(hint, std::move(v)); }

    template<typename It>
    void insert(It first, It last)
    { return insert_range_(first, last, iterator_category_t<It>{}); }

    void insert(std::initializer_list<value_type> ilist)
    { insert(ilist.begin(), ilist.end()); }

    template<typename... Args>
    iterator emplace(Args&&... args)
    { return insert(value_type(std::forward<Args>(args)...)); }

    template<typename... Args>
    iterator emplace_hint(const_iterator const hint, Args&&... args)
    { return insert(hint, value_type(std::forward<Args>(args)...)); }

    template<typename K, typename M>
    std::pair<iterator, bool> try_emplace(K&& k, M&& m)
    {
        auto pos = lower_bound(k);
        if (pos != end() && !value_comp()(k, *pos)) {
            return {pos, false};
        }

        pos = emplace_hint(pos, std::forward<K>(k), std::forward<M>(m));
        return {pos, true};
    }

    template<typename K, typename M>
    iterator insert_or_assign(K&& k, M&& m)
    {
        auto const pos = lower_bound(k);
        if (pos == end() || value_comp()(k, *pos)) {
            return emplace_hint(pos, std::forward<K>(k), std::forward<M>(m));
        }

        pos->second = std::forward<M>(m);
        erase(pos + 1, upper_bound(k));
        return pos;
    }

    size_type erase(key_type const& k) noexcept
    {
        auto const r = equal_range(k);
        auto const n = std::distance(r.first, r.second);
        erase(r.first, r.second);
        return static_cast<size_type>(n);
    }

    iterator erase(const_iterator const pos) noexcept
    {
        AMP_ASSERT((pos != cend()) && "cannot erase end iterator");
        return erase(pos, pos + 1);
    }

    iterator erase(const_iterator const first,
                   const_iterator const last) noexcept
    {
        AMP_ASSERT((first <= last)     && "invalid iterator range");
        AMP_ASSERT((first >= cbegin()) && "foregin begin iterator");
        AMP_ASSERT((last  <= cend())   && "foregin end iterator");

        auto const n = static_cast<size_type>(std::distance(first, last));
        if (n != 0) {
            auto pos = std::move(const_cast<iterator>(last), end(),
                                 const_cast<iterator>(first));
            for (; pos != end(); ++pos) {
                pos->~value_type();
            }
            size_ -= n;
        }
        return const_cast<iterator>(first);
    }

    void clear() noexcept
    {
        if (data_ != nullptr) {
            for (auto&& v : *this) {
                v.~value_type();
            }
            std::free(data_);

            data_ = nullptr;
            size_ = 0;
            capacity_ = 0;
        }
    }

    void reserve(size_type const n)
    {
        if (capacity_ < n) {
            auto const tmp = std::realloc(data_, n * sizeof(value_type));
            if (tmp == nullptr) {
                raise_bad_alloc();
            }
            data_ = static_cast<pointer>(tmp);
            capacity_ = n;
        }
    }

    void assign(std::initializer_list<value_type> ilist)
    {
        clear();
        insert(ilist);
    }

    void merge(dictionary const& x)
    {
        if (empty()) {
            *this = x;
            return;
        }

        auto const last = x.end();
        for (auto first = x.begin(); first != last; ) {
            auto pos = find(first->first);
            if (pos != end()) {
                ++first;
            }
            else {
                auto&& key = first->first;
                do {
                    insert(*first++);
                }
                while (first != last && first->first == key);
            }
        }
    }

    template<
        typename V,
        typename = enable_if_t<is_convertible_v<V, value_type>>
    >
    iterator insert_no_intern(const_iterator hint, V&& v)
    {
        if (hint == cend() || !value_comp()(*hint, v)) {
            if (hint != cbegin() && value_comp()(v, hint[-1])) {
                hint = std::upper_bound(cbegin(), hint, v, value_comp());
            }
        }
        else {
            hint = std::lower_bound(hint, cend(), v, value_comp());
        }
        return insert_at_no_intern_(const_cast<iterator>(hint),
                                    std::forward<V>(v));
    }

    iterator insert_no_intern(const_iterator const hint, value_type const& v)
    { return insert_no_intern<value_type const&>(hint, v); }

    iterator insert_no_intern(const_iterator const hint, value_type&& v)
    { return insert_no_intern<value_type&&>(hint, std::move(v)); }

    template<typename... Args>
    iterator emplace_hint_no_intern(const_iterator const hint, Args&&... args)
    { return insert_no_intern(hint, value_type(std::forward<Args>(args)...)); }

private:
    template<typename V>
    iterator insert_at_no_intern_(iterator pos, V&& v)
    {
        if (size() == capacity()) {
            auto const offset = std::distance(begin(), pos);
            reserve(std::max(capacity() * 2, size_type{2}));
            pos = std::next(begin(), offset);
        }
        if (pos != end()) {
            ::new(static_cast<void*>(end())) value_type();
            std::move_backward(pos, end(), end() + 1);
        }

        ::new(static_cast<void*>(pos)) value_type(std::forward<V>(v));
        size_ += 1;
        return pos;
    }

    template<typename V>
    iterator insert_at_(iterator pos, V&& v)
    {
        pos = insert_at_no_intern_(pos, std::forward<V>(v));
        pos->first.intern();
        pos->second.intern();
        return pos;
    }

    template<typename InIt>
    void insert_range_(InIt first, InIt last, std::input_iterator_tag)
    {
        for (; first != last; ++first) {
            insert(*first);
        }
    }

    template<typename FwdIt>
    void insert_range_(FwdIt first, FwdIt last, std::forward_iterator_tag)
    {
        auto const n = static_cast<size_type>(std::distance(first, last));
        if (n != 0) {
            reserve(size() + n);
            for (; first != last; ++first) {
                insert(*first);
            }
        }
    }

    pointer   data_{};
    size_type size_{};
    size_type capacity_{};
};


inline void swap(dictionary& x, dictionary& y) noexcept
{
    x.swap(y);
}

}}    // namespace amp::media


#endif  // AMP_INCLUDED_B3437A79_63A6_4451_981C_F424BCC2249E

