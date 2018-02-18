////////////////////////////////////////////////////////////////////////////////
//
// core/base64.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_36D54937_85B9_4B1F_B2EE_45737A51065E
#define AMP_INCLUDED_36D54937_85B9_4B1F_B2EE_45737A51065E


#include <amp/stddef.hpp>
#include <amp/string_view.hpp>

#include <cstddef>


namespace amp {
namespace base64 {

constexpr auto encoded_size(std::size_t const n) noexcept
{ return ((n + 2) / 3) * 4; }

constexpr auto max_decoded_size(std::size_t const n) noexcept
{ return ((n + 3) / 4) * 3; }

constexpr auto decoded_size(char const* const s, std::size_t const n) noexcept
{
    auto ret = (n / 4) * 3;
    if ((n & 3) != 0) {
        ret += (n & 3) - 1;
    }
    else if (n >= 4) {
        ret -= (s[n - 1] == '=');
        ret -= (s[n - 2] == '=');
    }
    return ret;
}

constexpr auto decoded_size(string_view const s) noexcept
{ return base64::decoded_size(s.data(), s.size()); }


class stream
{
public:
    constexpr stream() noexcept = default;

    AMP_EXPORT std::size_t decode(void const*, std::size_t, void*);
    AMP_EXPORT std::size_t encode(void const*, std::size_t, void*) noexcept;
    AMP_EXPORT std::size_t encode_finish(void*) noexcept;

private:
    uint8 state_{};
    uint8 carry_{};
};


AMP_INLINE std::size_t decode(void const* const src, std::size_t const len,
                              void* const dst)
{
    return base64::stream{}.decode(src, len, dst);
}

AMP_INLINE std::size_t encode(void const* const src, std::size_t const len,
                              void* const dst)
{
    base64::stream s;
    auto const ret = s.encode(src, len, dst);
    return ret + s.encode_finish(static_cast<uint8*>(dst) + ret);
}

}}    // namespace amp::base64


#endif  // AMP_INCLUDED_36D54937_85B9_4B1F_B2EE_45737A51065E

