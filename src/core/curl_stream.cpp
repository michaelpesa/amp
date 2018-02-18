////////////////////////////////////////////////////////////////////////////////
//
// core/curl_stream.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/error.hpp>
#include <amp/io/buffer.hpp>
#include <amp/io/stream.hpp>
#include <amp/net/uri.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/scope_guard.hpp>
#include <amp/stddef.hpp>

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>

#include <curl/curl.h>


namespace amp {
namespace io {
namespace {

class curl_stream final :
    public implement_ref_count<curl_stream, io::stream>
{
public:
    explicit curl_stream(net::uri u, io::open_mode const mode) :
        location_{std::move(u)}
    {
        if ((mode & ~io::binary) != io::in) {
            raise(errc::not_implemented,
                  "HTTP(S) stream writing is not implemented");
        }

        auto self = ref_ptr<io::curl_stream>::acquire(this);
        std::thread(&io::curl_stream::fetch_, std::move(self)).detach();
    }

    net::uri location() const noexcept override
    { return location_; }

    bool eof() noexcept override
    { return eof_; }

    uint64 size() override
    {
        wait_until_fetched_();
        return buffer_.size();
    }

    uint64 tell() noexcept override
    { return cursor_; }

    void truncate(uint64) override
    { raise(errc::not_implemented); }

    void write(void const*, std::size_t) override
    { raise(errc::not_implemented); }

    std::size_t try_read(void* const dst, std::size_t len) override
    {
        if (AMP_LIKELY(len != 0)) {
            wait_until_fetched_();

            auto const remain = buffer_.size() - cursor_;
            if (len > remain) {
                len = remain;
                eof_ = true;
            }
            std::copy_n(&buffer_[cursor_], len, static_cast<uint8*>(dst));
            cursor_ += len;
        }
        return len;
    }

    void read(void* const dst, std::size_t const len) override
    {
        auto const ret = try_read(dst, len);
        if (AMP_UNLIKELY(ret != len)) {
            raise(errc::end_of_file);
        }
    }

    void seek(int64 off, seekdir const way) override
    {
        wait_until_fetched_();
        eof_ = false;

        auto const end_pos = static_cast<int64>(buffer_.size());
        auto const cur_pos = static_cast<int64>(cursor_);

        switch (way) {
        case seekdir::beg: break;
        case seekdir::cur: off += cur_pos; break;
        case seekdir::end: off += end_pos; break;
        }

        if (AMP_UNLIKELY(off < 0 || off > end_pos)) {
            raise(errc::invalid_argument);
        }
        cursor_ = static_cast<uint64>(off);
    }

private:
    static remove_pointer_t<::curl_write_callback> write_cb;

    static void fetch_(ref_ptr<io::curl_stream> const self)
    {
        auto const handle = ::curl_easy_init();
        AMP_SCOPE_EXIT {
            {
                std::lock_guard<std::mutex> const lk{self->mtx_};
                self->fetched_ = true;
            }
            self->cnd_.notify_all();
            ::curl_easy_cleanup(handle);
        };

        ::curl_easy_setopt(handle, ::CURLOPT_URL, self->location_.data());
        ::curl_easy_setopt(handle, ::CURLOPT_WRITEDATA, &self->buffer_);
        ::curl_easy_setopt(handle, ::CURLOPT_WRITEFUNCTION, &write_cb);
        ::curl_easy_setopt(handle, ::CURLOPT_VERBOSE, 0L);

        auto const response = ::curl_easy_perform(handle);
        std::fprintf(stderr, "[curl_stream] response=%d\n", response);
    }

    void wait_until_fetched_()
    {
        std::unique_lock<std::mutex> lk{mtx_};
        cnd_.wait(lk, [&]{ return fetched_; });
    }

    net::uri location_;
    io::buffer buffer_;
    std::size_t cursor_{};
    bool eof_{};
    bool fetched_{};
    std::mutex mtx_;
    std::condition_variable cnd_;
};

std::size_t curl_stream::write_cb(char* const src, std::size_t const size,
                                  std::size_t const n, void* const opaque)
{
    static_cast<io::buffer*>(opaque)->append(src, size * n);
    return size * n;
}

AMP_REGISTER_IO_STREAM(curl_stream, "http", "https");

}}}   // namespace amp::io::<unnamed>

