////////////////////////////////////////////////////////////////////////////////
//
// amp/net/endian.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_A81480C9_9151_41FA_986A_83231F4C0A01
#define AMP_INCLUDED_A81480C9_9151_41FA_986A_83231F4C0A01


#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>
#include <amp/utility.hpp>

#include <cstring>


// All architectures currently supported by Windows are little-endian.
#if defined(_WIN32)
# define AMP_LITTLE_ENDIAN 1

#elif defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
# define AMP_LITTLE_ENDIAN 1
#elif !defined(__LITTLE_ENDIAN__) && defined(__BIG_ENDIAN__)
# define AMP_BIG_ENDIAN 1

#elif defined(__BYTE_ORDER__)

# if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#  define AMP_LITTLE_ENDIAN 1
# elif (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#  define AMP_BIG_ENDIAN 1
# endif

#elif defined(__linux__) || defined(__GLIBC__)

# include <endian.h>
# if (__BYTE_ORDER == __LITTLE_ENDIAN)
#  define AMP_LITTLE_ENDIAN 1
# elif (__BYTE_ORDER == __BIG_ENDIAN)
#  define AMP_BIG_ENDIAN 1
# endif

#elif defined(__FreeBSD__) || defined(__DragonFly__) \
   || defined(__NetBSD__)  || defined(__minix)

# include <sys/endian.h>
# if (_BYTE_ORDER == _LITTLE_ENDIAN)
#  define AMP_LITTLE_ENDIAN 1
# elif (_BYTE_ORDER == _BIG_ENDIAN)
#  define AMP_BIG_ENDIAN 1
# endif

#elif defined(__OpenBSD__) || defined(__Bitrig__)

# include <machine/endian.h>
# if (_BYTE_ORDER == _LITTLE_ENDIAN)
#  define AMP_LITTLE_ENDIAN 1
# elif (_BYTE_ORDER == _BIG_ENDIAN)
#  define AMP_BIG_ENDIAN 1
# endif

#elif defined(_AIX)

# include <sys/machine.h>
# if (BYTE_ORDER == LITTLE_ENDIAN)
#  define AMP_LITTLE_ENDIAN 1
# elif (BYTE_ORDER == BIG_ENDIAN)
#  define AMP_BIG_ENDIAN 1
# endif

#elif defined(__sun)

# include <sys/isa_defs.h>
# if defined(_LITTLE_ENDIAN)
#  define AMP_LITTLE_ENDIAN 1
# elif defined(_BIG_ENDIAN)
#  define AMP_BIG_ENDIAN 1
# endif

#elif defined(__ARMEL__) || defined(__THUMBEL__) || defined(__AARCH64EL__) \
   || defined(__MIPSEL)  || defined(__MIPSEL__)  || defined(_MIPSEL)
# define AMP_LITTLE_ENDIAN 1
#elif defined(__ARMEB__) || defined(__THUMBEB__) || defined(__AARCH64EB__) \
   || defined(__MIPSEB)  || defined(__MIPSEB__)  || defined(_MIPSEB)
# define AMP_BIG_ENDIAN 1

// Fall back to platform-based endianness detection. Some notes:
//   - Blackfin, x86(_64), Tile, and VAX are always little-endian.
//   - Alpha is little-endian except on the Cray T3E.
//   - Itanium is little-endian except on HP-UX.
//   - PA-RISC, System/390, z/Architecture are always big-endian.
//   - HP-UX is big endian on all supported architectures.
#elif defined(__zarch__) || defined(__s390x__) || defined(__SYSC_ZARCH__) \
   || defined(__s390__)  || defined(__370__)   || defined(__THW_370__) \
   || defined(__hppa)    || defined(__hppa__)  || defined(__HPPA__) \
   || defined(__hpux)    || defined(__sparcv8) || defined(_CRAYT3E)
# define AMP_BIG_ENDIAN 1
#elif defined(__alpha) || defined(__alpha__) || defined(__vax__) \
   || defined(__i386)  || defined(__i386__)  || defined(__x86_64__) \
   || defined(__ia64)  || defined(__ia64__)  || defined(__itanium__) \
   || defined(__bfin)  || defined(__BFIN__)  || defined(__tile__)
# define AMP_LITTLE_ENDIAN 1
#endif


#ifndef AMP_BIG_ENDIAN
#define AMP_BIG_ENDIAN 0
#endif
#ifndef AMP_LITTLE_ENDIAN
#define AMP_LITTLE_ENDIAN 0
#endif

#if (!AMP_BIG_ENDIAN && !AMP_LITTLE_ENDIAN)
# error "unrecognized and/or unsupported byte order"
#elif (AMP_BIG_ENDIAN && AMP_LITTLE_ENDIAN)
# error "internal error in endianness auto detection"
#endif


namespace amp {

enum class endian {
    little = AMP_LITTLE_ENDIAN,
    big    = AMP_BIG_ENDIAN,
    host   = 1,
};

constexpr auto BE = endian::big;
constexpr auto LE = endian::little;


namespace net {
namespace aux {

template<typename T>
AMP_INLINE constexpr auto byte_swap_(T const x) noexcept ->
    enable_if_t<(sizeof(T) == 1), T>
{
    return x;
}

template<typename T>
AMP_INLINE constexpr auto byte_swap_(T const x) noexcept ->
    enable_if_t<(sizeof(T) == 2), T>
{
#if __has_builtin(__builtin_bswap16) || AMP_GCC_PREREQ(4, 8)
    return __builtin_bswap16(x);
#else
    return static_cast<T>((x & 0xff00) >> 8)
         | static_cast<T>((x & 0x00ff) << 8);
#endif
}

template<typename T>
AMP_INLINE constexpr auto byte_swap_(T const x) noexcept ->
    enable_if_t<(sizeof(T) == 4), T>
{
#if __has_builtin(__builtin_bswap32) || AMP_GCC_PREREQ(4, 3)
    return __builtin_bswap32(x);
#else
    return ((x & 0xff000000) >> 24)
         | ((x & 0x00ff0000) >>  8)
         | ((x & 0x0000ff00) <<  8)
         | ((x & 0x000000ff) << 24);
#endif
}

template<typename T>
AMP_INLINE constexpr auto byte_swap_(T const x) noexcept ->
    enable_if_t<(sizeof(T) == 8), T>
{
#if __has_builtin(__builtin_bswap64) || AMP_GCC_PREREQ(4, 3)
    return __builtin_bswap64(x);
#else
    return ((x & 0xff00000000000000) >> 56)
         | ((x & 0x00ff000000000000) >> 40)
         | ((x & 0x0000ff0000000000) >> 24)
         | ((x & 0x000000ff00000000) >>  8)
         | ((x & 0x00000000ff000000) <<  8)
         | ((x & 0x0000000000ff0000) << 24)
         | ((x & 0x000000000000ff00) << 40)
         | ((x & 0x00000000000000ff) << 56);
#endif
}

}     // namespace aux


template<typename T>
AMP_INLINE constexpr auto byte_swap(T const x) noexcept ->
    enable_if_t<is_integral_v<T>, T>
{
    return static_cast<T>(aux::byte_swap_(as_unsigned(x)));
}

template<typename T>
AMP_INLINE constexpr auto byte_swap(T const x) noexcept ->
    enable_if_t<is_enum_v<T>, T>
{
    return static_cast<T>(byte_swap(as_underlying(x)));
}

template<typename T>
AMP_INLINE auto byte_swap(T t) noexcept ->
    enable_if_t<is_floating_point_v<T>, T>
{
    using U = conditional_t<(sizeof(T) == sizeof(uint32)), uint32,
              conditional_t<(sizeof(T) == sizeof(uint64)), uint64, void>>;

    U u;
    std::memcpy(&u, &t, sizeof(T));
    u = byte_swap(u);
    std::memcpy(&t, &u, sizeof(T));
    return t;
}


namespace aux {

template<endian E>
struct to_host_
{
    template<typename T>
    AMP_INLINE constexpr T operator()(T const& t) const noexcept
    { return byte_swap(t); }
};

template<>
struct to_host_<endian::host>
{
    template<typename T>
    AMP_INLINE constexpr T operator()(T const& t) const noexcept
    { return t; }
};

}     // namespace aux

template<endian E>
constexpr aux::to_host_<E> to_host{};

}}    // namespace amp::net


#endif  // AMP_INCLUDED_A81480C9_9151_41FA_986A_83231F4C0A01

