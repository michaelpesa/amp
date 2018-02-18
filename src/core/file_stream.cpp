////////////////////////////////////////////////////////////////////////////////
//
// core/file_stream.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/error.hpp>
#include <amp/io/stream.hpp>
#include <amp/net/uri.hpp>
#include <amp/numeric.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>
#include <amp/utility.hpp>

#include <cerrno>
#include <cstddef>
#include <utility>

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


namespace amp {
namespace io {
namespace {

template<typename F, typename Byte>
AMP_INLINE auto nointr(F f, int const fd, Byte* const buf, std::size_t const n)
{
    auto pos = 0_sz;
    do {
        auto const ret = f(fd, buf + pos, n - pos);
        if (AMP_UNLIKELY(ret <= 0)) {
            if (ret == 0) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            return -1_sz;
        }
        pos += static_cast<std::size_t>(ret);
    }
    while (pos < n);
    return pos;
}

AMP_INLINE int get_open_flags(io::open_mode const mode)
{
    int oflags;
    if (mode & io::out) {
        oflags = ((mode & io::in) ? O_RDWR : O_WRONLY) | O_CREAT;
    }
    else if (mode & io::in) {
        oflags = O_RDONLY;
    }
    else {
        raise(errc::invalid_argument);
    }

    if (mode & io::app) {
        oflags |= O_APPEND;
    }
    if (mode & io::trunc) {
        oflags |= O_TRUNC;
    }
    return oflags;
}


class file_stream final :
    public implement_ref_count<file_stream, io::stream>
{
public:
    explicit file_stream(net::uri const& u, io::open_mode const mode) :
        location_(u)
    {
        auto const path = location_.get_file_path();
        fd_ = ::open(path.c_str(), get_open_flags(mode), 0666);
        if (AMP_UNLIKELY(fd_ == -1)) {
            raise_current_system_error();
        }
    }

    ~file_stream()
    {
        ::close(fd_);
    }

    net::uri location() const noexcept override
    { return location_; }

    bool eof() noexcept override
    { return eof_; }

    uint64 size() override
    {
        struct ::stat st;
        if (AMP_UNLIKELY(::fstat(fd_, &st) != 0)) {
            raise_current_system_error();
        }
        return static_cast<uint64>(st.st_size);
    }

    uint64 tell() override
    {
        auto const ret = ::lseek(fd_, 0, SEEK_CUR);
        if (AMP_UNLIKELY(ret == -1)) {
            raise_current_system_error();
        }
        return static_cast<uint64>(ret);
    }

    void truncate(uint64 const size) override
    {
        if (::ftruncate(fd_, numeric_cast<::off_t>(size)) != 0) {
            raise_current_system_error();
        }
    }

    void write(void const* const buf, std::size_t const n) override
    {
        auto ret = nointr(::write, fd_, static_cast<uchar const*>(buf), n);
        if (AMP_UNLIKELY(ret == -1_sz)) {
            raise_current_system_error();
        }
    }

    std::size_t try_read(void* const buf, std::size_t n) override
    {
        if (n != 0) {
            auto ret = nointr(::read, fd_, static_cast<uchar*>(buf), n);
            if (AMP_UNLIKELY(ret == -1_sz)) {
                raise_current_system_error();
            }
            if (ret == 0) {
                eof_ = true;
            }
            n = ret;
        }
        return n;
    }

    void read(void* const buf, std::size_t const n) override
    {
        if (AMP_UNLIKELY(try_read(buf, n) < n)) {
            raise(errc::end_of_file);
        }
    }

    void seek(int64 const off, seekdir const way) override
    {
        eof_ = false;
        if (AMP_UNLIKELY(::lseek(fd_, off, as_underlying(way)) == -1)) {
            raise_current_system_error();
        }
    }

private:
    net::uri location_;
    int fd_;
    bool eof_{};
};

AMP_REGISTER_IO_STREAM(file_stream, "file");

}}}   // namespace amp::io::<unnamed>

