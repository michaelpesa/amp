////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/wave.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_66CF8036_DB7E_4C74_8834_847433E3B6B9
#define AMP_INCLUDED_66CF8036_DB7E_4C74_8834_847433E3B6B9


#include <amp/audio/format.hpp>
#include <amp/error.hpp>
#include <amp/io/reader.hpp>

#include <cstddef>
#include <cstring>


namespace amp {
namespace wave {

audio::codec_format parse_format(io::reader);


struct guid
{
    uint8 data[16];
};

inline bool operator==(guid const& x, guid const& y) noexcept
{ return (std::memcmp(x.data, y.data, sizeof(x.data)) == 0); }

inline bool operator!=(guid const& x, guid const& y) noexcept
{ return !(x == y); }


namespace aux {

struct guid_parser_
{
private:
    static constexpr int xdigit_(char const c)
    {
        if (c >= '0' && c <= '9') { return c - '0'; }
        if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
        if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
        raise(errc::invalid_argument);
    }

    static constexpr uint8 byte_(char const* const s)
    { return static_cast<uint8>((xdigit_(s[0]) << 4) | xdigit_(s[1])); }

public:
    constexpr guid operator()(char const* const s, std::size_t const len) const
    {
        if (len != 36) {
            raise(errc::invalid_argument);
        }
        if (s[8] != '-' || s[13] != '-' || s[18] != '-' || s[23] != '-') {
            raise(errc::invalid_argument);
        }

        return {{
            byte_(s +  0), byte_(s +  2), byte_(s +  4), byte_(s +  6),
            byte_(s +  9), byte_(s + 11), byte_(s + 14), byte_(s + 16),
            byte_(s + 19), byte_(s + 21), byte_(s + 24), byte_(s + 26),
            byte_(s + 28), byte_(s + 30), byte_(s + 32), byte_(s + 34),
        }};
    }
};

}     // namespace aux


inline namespace literals {

constexpr guid operator"" _guid(char const* const s, std::size_t const len)
{ return aux::guid_parser_{}(s, len); }

}}}   // namespace amp::wave::literals


#endif  // AMP_INCLUDED_66CF8036_DB7E_4C74_8834_847433E3B6B9

