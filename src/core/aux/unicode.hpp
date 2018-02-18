////////////////////////////////////////////////////////////////////////////////
//
// core/aux/unicode.hpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/bitops.hpp>
#include <amp/error.hpp>
#include <amp/io/memory.hpp>
#include <amp/io/reader.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include "core/cpu.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

#if defined(AMP_HAS_SSE2)
# include <emmintrin.h>
#endif


namespace amp {
namespace unicode {
namespace {

constexpr auto replacement = u'\ufffd';
constexpr auto max_bmp     = u'\uffff';
constexpr auto max_valid   = U'\U0010ffff';

constexpr std::array<uint8, 128> utf8_sequence_length {{
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3, 4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,
}};

constexpr std::array<char, 4> utf8_first_byte_mark {{
    '\x00', '\xc0', '\xe0', '\xf0',
}};

constexpr std::array<char16, 32> cp1252_table {{
    u'\u20ac', u'\ufffd', u'\u201a', u'\u0192',
    u'\u201e', u'\u2026', u'\u2020', u'\u2021',
    u'\u02c6', u'\u2030', u'\u0160', u'\u2039',
    u'\u0152', u'\ufffd', u'\u017d', u'\ufffd',
    u'\ufffd', u'\u2018', u'\u2019', u'\u201c',
    u'\u201d', u'\u2022', u'\u2013', u'\u2014',
    u'\u02dc', u'\u2122', u'\u0161', u'\u203a',
    u'\u0153', u'\ufffd', u'\u017e', u'\u0178',
}};


constexpr char32 get_supplementary(char32 const hs, char32 const ls) noexcept
{ return ((hs - U'\xd800') << 10) + (ls - U'\xdc00') + U'\x10000'; }

constexpr bool is_surrogate(char32 const c) noexcept
{ return (c & U'\xfffff800') == U'\xd800'; }

constexpr bool is_high_surrogate(char32 const c) noexcept
{ return (c & U'\xfffffc00') == U'\xd800'; }

constexpr bool is_low_surrogate(char32 const c) noexcept
{ return (c & U'\xfffffc00') == U'\xdc00'; }

constexpr bool is_valid_code_point(char32 const c) noexcept
{ return (c <= unicode::max_valid) && !unicode::is_surrogate(c); }

constexpr bool is_utf8_continuation_byte(char const c) noexcept
{ return (c & '\xc0') == '\x80'; }

constexpr std::size_t utf8_encoded_size(char32 const c) noexcept
{
    return (c <= U'\u007f') ? 1
         : (c <= U'\u07ff') ? 2
         : (c <= U'\uffff') ? 3
         :                    4;
}


inline auto is_valid_ascii_until(char const* s, char const* const end) noexcept
{
#if defined(AMP_HAS_SSE2)
    for (; (end - s) >= 16; s += 16) {
        auto v = _mm_loadu_si128(reinterpret_cast<__m128i const*>(s));
        auto n = _mm_movemask_epi8(v);
        if (n != 0) {
            return s + tzcnt(n);
        }
    }
#endif
    for (; s != end && !(*s & '\x80'); ++s) {}
    return s;
}

inline bool check_utf8_sequence(char const*& s, char const* const end) noexcept
{
    AMP_ASSERT(s != end);
    AMP_ASSERT(*s & '\x80');

    auto const p = reinterpret_cast<uchar const*>(s);
    auto const bytes = utf8_sequence_length[*p & 0x7f];
    if (bytes == 0 || bytes > (end - s)) {
        return false;
    }

    switch (bytes) {
    case 4:
        if ((p[3] & 0xc0) != 0x80) { return false; }
        [[fallthrough]];
    case 3:
        if ((p[2] & 0xc0) != 0x80) { return false; }
        [[fallthrough]];
    case 2:
        if ((p[1] & 0xc0) != 0x80) { return false; }

        switch (p[0]) {
        case 0xe0: if (p[1] < 0xa0) { return false; } break;
        case 0xed: if (p[1] > 0x9f) { return false; } break;
        case 0xf0: if (p[1] < 0x90) { return false; } break;
        case 0xf4: if (p[1] > 0x8f) { return false; } break;
        }
        s += bytes;
        return true;
    }
    AMP_UNREACHABLE();
}

inline void encode_utf8(char32 ch, char*& s) noexcept
{
    AMP_ASSERT(unicode::is_valid_code_point(ch));

    switch (auto const bytes = utf8_encoded_size(ch)) {
    case 4:
        s[3] = static_cast<char>((ch | 0x80) & 0xbf);
        ch >>= 6;
        [[fallthrough]];
    case 3:
        s[2] = static_cast<char>((ch | 0x80) & 0xbf);
        ch >>= 6;
        [[fallthrough]];
    case 2:
        s[1] = static_cast<char>((ch | 0x80) & 0xbf);
        ch >>= 6;
        [[fallthrough]];
    case 1:
        s[0] = static_cast<char>(ch) | utf8_first_byte_mark[bytes - 1];
        s += bytes;
        break;
    default:
        AMP_UNREACHABLE();
    }
}


template<string_encoding>
struct to_utf8;

template<>
struct to_utf8<string_encoding::cp1252>
{
    static constexpr auto stride = sizeof(uchar);

    static char32 next(char const*& s, char const*, bool const lossy)
    {
        char32 c{static_cast<uchar>(*s++)};
        if (c >= U'\x80' && c <= U'\x9f') {
            c = cp1252_table[c - U'\x80'];
            if (c == unicode::replacement && !lossy) {
                raise(errc::invalid_unicode);
            }
        }
        return c;
    }
};

template<endian E>
struct utf16_to_utf8
{
    static constexpr auto stride = sizeof(char16);

    static char32 next(char const*& s, char const* const end, bool const lossy)
    {
        auto const c1 = io::load<char16,E>(s);
        s += sizeof(char16);
        if (AMP_LIKELY(!unicode::is_surrogate(c1))) {
            return c1;
        }

        if (unicode::is_high_surrogate(c1) && (s != end)) {
            auto const c2 = io::load<char16,E>(s);
            if (unicode::is_low_surrogate(c2)) {
                s += sizeof(char16);
                return unicode::get_supplementary(c1, c2);
            }
        }
        if (lossy) {
            return unicode::replacement;
        }
        raise(errc::invalid_unicode);
    }
};

template<endian E>
struct utf32_to_utf8
{
    static constexpr auto stride = sizeof(char32);

    static char32 next(char const*& s, char const*, bool const lossy)
    {
        auto c = io::load<char32,E>(s);
        s += sizeof(char32);
        if (AMP_UNLIKELY(!unicode::is_valid_code_point(c))) {
            if (!lossy) {
                raise(errc::invalid_unicode);
            }
            c = unicode::replacement;
        }
        return c;
    }
};

template<> struct to_utf8<string_encoding::utf16be> : utf16_to_utf8<BE> {};
template<> struct to_utf8<string_encoding::utf16le> : utf16_to_utf8<LE> {};
template<> struct to_utf8<string_encoding::utf32be> : utf32_to_utf8<BE> {};
template<> struct to_utf8<string_encoding::utf32le> : utf32_to_utf8<LE> {};


template<string_encoding Enc>
auto utf8_conv(char const* src, std::size_t const n, bool const lossy)
{
    auto bytes = 0_sz;
    for (auto const end = src + (n * to_utf8<Enc>::stride); src != end; ) {
        auto const c = to_utf8<Enc>::next(src, end, lossy);
        bytes += utf8_encoded_size(c);
    }
    return bytes;
}

template<string_encoding Enc>
auto utf8_conv(char const* src, std::size_t const n, bool, char* dst) noexcept
{
    for (auto const end = src + (n * to_utf8<Enc>::stride); src != end; ) {
        auto const c = to_utf8<Enc>::next(src, end, true);
        encode_utf8(c, dst);
    }
}


template<>
auto utf8_conv<string_encoding::utf8>(char const* src, std::size_t const n,
                                      bool const lossy)
{
    if (!lossy)  {
        if (!is_valid_utf8(src, n)) {
            raise(errc::invalid_unicode);
        }
        return n;
    }

    auto bytes = 0_sz;
    for (auto const end = src + n; src != end; ) {
        auto const until = is_valid_utf8_until(src, end);
        bytes += static_cast<std::size_t>(until - src);

        if ((src = until) != end) {
            while ((++src != end) && is_utf8_continuation_byte(*src)) {}
            bytes += utf8_encoded_size(unicode::replacement);
        }
    }
    return bytes;
}

template<>
auto utf8_conv<string_encoding::utf8>(char const* src, std::size_t const n,
                                      bool const lossy, char* dst) noexcept
{
    if (!lossy) {
        std::copy_n(src, n, dst);
        return;
    }

    for (auto const end = src + n; src != end; ) {
        auto const until = is_valid_utf8_until(src, end);
        dst = std::copy(src, until, dst);

        if ((src = until) != end) {
            while ((++src != end) && is_utf8_continuation_byte(*src)) {}
            encode_utf8(unicode::replacement, dst);
        }
    }
}


template<typename Char>
inline endian get_byte_order(char const*& src, std::size_t& len) noexcept
{
    auto const bom = io::load<Char>(src);
    if (bom == u'\ufeff' || bom == u'\ufffe') {
        src += sizeof(Char);
        len -= 1;
        if (bom == u'\ufffe') {
            return (endian::host == BE) ? LE : BE;
        }
    }
    return endian::host;
}

}}}   // namespace amp::unicode::<unnamed>

