////////////////////////////////////////////////////////////////////////////////
//
// core/uri.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/bitops.hpp>
#include <amp/cxp/char.hpp>
#include <amp/error.hpp>
#include <amp/net/uri.hpp>
#include <amp/numeric.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <string_view>


namespace amp {
namespace net {
namespace {

using namespace std::literals;


constexpr bool is_sub_delim(char const c) noexcept
{
    return c == '!' || c == '$' || c == '&' || c == '(' || c == ')' || c == '*'
        || c == '+' || c == ',' || c == ';' || c == '=' || c == '\'';
}

constexpr bool is_unreserved(char const c) noexcept
{ return cxp::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~'; }

constexpr bool is_pchar(char const c) noexcept
{ return is_unreserved(c) || is_sub_delim(c) || c == ':' || c == '@'; }


constexpr bool is_valid_scheme_char(char const c) noexcept
{ return cxp::isalnum(c) || c == '-' || c == '.' || c == '+'; }

constexpr bool is_valid_userinfo_char(char const c) noexcept
{ return is_unreserved(c) || is_sub_delim(c) || c == ':'; }

constexpr bool is_valid_host_char(char const c) noexcept
{ return is_unreserved(c) || is_sub_delim(c); }

constexpr bool is_valid_port_char(char const c) noexcept
{ return cxp::isdigit(c); }

constexpr bool is_valid_path_char(char const c) noexcept
{ return is_pchar(c) || c == '/'; }

constexpr bool is_valid_query_char(char const c) noexcept
{ return is_pchar(c) || c == '/' || c == '?'; }

constexpr bool is_valid_fragment_char(char const c) noexcept
{ return is_pchar(c) || c == '/' || c == '?'; }

constexpr bool is_valid_ipv6_char(char const c) noexcept
{ return cxp::isxdigit(c) || c == '[' || c == ']' || c == ':' || c == '.'; }


template<std::size_t... C>
constexpr auto gen_char_masks(std::index_sequence<C...>) noexcept
{
    return std::array<uint8, 256> {{
        ((uint{is_valid_scheme_char(C)}   << 0) |
         (uint{is_valid_userinfo_char(C)} << 1) |
         (uint{is_valid_host_char(C)}     << 2) |
         (uint{is_valid_port_char(C)}     << 3) |
         (uint{is_valid_path_char(C)}     << 4) |
         (uint{is_valid_query_char(C)}    << 5) |
         (uint{is_valid_fragment_char(C)} << 6) |
         (uint{is_valid_ipv6_char(C)}     << 7))...
    }};
}

constexpr auto char_masks = gen_char_masks(std::make_index_sequence<128>());

constexpr bool allowed(char const c, uint const mask) noexcept
{ return (char_masks[as_unsigned(c)] & mask); }


constexpr bool is_uri_escaped(char const* const first,
                              char const* const last) noexcept
{
    return ((last - first) >= 3)
        && (first[0] == '%')
        && cxp::isxdigit(first[1])
        && cxp::isxdigit(first[2]);
}

inline char uri_unescape(char hi, char lo) noexcept
{
    hi = (cxp::isdigit(hi) ? (hi - '0') : ((hi | 0x20) - 'a' + 10));
    lo = (cxp::isdigit(lo) ? (lo - '0') : ((lo | 0x20) - 'a' + 10));
    return static_cast<char>((hi << 4) | lo);
}

inline void uri_escape(char const c, char*& dst) noexcept
{
    static constexpr char const hex_digits[] {
        '0','1','2','3','4','5','6','7','8','9',
        'A','B','C','D','E','F',
    };

    *dst++ = '%';
    *dst++ = hex_digits[(c >> 4) & 0xf];
    *dst++ = hex_digits[(c >> 0) & 0xf];
}

inline bool starts_with(std::string_view const x,
                        std::string_view const y) noexcept
{
    return (y.size() <= x.size())
        && std::equal(y.begin(), y.end(), x.begin());
}


auto percent_encoded_size(std::string_view const src,
                          uri_part const part) noexcept
{
    auto const mask = 1U << as_underlying(part);
    auto size = 0_sz;
    for (auto const c : src) {
        size += (allowed(c, mask) ? 1_sz : 3_sz);
    }
    return size;
}

void percent_encode(std::string_view const src, char* dst,
                    uri_part const part) noexcept
{
    auto const mask = 1U << as_underlying(part);
    for (auto const c : src) {
        if (allowed(c, mask)) {
            *dst++ = c;
        }
        else {
            uri_escape(c, dst);
        }
    }
}

auto percent_decoded_size(std::string_view const src)
{
    auto size = 0_sz;
    auto const last = src.end();
    for (auto first = src.begin(); first != last; ) {
        auto const escape = std::find(first, last, '%');
        size += static_cast<std::size_t>(escape - first);

        if ((first = escape) != last) {
            if (AMP_UNLIKELY(!is_uri_escaped(first, last))) {
                raise(errc::invalid_argument, "invalid URI escape sequence");
            }
            first += 3;
            size  += 1;
        }
    }
    return size;
}

void percent_decode(std::string_view const src, char* dst) noexcept
{
    auto const last = src.end();
    for (auto first = src.begin(); first != last; ) {
        auto const escape = std::find(first, last, '%');
        dst = std::copy(first, escape, dst);

        if ((first = escape) != last) {
            AMP_ASSERT(is_uri_escaped(first, last));
            *dst++ = uri_unescape(first[1], first[2]);
            first += 3;
        }
    }
}


ref_ptr<uri_rep> allocate_uri(std::size_t const len)
{
    auto const rep_size = sizeof(uri_rep) + len + 1;
    auto const rep = static_cast<uri_rep*>(std::malloc(rep_size));
    if (AMP_UNLIKELY(rep == nullptr)) {
        raise_bad_alloc();
    }
    ::new(&rep->ref_count) decltype(rep->ref_count){1};
    ::new(&rep->length)    decltype(rep->length){numeric_cast<uint32>(len)};
    ::new(&rep->parts)     decltype(rep->parts){};
    ::new(rep->data())     char[len + 1];
    return ref_ptr<uri_rep>::consume(rep);
}

u8string merge_paths(uri const& base, uri const& rel)
{
    auto x = base.path();
    auto y = rel.path();

    if (x.empty()) {
        x = "/";
    }
    else {
        auto pos = x.find_last_of('/');
        if (pos != x.npos) {
            x.remove_suffix((x.size() - pos) - 1);
        }
    }

    u8string_buffer buf(x.size() + y.size(), uninitialized);
    std::copy_n(x.data(), x.size(), buf.data());
    std::copy_n(y.data(), y.size(), buf.data() + x.size());
    return buf.promote();
}

char* remove_dot_segments(char* const first, char* const last) noexcept
{
    auto dst = first;
    auto src = std::string_view{dst, as_unsigned(last - dst)};

    auto remove_last_segment = [&]() noexcept {
        while (dst != first) {
            if (*(--dst) == '/') {
                return;
            }
        }
    };

    while (!src.empty()) {
        if (src == "." || src == "..") {
            break;
        }
        if (src == "/.") {
            *dst++ = '/';
            break;
        }
        if (src == "/..") {
            remove_last_segment();
            *dst++ = '/';
            break;
        }

        if (starts_with(src, "../")) {
            src.remove_prefix(3);
        }
        else if (starts_with(src, "./")) {
            src.remove_prefix(2);
        }
        else if (starts_with(src, "/./")) {
            src.remove_prefix(2);
        }
        else if (starts_with(src, "//")) {
            src.remove_prefix(1);
        }
        else if (starts_with(src, "/../")) {
            src.remove_prefix(3);
            remove_last_segment();
        }
        else {
            auto const n = std::min(src.find('/', 1), src.size());
            dst = std::copy_n(src.begin(), n, dst);
            src.remove_prefix(n);
        }
    }
    return dst;
}

void copy_and_normalize(std::string_view const src, char*& dst,
                        uri_rep& rep, uri_part const part)
{
    AMP_ASSERT(!src.empty());
    auto const mask = (part == uri_part::host && src[0] == '[')
                    ? (1U << 7)
                    : (1U << as_underlying(part));

    auto const start = dst;
    auto const lower = (part == uri_part::scheme) || (part == uri_part::host);

    auto const last = src.end();
    for (auto first = src.begin(); first != last; ) {
        if (allowed(*first, mask)) {
            *dst++ = (lower ? cxp::tolower(*first) : *first);
            ++first;
        }
        else if (*first == '%') {
            if (AMP_UNLIKELY(!is_uri_escaped(first, last))) {
                raise(errc::invalid_argument, "invalid URI escape sequence");
            }

            auto const c = uri_unescape(first[1], first[2]);
            if (allowed(c, mask)) {
                *dst++ = (lower ? cxp::tolower(c) : c);
            }
            else {
                uri_escape(c, dst);
            }
            first += 3;
        }
        else {
            raise(errc::invalid_argument,
                  "URI contains unescaped reserved character '%c'", *first);
        }
    }

    if (part == uri_part::path && rep[uri_part::scheme].length != 0) {
        dst = remove_dot_segments(start, dst);
    }
    rep[part].offset = static_cast<uint32>(start - rep.data());
    rep[part].length = static_cast<uint32>(dst - start);
}

uri from_parts(std::string_view const scheme,
               std::string_view const userinfo,
               std::string_view const host,
               std::string_view const port,
               std::string_view const path,
               std::string_view const query,
               std::string_view const fragment)
{
    auto max_len = host.size();
    if (!scheme.empty())   { max_len += scheme.size() + 3; }
    if (!userinfo.empty()) { max_len += userinfo.size() + 1; }
    if (!port.empty())     { max_len += port.size() + 1; }
    if (!path.empty())     { max_len += path.size() + 1; }
    if (!query.empty())    { max_len += query.size() + 1; }
    if (!fragment.empty()) { max_len += fragment.size() + 1; }

    auto rep = net::allocate_uri(max_len);
    auto dst = rep->data();

    if (!scheme.empty()) {
        copy_and_normalize(scheme, dst, *rep, uri_part::scheme);
    }

    auto const has_authority = !userinfo.empty()
                            || !host.empty()
                            || !port.empty()
                            || (scheme == "file"sv);
    if (has_authority) {
        if (!scheme.empty()) {
            dst = std::copy_n("://", 3, dst);
        }
        if (!userinfo.empty()) {
            copy_and_normalize(userinfo, dst, *rep, uri_part::userinfo);
            *dst++ = '@';
        }
        if (!host.empty()) {
            copy_and_normalize(host, dst, *rep, uri_part::host);
        }
        if (!port.empty()) {
            *dst++ = ':';
            copy_and_normalize(port, dst, *rep, uri_part::port);
        }
    }
    else if (!scheme.empty()) {
        if (!path.empty() || !query.empty() || !fragment.empty()) {
            *dst++ = ':';
        }
    }

    if (!path.empty()) {
        auto const add_leading_slash = has_authority && (path[0] != '/');
        if (add_leading_slash) {
            *dst++ = '/';
        }
        copy_and_normalize(path, dst, *rep, uri_part::path);

        if (add_leading_slash) {
            auto&& part = (*rep)[uri_part::path];
            part.offset -= 1;
            part.length += 1;
        }
    }

    if (!query.empty()) {
        *dst++ = '?';
        copy_and_normalize(query, dst, *rep, uri_part::query);
    }
    if (!fragment.empty()) {
        *dst++ = '#';
        copy_and_normalize(fragment, dst, *rep, uri_part::fragment);
    }

    *dst = '\0';
    rep->length = static_cast<uint32>(dst - rep->data());
    return uri{std::move(rep)};
}

}     // namespace <unnamed>


void uri_rep::release() const noexcept
{
    auto remain = ref_count.fetch_sub(1, std::memory_order_release) - 1;
    if (!remain) {
        std::atomic_thread_fence(std::memory_order_acquire);
        std::free(const_cast<uri_rep*>(this));
    }
}

uri uri::resolve(uri const& base) const
{
    if (empty() || base.empty()) {
        return (empty() ? base : *this);
    }

    auto scheme   = this->scheme();
    auto userinfo = this->userinfo();
    auto host     = this->host();
    auto port     = this->port();
    auto path     = this->path();
    auto query    = this->query();
    auto fragment = this->fragment();

    u8string path_buf;
    if (scheme.empty()) {
        scheme = base.scheme();

        if (userinfo.empty() && host.empty() && port.empty()) {
            userinfo = base.userinfo();
            host     = base.host();
            port     = base.port();

            if (path.empty()) {
                path = base.path();
                if (query.empty()) {
                    query = base.query();
                }
            }
            else {
                if (path.front() != '/') {
                    path_buf = merge_paths(base, *this);
                    path = path_buf.to_string_view();
                }
            }
        }
    }
    return from_parts(scheme, userinfo, host, port, path, query, fragment);
}

uri uri::from_string(std::string_view const s)
{
    if (s.empty()) {
        return {};
    }

    auto make_part = [](char const* const b, char const* const e) noexcept {
        return std::string_view{b, static_cast<std::size_t>(e - b)};
    };

    std::string_view scheme, userinfo, host, port, path, query, fragment;

    auto src = s.begin();
    auto end = s.end();

    auto const fragment_begin = std::find(src, end, '#');
    if (fragment_begin != end) {
        fragment = make_part(fragment_begin + 1, end);
        end = fragment_begin;
    }

    auto const scheme_end = std::find(src, end, ':');
    if (scheme_end != end && cxp::isalpha(*src)) {
        if (std::all_of(src + 1, scheme_end,
                        [](char c) { return allowed(c, 1); })) {
            scheme = make_part(src, scheme_end);
            src = scheme_end + 1;
        }
    }

    auto const has_authority = ((end - src) >= 2)
                            && (src[0] == '/')
                            && (src[1] == '/');
    if (has_authority) {
        src += 2;

        auto const authority_end_delims = "/?#"sv;
        auto const authority_end = std::find_first_of(
            src, end,
            authority_end_delims.begin(),
            authority_end_delims.end());

        if (authority_end != src) {
            auto const userinfo_end = std::find(src, authority_end, '@');
            if (userinfo_end != authority_end) {
                userinfo = make_part(src, userinfo_end);
                src = userinfo_end + 1;
            }

            auto host_end = std::find(src, authority_end, ']');
            if (host_end != authority_end) {
                host_end += 1;
            }
            else {
                host_end = std::find(src, authority_end, ':');
            }

            if (host_end != src) {
                host = make_part(src, host_end);
                src = host_end;
            }
            if (src != authority_end && *src == ':') {
                port = make_part(src + 1, authority_end);
                src = authority_end;
            }
        }
    }

    auto const query_begin = std::find(src, end, '?');
    if (query_begin != end) {
        query = make_part(query_begin + 1, end);
        end = query_begin;
    }

    path = make_part(src, end);
    return from_parts(scheme, userinfo, host, port, path, query, fragment);
}

uri uri::from_file_path(std::string_view const path)
{
    if (path.empty()) {
        return {};
    }

    auto const is_absolute = (path[0] == '/');
    auto const enc_path_size = percent_encoded_size(path, uri_part::path);
    AMP_ASSERT(enc_path_size > 0);

    auto rep = allocate_uri(enc_path_size + (is_absolute ? 7 : 0));
    auto dst = rep->data();
    if (is_absolute) {
        dst = std::copy_n("file://", 7, dst);
        (*rep)[uri_part::scheme] = {0, 4};
        (*rep)[uri_part::path].offset = 7;
    }

    (*rep)[uri_part::path].length = static_cast<uint32>(enc_path_size);
    percent_encode(path, dst, uri_part::path);
    dst[enc_path_size] = '\0';
    return uri{std::move(rep)};
}

u8string uri::get_file_path() const
{
    auto const enc_path = path();

    u8string_buffer buf{percent_decoded_size(enc_path), uninitialized};
    percent_decode(enc_path, buf.data());
    return buf.promote();
}

}}    // namespace amp::net

