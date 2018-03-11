////////////////////////////////////////////////////////////////////////////////
//
// amp/io/memory.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_FB41D387_0C1D_42C0_A531_9540B9008F37
#define AMP_INCLUDED_FB41D387_0C1D_42C0_A531_9540B9008F37


#include <amp/bitops.hpp>
#include <amp/net/endian.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <cstddef>
#include <cstring>
#include <memory>


namespace amp {
namespace io {

// Clang warns that __attribute__((packed)) usage in this file are unnecessary,
// even though this is untrue and Clang will generate aligned load and store
// instructions without the attribute.
#if __has_warning("-Wpacked")
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wpacked"
#endif

template<typename T>
AMP_READONLY
AMP_INLINE T load(void const* const p) noexcept
{
    static_assert(is_trivially_copyable_v<T>, "");

#if __has_attribute(may_alias) || AMP_GCC_PREREQ(3, 3)
    struct Alias { T t; } __attribute__((may_alias, packed));
    return static_cast<Alias const*>(p)->t;
#else
    T v;
    std::memcpy(std::addressof(v), p, sizeof(T));
    return v;
#endif
}

template<typename T>
AMP_READONLY
AMP_INLINE T load_aligned(void const* const p) noexcept
{
    static_assert(is_trivially_copyable_v<T>, "");

    AMP_ASSERT(is_aligned(p, alignof(T)));
    AMP_ASSUME(is_aligned(p, alignof(T)));

#if __has_attribute(may_alias) || AMP_GCC_PREREQ(3, 3)
    struct Alias { T t; } __attribute__((may_alias));
    return static_cast<Alias const*>(p)->t;
#else
    T v;
    std::memcpy(std::addressof(v), p, sizeof(T));
    return v;
#endif
}


template<typename T>
AMP_INLINE void store(void* const AMP_RESTRICT p, T const& v) noexcept
{
    static_assert(is_trivially_copyable_v<T>, "");

#if __has_attribute(may_alias) || AMP_GCC_PREREQ(3, 3)
    struct Alias { T t; } __attribute__((may_alias, packed));
    static_cast<Alias*>(p)->t = v;
#else
    std::memcpy(p, std::addressof(v), sizeof(T));
#endif
}

template<typename T>
AMP_INLINE void store_aligned(void* const AMP_RESTRICT p, T const& v) noexcept
{
    static_assert(is_trivially_copyable_v<T>, "");

    AMP_ASSERT(is_aligned(p, alignof(T)));
    AMP_ASSUME(is_aligned(p, alignof(T)));

#if __has_attribute(may_alias) || AMP_GCC_PREREQ(3, 3)
    struct Alias { T t; } __attribute__((may_alias));
    static_cast<Alias*>(p)->t = v;
#else
    std::memcpy(p, std::addressof(v), sizeof(T));
#endif
}

#if __has_warning("-Wpacked")
# pragma clang diagnostic pop
#endif


template<typename T, endian E>
AMP_READONLY
AMP_INLINE T load(void const* const p) noexcept
{
    return net::to_host<E>(io::load<T>(p));
}

template<typename T, endian E>
AMP_READONLY
AMP_INLINE T load_aligned(void const* const p) noexcept
{
    return net::to_host<E>(io::load_aligned<T>(p));
}


template<endian E, typename T>
AMP_INLINE void store(void* const AMP_RESTRICT p, T const& v) noexcept
{
    io::store(p, net::to_host<E>(v));
}

template<endian E, typename T>
AMP_INLINE void store_aligned(void* const AMP_RESTRICT p, T const& v) noexcept
{
    io::store_aligned(p, net::to_host<E>(v));
}


namespace aux {

template<endian E, typename T>
AMP_INLINE auto load_n_(uchar const* const AMP_RESTRICT src,
                        std::size_t const n, T* const dst) noexcept ->
    enable_if_t<(E == endian::host)>
{
    std::memcpy(dst, src, n * sizeof(T));
}

template<endian E, typename T>
AMP_INLINE auto load_n_(uchar const* const AMP_RESTRICT src,
                        std::size_t const n, T* const dst) noexcept ->
    enable_if_t<(E != endian::host)>
{
    for (auto const i : xrange(n)) {
        dst[i] = io::load<T,E>(&src[i * sizeof(T)]);
    }
}


template<endian E, typename T>
AMP_INLINE auto store_n_(uchar* const dst, std::size_t const n,
                         T const* const AMP_RESTRICT src) noexcept ->
    enable_if_t<(E == endian::host)>
{
    std::memcpy(dst, src, n * sizeof(T));
}

template<endian E, typename T>
AMP_INLINE auto store_n_(uchar* const dst, std::size_t const n,
                         T const* const AMP_RESTRICT src) noexcept ->
    enable_if_t<(E != endian::host)>
{
    for (auto const i : xrange(n)) {
        io::store<E>(&dst[i * sizeof(T)], src[i]);
    }
}

}     // namespace aux


template<endian E, typename T>
AMP_INLINE void load_n(void const* const AMP_RESTRICT src,
                       std::size_t const n, T* const dst) noexcept
{
    aux::load_n_<E>(static_cast<uchar const*>(src), n, dst);
}

template<endian E, typename T>
AMP_INLINE void store_n(void* const dst, std::size_t const n,
                        T const* const AMP_RESTRICT src) noexcept
{
    aux::store_n_<E>(static_cast<uchar*>(dst), n, src);
}



namespace aux {

template<std::size_t>
struct ignore_ {};

template<typename T, typename U>
struct alias_ { U& u; };

}     // namespace aux


template<std::size_t N>
constexpr io::aux::ignore_<N> ignore{};

template<typename T, typename U>
AMP_INLINE constexpr auto alias(U& u) noexcept
{ return io::aux::alias_<T, U>{u}; }


namespace aux {

template<typename...>
struct packed_size_;

template<typename T>
struct packed_size_<T> :
    std::integral_constant<std::size_t, sizeof(T)>
{};

template<typename T, typename U>
struct packed_size_<io::aux::alias_<T, U>> :
    std::integral_constant<std::size_t, sizeof(T)>
{};

template<std::size_t N>
struct packed_size_<io::aux::ignore_<N>> :
    std::integral_constant<std::size_t, N>
{};

template<typename T, typename U, typename... Rest>
struct packed_size_<T, U, Rest...> :
    std::integral_constant<
        std::size_t,
        packed_size_<T>::value + packed_size_<U, Rest...>::value
    >
{};

}     // namespace aux

template<typename... T>
static constexpr auto packed_size =
    aux::packed_size_<remove_cv_t<remove_reference_t<T>>...>::value;


namespace aux {

template<endian E, typename T>
AMP_INLINE void gather_(uchar const* const src, T& dst) noexcept
{
    dst = io::load<T,E>(src);
}

template<endian E, typename T, std::size_t N>
AMP_INLINE void gather_(uchar const* const src, T(&dst)[N]) noexcept
{
    io::load_n<E>(src, N, dst);
}

template<endian E, typename T, typename U>
AMP_INLINE void gather_(uchar const* const src,
                        io::aux::alias_<T, U>&& dst) noexcept
{
    dst.u = io::load<T,E>(src);
}

template<endian E, std::size_t N>
AMP_INLINE void gather_(uchar const*, io::aux::ignore_<N> const&) noexcept
{
}

template<endian E, typename T, typename U, typename... Rest>
AMP_INLINE void gather_(uchar const* const src,
                        T&& t, U&& u, Rest&&... rest) noexcept
{
    gather_<E>(src,
               std::forward<T>(t));
    gather_<E>(src + packed_size<T>,
               std::forward<U>(u),
               std::forward<Rest>(rest)...);
}


template<endian E, typename T>
AMP_INLINE void scatter_(uchar* const dst, T const& src) noexcept
{
    io::store<E>(dst, src);
}

template<endian E, typename T, std::size_t N>
AMP_INLINE void scatter_(uchar* const dst, T const(&src)[N]) noexcept
{
    io::store_n<E>(dst, N, src);
}

template<endian E, typename T, typename U, typename... Rest>
AMP_INLINE void scatter_(uchar* const dst,
                         T&& t, U&& u, Rest&&... rest) noexcept
{
    scatter_<E>(dst,
                std::forward<T>(t));
    scatter_<E>(dst + packed_size<T>,
                std::forward<U>(u),
                std::forward<Rest>(rest)...);
}

}     // namespace aux

template<endian E, typename... T>
AMP_INLINE void gather(void const* const src, T&&... t) noexcept
{ aux::gather_<E>(static_cast<uchar const*>(src), std::forward<T>(t)...); }

template<endian E, typename... T>
AMP_INLINE void scatter(void* const dst, T&&... t) noexcept
{ aux::scatter_<E>(static_cast<uchar*>(dst), std::forward<T>(t)...); }

}}    // namespace amp::io


#endif  // AMP_INCLUDED_FB41D387_0C1D_42C0_A531_9540B9008F37

