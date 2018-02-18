////////////////////////////////////////////////////////////////////////////////
//
// amp/aux/string_search.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_2F7A748E_04BF_4412_881E_A6E595F37D3F
#define AMP_INCLUDED_2F7A748E_04BF_4412_881E_A6E595F37D3F


#include <amp/stddef.hpp>

#include <algorithm>


namespace amp {
namespace aux {

template<typename Traits, typename Char, typename Size>
AMP_INLINE Size str_find(Char const* const p, Size const sz,
                         Char const* const s, Size pos,
                         Size const n) noexcept
{
    if (pos > sz || (sz - pos) < n) {
        return Size(-1);
    }
    if (n == 0) {
        return pos;
    }

    for (; pos <= (sz - n); ++pos) {
        if (Traits::compare(p + pos, s, n) == 0) {
            return pos;
        }
    }
    return Size(-1);
}

template<typename Traits, typename Char, typename Size>
AMP_INLINE Size str_find(Char const* const p, Size const sz,
                         Char const c, Size const pos) noexcept
{
    if (pos < sz) {
        auto const r = Traits::find(p + pos, sz - pos, c);
        if (r != nullptr) {
            return static_cast<Size>(r - p);
        }
    }
    return Size(-1);
}


template<typename Traits, typename Char, typename Size>
AMP_INLINE Size str_rfind(Char const* const p, Size const sz,
                          Char const* const s, Size pos,
                          Size const n) noexcept
{
    if (n <= sz) {
        pos = std::min(pos, sz - n);
        do {
            if (Traits::compare(p + pos, s, n) == 0) {
                return pos;
            }
        }
        while (pos-- != 0);
    }
    return Size(-1);
}

template<typename Traits, typename Char, typename Size>
AMP_INLINE Size str_rfind(Char const* const p, Size sz,
                          Char const c, Size pos) noexcept
{
    if (sz != 0) {
        if (pos >= sz) {
            pos = sz - 1;
        }

        for (++pos; pos-- != 0; ) {
            if (Traits::eq(p[pos], c)) {
                return pos;
            }
        }
    }
    return Size(-1);
}

template<typename Traits, typename Char, typename Size>
AMP_INLINE Size str_find_first_of(Char const* const p, Size const sz,
                                  Char const* const s, Size pos,
                                  Size const n) noexcept
{
    if (n != 0) {
        for (; pos < sz; ++pos) {
            if (Traits::find(s, n, p[pos])) {
                return pos;
            }
        }
    }
    return Size(-1);
}

template<typename Traits, typename Char, typename Size>
AMP_INLINE Size str_find_last_of(Char const* const p, Size const sz,
                                 Char const* const s, Size pos,
                                 Size const n) noexcept
{
    if (sz != 0 && n != 0) {
        if (pos >= sz) {
            pos = sz - 1;
        }

        do {
            if (Traits::find(s, n, p[pos])) {
                return pos;
            }
        }
        while (pos-- != 0);
    }
    return Size(-1);
}


template<typename Traits, typename Char, typename Size>
AMP_INLINE Size str_find_first_not_of(Char const* const p, Size const sz,
                                      Char const* const s, Size pos,
                                      Size const n) noexcept
{
    for (; pos < sz; ++pos) {
        if (!Traits::find(s, n, p[pos])) {
            return pos;
        }
    }
    return Size(-1);
}

template<typename Traits, typename Char, typename Size>
AMP_INLINE Size str_find_first_not_of(Char const* const p, Size const sz,
                                      Char const c, Size pos) noexcept
{
    for (; pos < sz; ++pos) {
        if (!Traits::eq(p[pos], c)) {
            return pos;
        }
    }
    return Size(-1);
}


template<typename Traits, typename Char, typename Size>
AMP_INLINE Size str_find_last_not_of(Char const* const p, Size const sz,
                                     Char const* const s, Size pos,
                                     Size const n) noexcept
{
    if (sz != 0) {
        if (pos >= sz) {
            pos = sz - 1;
        }

        do {
            if (!Traits::find(s, n, p[pos])) {
                return pos;
            }
        }
        while (pos-- != 0);
    }
    return Size(-1);
}

template<typename Traits, typename Char, typename Size>
AMP_INLINE Size str_find_last_not_of(Char const* const p, Size const sz,
                                     Char const c, Size pos) noexcept
{
    if (sz != 0) {
        if (pos >= sz) {
            pos = sz - 1;
        }

        do {
            if (!Traits::eq(p[pos], c)) {
                return pos;
            }
        }
        while (pos-- != 0);
    }
    return Size(-1);
}

}}    // namespace amp::aux


#endif  // AMP_INCLUDED_2F7A748E_04BF_4412_881E_A6E595F37D3F

