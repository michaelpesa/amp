////////////////////////////////////////////////////////////////////////////////
//
// amp/io/reader.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_FEF4E62A_0B5E_40E6_A5C9_C9A0D8BEC1E1
#define AMP_INCLUDED_FEF4E62A_0B5E_40E6_A5C9_C9A0D8BEC1E1


#include <amp/error.hpp>
#include <amp/io/memory.hpp>
#include <amp/optional.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <utility>


namespace amp {
namespace io {

class reader
{
public:
    using value_type = uint8;
    using pointer    = uint8 const*;
    using size_type  = std::size_t;

    constexpr reader() = default;

    constexpr reader(void const* const p, size_type const n) noexcept :
        data_{static_cast<pointer>(p)},
        size_{n}
    {}

    template<typename T, size_type N, typename = enable_if_t<is_byte_v<T>>>
    constexpr reader(T const(&buf)[N]) noexcept :
        reader{buf, N}
    {}

    template<
        typename Container,
        typename = enable_if_t<
            !is_same_v<reader, decay_t<Container>> &&
            !is_base_of_v<reader, decay_t<Container>> &&
            is_byte_v<typename decay_t<Container>::value_type>
        >
    >
    constexpr reader(Container&& c) noexcept :
        reader{c.data(), c.size()}
    {}

    AMP_INLINE size_type size() const noexcept
    { return size_; }

    AMP_INLINE size_type tell() const noexcept
    { return cursor_; }

    AMP_INLINE size_type remain() const noexcept
    { return size() - tell(); }

    AMP_INLINE pointer data() const noexcept
    { return data_; }

    AMP_INLINE pointer peek() const noexcept
    { return data() + tell(); }

    AMP_INLINE pointer peek(size_type const n) const
    {
        check_read_available(n);
        return peek();
    }

    AMP_INLINE pointer read_n(size_type const n)
    {
        check_read_available(n);
        return data() + std::exchange(cursor_, cursor_ + n);
    }

    template<typename T, endian E>
    AMP_INLINE std::string_view read_pascal_string()
    {
        auto const limit = remain();
        if (limit >= sizeof(T)) {
            auto const len = io::load<T,E>(peek());
            if (limit >= len + sizeof(T)) {
                auto const str = peek() + sizeof(T);
                cursor_ += len + sizeof(T);
                return {reinterpret_cast<char const*>(str), len};
            }
        }
        raise(errc::out_of_bounds);
    }

    template<typename T, typename = enable_if_t<is_byte_v<T>>>
    AMP_INLINE std::string_view read_pascal_string()
    {
        return read_pascal_string<T, endian::host>();
    }


    template<typename T, typename = enable_if_t<is_byte_v<T>>>
    AMP_INLINE void read(T* const dst, size_type const n)
    { std::copy_n(read_n(n), n, reinterpret_cast<uint8*>(dst)); }

    template<endian E, typename T>
    AMP_INLINE void read(T* const dst, size_type const n)
    { io::load_n<E>(read_n(n), n, dst); }

    template<typename T, typename = enable_if_t<is_byte_v<T>>>
    AMP_INLINE void read(T& dst)
    { dst = static_cast<T>(*read_n(1)); }

    template<endian E, typename T>
    AMP_INLINE void read(T& dst)
    { io::load<T,E>(read_n(sizeof(T)), dst); }

    template<typename T, size_type N, typename = enable_if_t<is_byte_v<T>>>
    AMP_INLINE void read(T(&dst)[N])
    { read(dst, N); }

    template<endian E, typename T, size_type N>
    AMP_INLINE void read(T(&dst)[N])
    { read<E>(dst, N); }

    template<typename T, typename = enable_if_t<is_byte_v<T>>>
    AMP_INLINE T read()
    { return static_cast<T>(*read_n(1)); }

    template<typename T, endian E>
    AMP_INLINE T read()
    { return io::load<T,E>(read_n(sizeof(T))); }

    AMP_INLINE void seek(size_type const pos)
    {
        if (pos <= size()) {
            cursor_ = pos;
            return;
        }

        raise(errc::out_of_bounds,
              "io::reader: cannot seek to byte %zu of %zu",
              pos, size());
    }

    AMP_INLINE void skip(size_type const n)
    { seek(tell() + n); }

    AMP_INLINE void rewind(size_type const n) noexcept
    { seek(tell() - n); }

    AMP_INLINE void rewind() noexcept
    { cursor_ = 0; }

    AMP_INLINE reader slice(size_type const n) const
    {
        if (n <= remain()) {
            return reader{peek(), n};
        }

        raise(errc::out_of_bounds,
              "io::reader: cannot slice %zu of %zu bytes",
              n, remain());
    }

    template<endian E, typename... T>
    AMP_INLINE void gather(T&&... t)
    {
        io::gather<E>(read_n(io::packed_size<T...>),
                      std::forward<T>(t)...);
    }


    // ------------------------------------------------------------------------
    // Unsafe methods.
    // ------------------------------------------------------------------------

    pointer read_n_unchecked(size_type const n) noexcept
    { return data() + std::exchange(cursor_, cursor_ + n); }

    template<typename T, typename = enable_if_t<is_byte_v<T>>>
    void read_unchecked(T* const dst, size_type const n) noexcept
    { std::copy_n(read_n_unchecked(n), n, reinterpret_cast<uint8*>(dst)); }

    template<endian E, typename T>
    void read_unchecked(T* const dst, size_type const n) noexcept
    { io::load_n<E>(read_n_unchecked(n), n, dst); }

    template<typename T, size_type N, typename = enable_if_t<is_byte_v<T>>>
    void read_unchecked(T(&dst)[N]) noexcept
    { read_unchecked(dst, N); }

    template<endian E, typename T, size_type N>
    void read_unchecked(T(&dst)[N]) noexcept
    { read_unchecked<E>(dst, N); }

    template<typename T, typename = enable_if_t<is_byte_v<T>>>
    void read_unchecked(T& dst) noexcept
    { dst = static_cast<T>(*read_n_unchecked(1)); }

    template<endian E, typename T>
    void read_unchecked(T& dst) noexcept
    { io::load<T,E>(read_n_unchecked(sizeof(T)), dst); }

    template<typename T, typename = enable_if_t<is_byte_v<T>>>
    T read_unchecked() noexcept
    { return static_cast<T>(*read_n_unchecked(1)); }

    template<typename T, endian E>
    T read_unchecked() noexcept
    { return io::load<T,E>(read_n_unchecked(sizeof(T))); }

    void seek_unchecked(size_type const pos) noexcept
    { cursor_ = pos; }

    void skip_unchecked(size_type const n) noexcept
    { cursor_ += n; }

    void rewind_unchecked(size_type const n) noexcept
    { cursor_ -= n; }

    reader slice_unchecked(size_type const n) const noexcept
    { return reader{data() + tell(), n}; }

    template<endian E, typename... T>
    AMP_INLINE void gather_unchecked(T&&... t) noexcept
    {
        io::gather<E>(read_n_unchecked(io::packed_size<T...>),
                      std::forward<T>(t)...);
    }


    // ------------------------------------------------------------------------
    // Optional methods.
    // ------------------------------------------------------------------------

    template<typename T, typename = enable_if_t<is_byte_v<T>>>
    optional<T> try_read() noexcept
    {
        return (remain() >= sizeof(T))
             ? optional<T>{read_unchecked<T>()}
             : optional<T>{};
    }

    template<typename T, endian E>
    optional<T> try_read() noexcept
    {
        return (remain() >= sizeof(T))
             ? optional<T>{read_unchecked<T,E>()}
             : optional<T>{};
    }

private:
    AMP_INLINE void check_read_available(size_type const n) const
    {
        if (n > remain()) {
            raise(errc::out_of_bounds,
                  "io::reader: cannot read %zu of %zu bytes", n, remain());
        }
    }

    pointer   data_{};
    size_type size_{};
    size_type cursor_{};
};

}}    // namespace amp::io


#endif  // AMP_INCLUDED_FEF4E62A_0B5E_40E6_A5C9_C9A0D8BEC1E1

