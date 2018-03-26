////////////////////////////////////////////////////////////////////////////////
//
// amp/string.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_37E23319_5D99_4157_BF42_B29E92415B31
#define AMP_INCLUDED_37E23319_5D99_4157_BF42_B29E92415B31


#include <amp/aux/operators.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string_view>

#if !__has_builtin(__builtin_strcasecmp) \
 || !__has_builtin(__builtin_strncasecmp)
# if defined(AMP_HAS_POSIX)
#  include <strings.h>
# elif defined(_MSC_VER)
#  include <string.h>
# else
#  include <cctype>
# endif
#endif


namespace amp {

AMP_READONLY
AMP_INLINE int stricmp(char const* s1, char const* s2) noexcept
{
#if __has_builtin(__builtin_strcasecmp)
    return __builtin_strcasecmp(s1, s2);
#elif defined(AMP_HAS_POSIX)
    return ::strcasecmp(s1, s2);
#elif defined(_MSC_VER)
    return ::_stricmp(s1, s2);
#else
    int c1, c2;
    do {
        c1 = std::tolower(static_cast<uchar>(*s1++));
        c2 = std::tolower(static_cast<uchar>(*s2++));
    }
    while (c1 != 0 && c1 == c2);
    return c1 - c2;
#endif
}

AMP_READONLY
AMP_INLINE int stricmp(char const* s1, char const* s2, std::size_t n) noexcept
{
#if __has_builtin(__builtin_strncasecmp)
    return __builtin_strncasecmp(s1, s2, n);
#elif defined(AMP_HAS_POSIX)
    return ::strncasecmp(s1, s2, n);
#elif defined(_MSC_VER)
    return ::_strnicmp(s1, s2, n);
#else
    if (n == 0) {
        return 0;
    }

    int c1, c2;
    do {
        c1 = static_cast<uchar>(std::tolower(*s1++));
        c2 = static_cast<uchar>(std::tolower(*s2++));
    }
    while (c1 != 0 && c1 == c2 && --n != 0);
    return c1 - c2;
#endif
}

AMP_READONLY
AMP_INLINE int stricmp(char const* const s1, std::size_t const n1,
                       char const* const s2, std::size_t const n2) noexcept
{
    auto ret = stricmp(s1, s2, std::min(n1, n2));
    if (ret == 0) {
        ret = int{n1 > n2} - int{n1 < n2};
    }
    return ret;
}

AMP_READONLY
AMP_INLINE int stricmp(std::string_view const s1,
                       std::string_view const s2) noexcept
{
    return stricmp(s1.data(), s1.size(),
                   s2.data(), s2.size());
}

AMP_READONLY
AMP_INLINE int stricmp(std::string_view const s1,
                       char const* const s2) noexcept
{
    return stricmp(s1.data(), s2, s1.size());
}

AMP_READONLY
AMP_INLINE int stricmp(char const* const s1,
                       std::string_view const s2) noexcept
{
    return stricmp(s1, s2.data(), s2.size());
}


AMP_READONLY
AMP_INLINE bool stricmpeq(char const* const s1,
                          char const* const s2) noexcept
{
    return (stricmp(s1, s2) == 0);
}

AMP_READONLY
AMP_INLINE bool stricmpeq(char const* const s1,
                          char const* const s2,
                          std::size_t const n) noexcept
{
    return (stricmp(s1, s2, n) == 0);
}

AMP_READONLY
AMP_INLINE bool stricmpeq(char const* const s1, std::size_t const n1,
                          char const* const s2, std::size_t const n2) noexcept
{
    return (n1 == n2) && stricmpeq(s1, s2, n2);
}

AMP_READONLY
AMP_INLINE bool stricmpeq(std::string_view const s1,
                          std::string_view const s2) noexcept
{
    return stricmpeq(s1.data(), s1.size(),
                     s2.data(), s2.size());
}

AMP_READONLY
AMP_INLINE bool stricmpeq(std::string_view const s1,
                          char const* const s2) noexcept
{
    return (stricmp(s1, s2) == 0);
}

AMP_READONLY
AMP_INLINE bool stricmpeq(char const* const s1,
                          std::string_view const s2) noexcept
{
    return (stricmp(s1, s2) == 0);
}


struct stricmp_less
{
    AMP_READONLY
    AMP_INLINE bool operator()(char const* const x,
                               char const* const y) const noexcept
    { return (stricmp(x, y) < 0); }

    AMP_READONLY
    AMP_INLINE bool operator()(char const* const x,
                               std::string_view const y) const noexcept
    { return (stricmp(x, y) < 0); }

    AMP_READONLY
    AMP_INLINE bool operator()(std::string_view const x,
                               char const* const y) const noexcept
    { return (stricmp(x, y) < 0); }

    AMP_READONLY
    AMP_INLINE bool operator()(std::string_view const x,
                               std::string_view const y) const noexcept
    { return (stricmp(x, y) < 0); }
};


namespace aux {

template<typename Delim>
class token_iterator_ :
    private equality_comparable<token_iterator_<Delim>>
{
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = std::string_view;
    using reference         = std::string_view const&;
    using pointer           = std::string_view const*;

    token_iterator_() = default;

    token_iterator_(std::string_view const s, Delim const d) noexcept :
        input_(s),
        delim_(d)
    {
        ++(*this);
    }

    token_iterator_& operator++() noexcept
    {
        auto const end = input_.find_first_not_of(delim_, token_.size());
        input_.remove_prefix(std::min(end, input_.size()));
        token_ = input_.substr(0, input_.find_first_of(delim_));
        return *this;
    }

    token_iterator_ operator++(int) noexcept
    {
        auto tmp = *this;
        return (++(*this), tmp);
    }

    reference operator*() const noexcept
    { return token_; }

    pointer operator->() const noexcept
    { return &token_; }

private:
    friend bool operator==(token_iterator_ const& x,
                           token_iterator_ const& y) noexcept
    { return (x.input_ == y.input_); }

    std::string_view input_;
    std::string_view token_;
    Delim delim_;
};

}     // namespace aux

template<typename Delim>
inline auto tokenize(std::string_view const str, Delim const delim) noexcept
{
    return make_range(aux::token_iterator_<Delim>{str, delim},
                      aux::token_iterator_<Delim>{});
}

}     // namespace amp


#endif  // AMP_INCLUDED_37E23319_5D99_4157_BF42_B29E92415B31

