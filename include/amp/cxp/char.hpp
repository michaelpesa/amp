////////////////////////////////////////////////////////////////////////////////
//
// amp/cxp/char.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_58899DA7_4EBB_453B_BFF7_BE8FC209C5AD
#define AMP_INCLUDED_58899DA7_4EBB_453B_BFF7_BE8FC209C5AD


#include <amp/stddef.hpp>


namespace amp {
namespace cxp {

AMP_INLINE constexpr char toupper(char const c) noexcept
{ return (c >= 'a' && c <= 'z') ? (c - ('a' - 'A')) : c; }

AMP_INLINE constexpr char tolower(char const c) noexcept
{ return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c; }


AMP_INLINE constexpr bool isprint(char const c) noexcept
{ return (c >= '\x20' && c <= '\x7e'); }

AMP_INLINE constexpr bool isdigit(char const c) noexcept
{ return (c >= '0' && c <= '9'); }

AMP_INLINE constexpr bool isupper(char const c) noexcept
{ return (c >= 'A' && c <= 'Z'); }

AMP_INLINE constexpr bool islower(char const c) noexcept
{ return (c >= 'a' && c <= 'z'); }

AMP_INLINE constexpr bool isalpha(char const c) noexcept
{ return cxp::isupper(c) || cxp::islower(c); }

AMP_INLINE constexpr bool isalnum(char const c) noexcept
{ return cxp::isalpha(c) || cxp::isdigit(c); }

AMP_INLINE constexpr bool isxdigit(char const c) noexcept
{ return cxp::isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); }

}}    // namespace amp::cxp


#endif  // AMP_INCLUDED_58899DA7_4EBB_453B_BFF7_BE8FC209C5AD

