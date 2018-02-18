////////////////////////////////////////////////////////////////////////////////
//
// amp/cxp/string.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_3F94D326_0C3F_494D_AF0E_52B3415634CB
#define AMP_INCLUDED_3F94D326_0C3F_494D_AF0E_52B3415634CB


#include <amp/stddef.hpp>

#include <cstddef>


namespace amp {
namespace cxp {

template<typename Char>
constexpr std::size_t strlen(Char const* const s) noexcept
{
    auto n = 0_sz;
    for (; s[n] != '\0'; ++n) {}
    return n;
}

template<typename Char>
constexpr std::size_t strlen(Char const* const s,
                             std::size_t const limit) noexcept
{
    auto n = 0_sz;
    for (; n != limit && s[n] != '\0'; ++n) {}
    return n;
}

#if __has_builtin(__builtin_strlen) || AMP_GCC_PREREQ(4, 0)
template<> constexpr std::size_t strlen<char>(char const* const s) noexcept
{ return __builtin_strlen(s); }
#endif

static_assert(cxp::strlen( "nineteen characters")     == 19, "");
static_assert(cxp::strlen(u"nineteen characters", 29) == 19, "");
static_assert(cxp::strlen(U"nineteen characters", 10) == 10, "");
static_assert(cxp::strlen(L"nineteen characters",  0) ==  0, "");


constexpr int strcmp(char const* s1, char const* s2) noexcept
{
    uchar c1{}, c2{};
    do {
        c1 = static_cast<uchar>(*s1++);
        c2 = static_cast<uchar>(*s2++);
    }
    while (c1 != 0 && c1 == c2);
    return c1 - c2;
}

constexpr int strcmp(char const* s1, std::size_t n1,
                     char const* s2, std::size_t n2) noexcept
{
    for (; n1 != 0 && n2 != 0; --n1, --n2) {
        auto const c1 = *s1++;
        auto const c2 = *s2++;
        if (c1 != c2) {
            return int{c1 > c2} - int{c1 < c2};
        }
    }
    return int{n1 > n2} - int{n1 < n2};
}

struct strcmp_less
{
    constexpr bool operator()(char const* const s1,
                              char const* const s2) const noexcept
    {
        return (cxp::strcmp(s1, s2) < 0);
    }

    template<typename S1, typename S2>
    constexpr bool operator()(S1 const& s1, S2 const& s2) const noexcept
    { return (cxp::strcmp(s1.data(), s1.size(), s2.data(), s2.size()) < 0); }
};

}}    // namespace amp::cxp


#endif  // AMP_INCLUDED_3F94D326_0C3F_494D_AF0E_52B3415634CB

