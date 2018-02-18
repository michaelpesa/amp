////////////////////////////////////////////////////////////////////////////////
//
// amp/io/stream.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_9241F152_1ADD_4B26_8FD9_0147D3B724A0
#define AMP_INCLUDED_9241F152_1ADD_4B26_8FD9_0147D3B724A0


#include <amp/net/endian.hpp>
#include <amp/net/uri.hpp>
#include <amp/io/memory.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <utility>


namespace amp {
namespace io {

enum class seekdir : int32 {
    beg = 0,
    cur = 1,
    end = 2,
};
enum open_mode : uint32 {
    in     = (1 << 0),
    out    = (1 << 1),
    app    = (1 << 2),
    trunc  = (1 << 3),
    binary = (1 << 4),
};
AMP_DEFINE_ENUM_FLAG_OPERATORS(open_mode);

constexpr auto invalid_pos = -uint64{1};


class stream
{
public:
    static constexpr auto beg = seekdir::beg;
    static constexpr auto cur = seekdir::cur;
    static constexpr auto end = seekdir::end;

    virtual void add_ref() noexcept = 0;
    virtual void release() noexcept = 0;

    virtual net::uri location() const = 0;
    virtual bool eof() = 0;
    virtual uint64 size() = 0;
    virtual uint64 tell() = 0;
    virtual void seek(int64, io::seekdir) = 0;
    virtual void read(void*, std::size_t) = 0;
    virtual std::size_t try_read(void*, std::size_t) = 0;

    virtual void write(void const*, std::size_t) = 0;
    virtual void truncate(uint64) = 0;

    AMP_INLINE auto remain()
    { return size() - tell(); }

    AMP_INLINE void seek(uint64 const pos)
    { return seek(static_cast<int64>(pos), seekdir::beg); }

    AMP_INLINE void skip(uint64 const n)
    { return seek(static_cast<int64>(n), seekdir::cur); }

    AMP_INLINE void rewind(uint64 const n)
    { return seek(-static_cast<int64>(n), seekdir::cur); }

    AMP_INLINE void rewind()
    { return seek(0, seekdir::beg); }

    AMP_INLINE void peek(void* const dst, std::size_t const n)
    {
        read(dst, n);
        rewind(n);
    }

    template<typename T, std::size_t N, typename = enable_if_t<is_byte_v<T>>>
    AMP_INLINE void read(T(&src)[N])
    {
        read(src, N);
    }

    template<typename T, typename = enable_if_t<is_byte_v<T>>>
    AMP_INLINE T read()
    {
        T t;
        read(std::addressof(t), sizeof(t));
        return t;
    }

    template<typename T, endian E>
    AMP_INLINE T read()
    {
        T t;
        read(std::addressof(t), sizeof(t));
        return net::to_host<E>(t);
    }

    template<endian E, typename... T>
    AMP_INLINE void gather(T&&... t)
    {
        uchar packed[io::packed_size<T...>];
        read(packed);
        io::gather<E>(packed, std::forward<T>(t)...);
    }

    template<endian E, typename... T>
    AMP_INLINE void scatter(T&&... t)
    {
        uchar packed[io::packed_size<T...>];
        io::scatter<E>(packed, std::forward<T>(t)...);
        write(packed);
    }

    template<endian E, typename T>
    AMP_INLINE void write(T t)
    {
        t = net::to_host<E>(t);
        write(std::addressof(t), sizeof(t));
    }

    template<typename T, std::size_t N, typename = enable_if_t<is_byte_v<T>>>
    AMP_INLINE void write(T const(&src)[N])
    {
        write(src, N);
    }

protected:
    stream() = default;
    ~stream() = default;
};


AMP_EXPORT
ref_ptr<io::stream> open(net::uri const&, io::open_mode);


class stream_factory
{
public:
    virtual ref_ptr<stream> create(net::uri const&, io::open_mode) const = 0;

protected:
    stream_factory() = default;
    ~stream_factory() = default;

    AMP_EXPORT
    void register_(char const* const*, char const* const*) noexcept;
};


template<typename T>
class register_stream final :
    public stream_factory
{
public:
    explicit register_stream(std::initializer_list<char const*> il) noexcept
    {
        stream_factory::register_(il.begin(), il.end());
    }

    ref_ptr<stream> create(net::uri const& location,
                           io::open_mode const mode) const override
    {
        return T::make(location, mode);
    }
};

#define AMP_REGISTER_IO_STREAM(T, ...) \
    static ::amp::io::register_stream<T> AMP_PP_ANON(reg){__VA_ARGS__}

}}    // namespace amp::io


#endif  // AMP_INCLUDED_9241F152_1ADD_4B26_8FD9_0147D3B724A0

