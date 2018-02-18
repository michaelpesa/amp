////////////////////////////////////////////////////////////////////////////////
//
// audio/circular_buffer.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_F533A19F_2933_497B_A75D_23246A9A483A
#define AMP_INCLUDED_F533A19F_2933_497B_A75D_23246A9A483A


#include <amp/bitops.hpp>
#include <amp/stddef.hpp>

#include "core/cpu.hpp"

#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>


namespace amp {
namespace audio {
namespace aux {

extern void* allocate_mirrored(std::size_t);
extern void deallocate_mirrored(void*, std::size_t) noexcept;

}     // namespace aux


template<typename T>
class circular_buffer
{
public:
    static_assert(is_pod_v<T>, "");
    static_assert(is_pow2(sizeof(T)), "");

    using value_type    = T;
    using pointer       = T*;
    using const_pointer = T const*;
    using size_type     = std::size_t;

    explicit circular_buffer(size_type const n) :
        size_{ceil_pow2(n)},
        data_{static_cast<pointer>(aux::allocate_mirrored(size_ * sizeof(T)))}
    {}

    circular_buffer(circular_buffer const&) = delete;
    circular_buffer& operator=(circular_buffer const&) = delete;

    ~circular_buffer()
    {
        aux::deallocate_mirrored(data_, size_ * sizeof(T));
    }

    size_type read_avail() const noexcept
    { return fill_.load(std::memory_order_relaxed); }

    size_type read_acquire() const noexcept
    { return fill_.load(std::memory_order_acquire); }

    const_pointer read_cursor() const noexcept
    { return data_ + tail_; }

    void read_release(size_type const n) noexcept
    {
        tail_ = (tail_ + n) & (capacity() - 1);
        fill_.fetch_sub(n, std::memory_order_release);
    }

    void read_flush() noexcept
    {
        if (auto const n = read_acquire()) {
            read_release(n);
        }
    }

    size_type write_avail() const noexcept
    { return capacity() - fill_.load(std::memory_order_relaxed); }

    size_type write_prepare() const noexcept
    { return capacity() - fill_.load(std::memory_order_acquire); }

    pointer write_cursor() const noexcept
    { return data_ + head_; }

    size_type capacity() const noexcept
    { return size_; }

    void write_commit(size_type const n) noexcept
    {
        head_ = (head_ + n) & (capacity() - 1);
        fill_.fetch_add(n, std::memory_order_release);
    }


    static void* operator new(std::size_t) = delete;
    static void* operator new[](std::size_t) = delete;
    static void operator delete(void*, std::size_t) = delete;
    static void operator delete[](void*, std::size_t) = delete;

private:
    alignas(cache_line_size) std::atomic<size_type> fill_{0};
    alignas(cache_line_size) size_type head_{0};
    alignas(cache_line_size) size_type tail_{0};
    size_type const size_;
    pointer const data_;
};

}}    // namespace amp::audio


#endif  // AMP_INCLUDED_F533A19F_2933_497B_A75D_23246A9A483A

