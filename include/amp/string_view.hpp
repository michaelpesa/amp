////////////////////////////////////////////////////////////////////////////////
//
// amp/string_view.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_4E72C427_2A3B_4ADB_96F5_018AF99C7F2E
#define AMP_INCLUDED_4E72C427_2A3B_4ADB_96F5_018AF99C7F2E


#include <amp/aux/string_search.hpp>
#include <amp/cxp/string.hpp>
#include <amp/error.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>
#include <amp/utility.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <limits>
#include <string>
#include <utility>


namespace amp {

template<typename Char, typename Traits = std::char_traits<Char>>
class basic_string_view
{
public:
    using traits_type            = Traits;
    using value_type             = Char;
    using pointer                = Char const*;
    using const_pointer          = Char const*;
    using iterator               = pointer;
    using const_iterator         = const_pointer;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;

    static constexpr auto npos = static_cast<size_type>(-1);

    constexpr basic_string_view() noexcept :
        str_{nullptr},
        len_{0}
    {}

    constexpr basic_string_view(const_pointer const s,
                                size_type const n) noexcept :
        str_{s},
        len_{n}
    {
        AMP_ASSERT((s != nullptr || n == 0) && "null string");
    }

    constexpr basic_string_view(const_pointer const s) noexcept :
        str_{s},
        len_{s ? cxp::strlen(s) : 0}
    {}

    template<typename A>
    basic_string_view(std::basic_string<Char, Traits, A> const& s) noexcept :
        basic_string_view{s.data(), s.size()}
    {}

    constexpr basic_string_view(basic_string_view const&) noexcept = default;
    basic_string_view& operator=(basic_string_view const&) & noexcept = default;

    constexpr const_iterator begin() const noexcept
    { return cbegin(); }

    constexpr const_iterator end() const noexcept
    { return cend(); }

    constexpr const_iterator cbegin() const noexcept
    { return data(); }

    constexpr const_iterator cend() const noexcept
    { return data() + size(); }

    const_reverse_iterator rbegin() const noexcept
    { return crbegin(); }

    const_reverse_iterator rend() const noexcept
    { return crend(); }

    const_reverse_iterator crbegin() const noexcept
    { return const_reverse_iterator{end()}; }

    const_reverse_iterator crend() const noexcept
    { return const_reverse_iterator{begin()}; }

    constexpr const_pointer data() const noexcept
    { return str_; }

    constexpr size_type max_size() const noexcept
    { return std::numeric_limits<size_type>::max(); }

    constexpr size_type length() const noexcept
    { return len_; }

    constexpr size_type size() const noexcept
    { return length(); }

    constexpr bool empty() const noexcept
    { return size() == 0; }

    constexpr value_type const& operator[](size_type const i) const noexcept
    {
        AMP_ASSERT((i < size()) && "index out of bounds");
        return data()[i];
    }

    constexpr value_type const& at(size_type const i) const
    {
        if (i < size()) {
            return data()[i];
        }
        raise(errc::out_of_bounds);
    }

    constexpr value_type const& front() const noexcept
    {
        AMP_ASSERT(!empty() && "null string");
        return data()[0];
    }

    constexpr value_type const& back() const noexcept
    {
        AMP_ASSERT(!empty() && "null string");
        return data()[size() - 1];
    }

    void clear() noexcept
    {
        str_ = nullptr;
        len_ = 0;
    }

    void remove_prefix(size_type n) noexcept
    {
        n = std::min(n, size());
        str_ += n;
        len_ -= n;
    }

    void remove_suffix(size_type n) noexcept
    {
        n = std::min(n, size());
        len_ -= n;
    }

    void swap(basic_string_view& x) noexcept
    {
        using std::swap;
        swap(str_, x.str_);
        swap(len_, x.len_);
    }

    template<typename Allocator = std::allocator<Char>>
    auto to_string(Allocator const& a = Allocator()) const
    { return std::basic_string<Char, Traits, Allocator>(begin(), end(), a); }

    template<typename Allocator>
    explicit operator std::basic_string<Char, Traits, Allocator>() const
    { return std::basic_string<Char, Traits, Allocator>(begin(), end()); }

    template<typename OutIt>
    size_type copy(OutIt dest, size_type n, size_type const pos = 0) const
    {
        if (pos < size()) {
            n = std::min(n, size() - pos);
            std::copy_n(begin() + pos, n, dest);
            return n;
        }
        raise(errc::out_of_bounds);
    }

    constexpr basic_string_view substr(size_type const pos,
                                       size_type const n = npos) const
    {
        if (pos <= size()) {
            return basic_string_view{data() + pos, std::min(n, size() - pos)};
        }
        raise(errc::out_of_bounds);
    }

    int compare(basic_string_view const x) const noexcept
    {
        auto const rlen = std::min(size(), x.size());
        auto ret = traits_type::compare(data(), x.data(), rlen);
        if (ret == 0) {
            ret = (size() > x.size()) - (size() < x.size());
        }
        return ret;
    }

    int compare(size_type pos, size_type n, basic_string_view s) const
    { return substr(pos, n).compare(s); }

    int compare(size_type pos1, size_type n1, basic_string_view s,
                size_type pos2, size_type n2) const
    { return substr(pos1, n1).compare(s.substr(pos2, n2)); }

    int compare(size_type pos1, size_type n1, const_pointer s,
                size_type n2) const
    { return substr(pos1, n1).compare(basic_string_view{s, n2}); }


    size_type find(basic_string_view const s,
                   size_type const pos = 0) const noexcept
    { return find(s.data(), pos, s.size()); }

    size_type find(const_pointer const s, size_type const pos,
                   size_type const n) const noexcept
    { return aux::str_find<traits_type>(str_, len_, s, pos, n); }

    size_type find(value_type const c, size_type const pos = 0) const noexcept
    { return aux::str_find<traits_type>(str_, len_, c, pos); }


    size_type rfind(basic_string_view const s,
                    size_type const pos = npos) const noexcept
    { return rfind(s.data(), pos, s.size()); }

    size_type rfind(const_pointer const s, size_type const pos,
                    size_type const n) const noexcept
    { return aux::str_rfind<traits_type>(str_, len_, s, pos, n); }

    size_type rfind(value_type const c,
                    size_type const pos = npos) const noexcept
    { return aux::str_rfind<traits_type>(str_, len_, c, pos); }


    size_type find_first_of(basic_string_view const s,
                            size_type const pos = 0) const noexcept
    { return find_first_of(s.data(), pos, s.size()); }

    size_type find_first_of(const_pointer const s, size_type const pos,
                            size_type const n) const noexcept
    { return aux::str_find_first_of<traits_type>(str_, len_, s, pos, n); }

    size_type find_first_of(value_type const c,
                            size_type const pos = 0) const noexcept
    { return find(c, pos); }


    size_type find_last_of(basic_string_view const s,
                           size_type const pos = npos) const noexcept
    { return find_last_of(s.data(), pos, s.size()); }

    size_type find_last_of(const_pointer const s, size_type const pos,
                           size_type const n) const noexcept
    { return aux::str_find_last_of<traits_type>(str_, len_, s, pos, n); }

    size_type find_last_of(value_type const c,
                           size_type const pos = npos) const noexcept
    { return rfind(c, pos); }


    size_type find_first_not_of(basic_string_view const s,
                                size_type const pos = 0) const noexcept
    { return find_first_not_of(s.data(), pos, s.size()); }

    size_type find_first_not_of(const_pointer const s, size_type const pos,
                                size_type const n) const noexcept
    { return aux::str_find_first_not_of<traits_type>(str_, len_, s, pos, n); }

    size_type find_first_not_of(value_type const c,
                                size_type const pos = 0) const noexcept
    { return aux::str_find_first_not_of<traits_type>(str_, len_, c, pos); }


    size_type find_last_not_of(basic_string_view const s,
                               size_type const pos = npos) const noexcept
    { return find_last_not_of(s.data(), pos, s.size()); }

    size_type find_last_not_of(const_pointer const s, size_type const pos,
                               size_type const n) const noexcept
    { return aux::str_find_last_not_of<traits_type>(str_, len_, s, pos, n); }

    size_type find_last_not_of(value_type const c,
                               size_type const pos = npos) const noexcept
    { return aux::str_find_last_not_of<traits_type>(str_, len_, c, pos); }

private:
    pointer str_;
    size_type len_;
};


template<typename Char, typename Traits>
bool operator==(basic_string_view<Char, Traits> x,
                basic_string_view<Char, Traits> y) noexcept
{ return (x.size() == y.size()) && (x.compare(y) == 0); }

template<typename Char, typename Traits>
bool operator==(basic_string_view<Char, Traits> x,
                common_type_t<basic_string_view<Char, Traits>> y) noexcept
{ return (x.size() == y.size()) && (x.compare(y) == 0); }

template<typename Char, typename Traits>
bool operator==(common_type_t<basic_string_view<Char, Traits>> x,
                basic_string_view<Char, Traits> y) noexcept
{ return (x.size() == y.size()) && (x.compare(y) == 0); }

template<typename Char, typename Traits>
bool operator!=(basic_string_view<Char, Traits> x,
                basic_string_view<Char, Traits> y) noexcept
{ return (x.size() != y.size()) || (x.compare(y) != 0); }

template<typename Char, typename Traits>
bool operator!=(basic_string_view<Char, Traits> x,
                common_type_t<basic_string_view<Char, Traits>> y) noexcept
{ return (x.size() != y.size()) || (x.compare(y) != 0); }

template<typename Char, typename Traits>
bool operator!=(common_type_t<basic_string_view<Char, Traits>> x,
                basic_string_view<Char, Traits> y) noexcept
{ return (x.size() != y.size()) || (x.compare(y) != 0); }

template<typename Char, typename Traits>
bool operator<(basic_string_view<Char, Traits> x,
               basic_string_view<Char, Traits> y) noexcept
{ return (x.compare(y) < 0); }

template<typename Char, typename Traits>
bool operator<(basic_string_view<Char, Traits> x,
               common_type_t<basic_string_view<Char, Traits>> y) noexcept
{ return (x.compare(y) < 0); }

template<typename Char, typename Traits>
bool operator<(common_type_t<basic_string_view<Char, Traits>> x,
               basic_string_view<Char, Traits> y) noexcept
{ return (x.compare(y) < 0); }

template<typename Char, typename Traits>
bool operator>(basic_string_view<Char, Traits> x,
               basic_string_view<Char, Traits> y) noexcept
{ return (x.compare(y) > 0); }

template<typename Char, typename Traits>
bool operator>(basic_string_view<Char, Traits> x,
               common_type_t<basic_string_view<Char, Traits>> y) noexcept
{ return (x.compare(y) > 0); }

template<typename Char, typename Traits>
bool operator>(common_type_t<basic_string_view<Char, Traits>> x,
               basic_string_view<Char, Traits> y) noexcept
{ return (x.compare(y) > 0); }

template<typename Char, typename Traits>
bool operator<=(basic_string_view<Char, Traits> x,
                basic_string_view<Char, Traits> y) noexcept
{ return (x.compare(y) <= 0); }

template<typename Char, typename Traits>
bool operator<=(basic_string_view<Char, Traits> x,
                common_type_t<basic_string_view<Char, Traits>> y) noexcept
{ return (x.compare(y) <= 0); }

template<typename Char, typename Traits>
bool operator<=(common_type_t<basic_string_view<Char, Traits>> x,
                basic_string_view<Char, Traits> y) noexcept
{ return (x.compare(y) <= 0); }

template<typename Char, typename Traits>
bool operator>=(basic_string_view<Char, Traits> x,
                basic_string_view<Char, Traits> y) noexcept
{ return (x.compare(y) >= 0); }

template<typename Char, typename Traits>
bool operator>=(basic_string_view<Char, Traits> x,
                common_type_t<basic_string_view<Char, Traits>> y) noexcept
{ return (x.compare(y) >= 0); }

template<typename Char, typename Traits>
bool operator>=(common_type_t<basic_string_view<Char, Traits>> x,
                basic_string_view<Char, Traits> y) noexcept
{ return (x.compare(y) >= 0); }


template<
    typename Char,
    typename Traits,
    typename Allocator = std::allocator<Char>
>
auto to_string(basic_string_view<Char, Traits> const s,
               Allocator const& a = Allocator())
{
    return s.to_string(a);
}


using string_view    = basic_string_view<char>;
using wstring_view   = basic_string_view<wchar_t>;
using u16string_view = basic_string_view<char16>;
using u32string_view = basic_string_view<char32>;


inline namespace literals {
inline namespace string_view_literals {

constexpr auto operator"" _sv(char const* const s,
                              std::size_t const len) noexcept
{ return string_view{s, len}; }

constexpr auto operator"" _sv(wchar_t const* const s,
                              std::size_t    const len) noexcept
{ return wstring_view{s, len}; }

constexpr auto operator"" _sv(char16 const* const s,
                              std::size_t   const len) noexcept
{ return u16string_view{s, len}; }

constexpr auto operator"" _sv(char32 const* const s,
                              std::size_t   const len) noexcept
{ return u32string_view{s, len}; }

}}}   // inline namespace amp::literals::string_view_literals


#endif  // AMP_INCLUDED_4E72C427_2A3B_4ADB_96F5_018AF99C7F2E

