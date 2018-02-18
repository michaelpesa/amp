////////////////////////////////////////////////////////////////////////////////
//
// amp/net/uri.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_B6BA4030_B6EE_4EDD_B147_DE720C87C20C
#define AMP_INCLUDED_B6BA4030_B6EE_4EDD_B147_DE720C87C20C


#include <amp/aux/operators.hpp>
#include <amp/memory.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>
#include <amp/string_view.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include <atomic>
#include <cstddef>
#include <iterator>
#include <utility>


namespace amp {
namespace net {

enum class uri_part : uint8 {
    scheme   = 0,
    userinfo = 1,
    host     = 2,
    port     = 3,
    path     = 4,
    query    = 5,
    fragment = 6,
};


struct uri_rep
{
    mutable std::atomic<uint32> ref_count;
    uint32 length;

    struct {
        uint32 offset;
        uint32 length;
    }
    parts[7];

    AMP_EXPORT
    void release() const noexcept;

    AMP_INLINE constexpr auto& operator[](net::uri_part const p) noexcept
    { return parts[as_underlying(p)]; }

    AMP_INLINE constexpr auto& operator[](net::uri_part const p) const noexcept
    { return parts[as_underlying(p)]; }

    AMP_INLINE char* data() noexcept
    { return reinterpret_cast<char*>(this + 1); }

    AMP_INLINE char const* data() const noexcept
    { return reinterpret_cast<char const*>(this + 1); }

    AMP_INLINE void add_ref() const noexcept
    { ref_count.fetch_add(1, std::memory_order_relaxed); }

    AMP_INLINE uint32 use_count() const noexcept
    { return ref_count.load(std::memory_order_relaxed); }
};


class uri :
    private equality_comparable<uri>
{
public:
    using size_type              = uint32;
    using value_type             = char;
    using pointer                = char const*;
    using const_pointer          = char const*;
    using iterator               = char const*;
    using const_iterator         = char const*;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    uri() = default;
    uri(uri&&) = default;
    uri(uri const&) = default;
    uri& operator=(uri&&) & = default;
    uri& operator=(uri const&) & = default;

    explicit uri(ref_ptr<net::uri_rep> p) noexcept :
        rep{std::move(p)}
    {}

    void swap(uri& x) noexcept
    {
        using std::swap;
        swap(rep, x.rep);
    }

    bool empty() const noexcept
    { return (size() == 0); }

    size_type size() const noexcept
    { return rep ? rep->length : 0; }

    const_pointer data() const noexcept
    { return rep ? rep->data() : nullptr; }

    const_iterator begin() const noexcept
    { return cbegin(); }

    const_iterator end() const noexcept
    { return cend(); }

    const_iterator cbegin() const noexcept
    { return data(); }

    const_iterator cend() const noexcept
    { return data() + size(); }

    const_reverse_iterator rbegin() const noexcept
    { return crbegin(); }

    const_reverse_iterator rend() const noexcept
    { return crend(); }

    const_reverse_iterator crbegin() const noexcept
    { return const_reverse_iterator(cend()); }

    const_reverse_iterator crend() const noexcept
    { return const_reverse_iterator(cbegin()); }

    string_view scheme() const noexcept
    { return get_part_(uri_part::scheme); }

    string_view userinfo() const noexcept
    { return get_part_(uri_part::userinfo); }

    string_view host() const noexcept
    { return get_part_(uri_part::host); }

    string_view port() const noexcept
    { return get_part_(uri_part::port); }

    string_view query() const noexcept
    { return get_part_(uri_part::query); }

    string_view path() const noexcept
    { return get_part_(uri_part::path); }

    string_view fragment() const noexcept
    { return get_part_(uri_part::fragment); }

    uint32 use_count() const noexcept
    { return rep ? rep->use_count() : 0; }

    u8string to_u8string() const
    { return u8string(data(), size()); }

    void clear() noexcept
    { rep = nullptr; }

    AMP_EXPORT
    uri resolve(uri const&) const;

    AMP_EXPORT
    u8string get_file_path() const;

    AMP_EXPORT
    static uri from_string(string_view);

    AMP_EXPORT
    static uri from_file_path(string_view);

private:
    AMP_INLINE string_view get_part_(uri_part const part) const noexcept
    {
        if (rep) {
            return {rep->data() + (*rep)[part].offset, (*rep)[part].length};
        }
        return string_view{};
    }

    friend bool operator==(uri const& x, uri const& y) noexcept
    { return mem::equal(x.data(), x.size(), y.data(), y.size()); }

    ref_ptr<net::uri_rep const> rep;
};


inline void swap(net::uri& x, net::uri& y) noexcept
{
    x.swap(y);
}

inline u8string to_u8string(net::uri const& x)
{
    return x.to_u8string();
}

}     // namespace net


inline namespace literals {
inline namespace uri_literals {

inline auto operator"" _uri(char const* const s, std::size_t const len)
{
    return net::uri::from_string({s, len});
}

}}    // inline namespace literals::uri_literals

namespace net {

using namespace ::amp::uri_literals;

}}    // namespace amp::net


#endif  // AMP_INCLUDED_B6BA4030_B6EE_4EDD_B147_DE720C87C20C

