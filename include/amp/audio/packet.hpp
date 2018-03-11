////////////////////////////////////////////////////////////////////////////////
//
// amp/audio/packet.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_3CF5CAEB_70C6_4D5D_BEF2_4FF751DEB901
#define AMP_INCLUDED_3CF5CAEB_70C6_4D5D_BEF2_4FF751DEB901


#include <amp/bitops.hpp>
#include <amp/error.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>


namespace amp {
namespace audio {

class packet_buffer
{
public:
    using value_type     = float;
    using pointer        = float*;
    using const_pointer  = float const*;
    using iterator       = float*;
    using const_iterator = float const*;
    using size_type      = std::size_t;

    explicit packet_buffer(size_type const n, uninitialized_t) :
        data_{packet_buffer::allocate_(n)},
        size_{n},
        capacity_{n}
    {}

    explicit packet_buffer(size_type const n) :
        packet_buffer{n, uninitialized}
    {
        std::fill(begin(), end(), value_type{});
    }

    packet_buffer() = default;

    packet_buffer(packet_buffer const& x) :
        packet_buffer{x.size(), uninitialized}
    {
        std::copy(x.begin(), x.end(), begin());
    }

    packet_buffer(packet_buffer&& x) noexcept :
        data_{std::exchange(x.data_, nullptr)},
        size_{std::exchange(x.size_, 0)},
        capacity_{std::exchange(x.capacity_, 0)}
    {}

    packet_buffer& operator=(packet_buffer const& x) &
    {
        packet_buffer{x}.swap(*this);
        return *this;
    }

    packet_buffer& operator=(packet_buffer&& x) & noexcept
    {
        packet_buffer{std::move(x)}.swap(*this);
        return *this;
    }

    ~packet_buffer()
    {
        if (data_ != nullptr) {
            std::free(data_);
        }
    }

    void swap(packet_buffer& x) noexcept
    {
        using std::swap;
        swap(data_, x.data_);
        swap(size_, x.size_);
        swap(capacity_, x.capacity_);
    }

    AMP_INLINE size_type size() const noexcept
    { return size_; }

    AMP_INLINE size_type capacity() const noexcept
    { return capacity_; }

    AMP_INLINE bool empty() const noexcept
    { return (size() == 0); }

    AMP_INLINE pointer data() noexcept
    { return data_; }

    AMP_INLINE const_pointer data() const noexcept
    { return data_; }

    AMP_INLINE iterator begin() noexcept
    { return data(); }

    AMP_INLINE iterator end() noexcept
    { return data() + size(); }

    AMP_INLINE const_iterator begin() const noexcept
    { return cbegin(); }

    AMP_INLINE const_iterator end() const noexcept
    { return cend(); }

    AMP_INLINE const_iterator cbegin() const noexcept
    { return data(); }

    AMP_INLINE const_iterator cend() const noexcept
    { return data() + size(); }

    AMP_INLINE value_type& operator[](size_type const n) noexcept
    { return data()[n]; }

    AMP_INLINE value_type const& operator[](size_type const n) const noexcept
    { return data()[n]; }

    void clear() noexcept
    {
        size_ = 0;
    }

    void pop_front(size_type const n) noexcept
    {
        if (n >= size()) {
            clear();
        }
        else if (n != 0) {
            std::copy(cbegin() + n, cend(), begin());
            size_ -= n;
        }
    }

    void pop_back(size_type const n) noexcept
    {
        size_ -= std::min(n, size());
    }

    void assign(const_pointer const first, size_type const n)
    {
        resize(n, uninitialized);
        std::copy_n(first, n, data());
    }

    void assign(const_pointer const first, const_pointer const last)
    {
        assign(first, static_cast<size_type>(last - first));
    }

    void append(const_pointer const first, size_type const n)
    {
        auto const start = size();
        resize(start + n);
        std::copy_n(first, n, begin() + start);
    }

    void append(const_pointer const first, const_pointer const last)
    {
        append(first, static_cast<size_type>(last - first));
    }

    void resize(size_type const n)
    {
        if (n > size()) {
            if (n > capacity()) {
                auto const tmp = std::realloc(data_, n * sizeof(value_type));
                if (tmp == nullptr) {
                    raise_bad_alloc();
                }
                data_ = static_cast<value_type*>(tmp);
                capacity_ = n;
            }
            std::fill(begin() + size(), begin() + n, value_type{});
        }
        size_ = n;
    }

    void resize(size_type const n, uninitialized_t)
    {
        if (n > capacity()) {
            auto const tmp = std::realloc(data_, n * sizeof(value_type));
            if (tmp == nullptr) {
                raise_bad_alloc();
            }
            data_ = static_cast<value_type*>(tmp);
            capacity_ = n;
        }
        size_ = n;
    }

private:
    AMP_INLINE static pointer allocate_(size_type const n)
    {
        if (n != 0) {
            auto const p = std::malloc(n * sizeof(float));
            if (p == nullptr) {
                raise_bad_alloc();
            }
            return static_cast<pointer>(p);
        }
        return nullptr;
    }

    pointer data_{};
    size_type size_{};
    size_type capacity_{};
};


class packet
{
public:
    using value_type     = typename packet_buffer::value_type;
    using pointer        = typename packet_buffer::pointer;
    using const_pointer  = typename packet_buffer::const_pointer;
    using iterator       = typename packet_buffer::iterator;
    using const_iterator = typename packet_buffer::const_iterator;
    using size_type      = typename packet_buffer::size_type;

    explicit packet(size_type const n, uninitialized_t) :
        buffer_{n, uninitialized}
    {}

    explicit packet(size_type const n) :
        buffer_{n}
    {}

    packet() = default;
    packet(packet const&) = default;
    packet(packet&&) = default;
    packet& operator=(packet const&) & = default;
    packet& operator=(packet&&) & = default;

    void swap(packet& x) noexcept
    {
        using std::swap;
        swap(buffer_, x.buffer_);
        swap(bit_rate_, x.bit_rate_);
        swap(channels_, x.channels_);
        swap(channel_layout_, x.channel_layout_);
    }

    AMP_INLINE bool empty() const noexcept
    { return buffer_.empty(); }

    AMP_INLINE size_type size() const noexcept
    { return buffer_.size(); }

    AMP_INLINE size_type capacity() const noexcept
    { return buffer_.capacity(); }

    AMP_INLINE size_type samples() const noexcept
    { return size(); }

    AMP_INLINE uint32 channels() const noexcept
    { return channels_; }

    AMP_INLINE uint32 channel_layout() const noexcept
    { return channel_layout_; }

    AMP_INLINE uint32 bit_rate() const noexcept
    { return bit_rate_; }

    AMP_INLINE size_type frames() const noexcept
    {
        return channels() != 0
             ? samples() / channels()
             : 0;
    }

    AMP_INLINE pointer data() noexcept
    { return buffer_.data(); }

    AMP_INLINE const_pointer data() const noexcept
    { return buffer_.data(); }

    AMP_INLINE iterator begin() noexcept
    { return buffer_.begin(); }

    AMP_INLINE iterator end() noexcept
    { return buffer_.end(); }

    AMP_INLINE const_iterator begin() const noexcept
    { return buffer_.begin(); }

    AMP_INLINE const_iterator end() const noexcept
    { return buffer_.end(); }

    AMP_INLINE const_iterator cbegin() const noexcept
    { return buffer_.cbegin(); }

    AMP_INLINE const_iterator cend() const noexcept
    { return buffer_.cend(); }

    AMP_INLINE value_type& operator[](size_type const n) noexcept
    { return buffer_[n]; }

    AMP_INLINE value_type const& operator[](size_type const n) const noexcept
    { return buffer_[n]; }

    void clear() noexcept
    {
        buffer_.clear();
        bit_rate_ = 0;
    }

    void pop_front(size_type const n) noexcept
    { buffer_.pop_front(n); }

    void pop_back(size_type const n) noexcept
    { buffer_.pop_back(n); }

    void assign(const_pointer const first, size_type const n)
    { buffer_.assign(first, n); }

    void assign(const_pointer const first, const_pointer const last)
    { buffer_.assign(first, last); }

    void append(const_pointer const first, size_type const n)
    { buffer_.append(first, n); }

    void append(const_pointer const first, const_pointer const last)
    { buffer_.append(first, last); }

    void resize(size_type const n)
    { buffer_.resize(n); }

    void resize(size_type const n, uninitialized_t)
    { buffer_.resize(n, uninitialized); }

    void fill_planar(const_pointer const* const planes, size_type const n)
    {
        resize(n * channels(), uninitialized);
        interleave_(planes, n, begin(), channels());
    }

    void append_planar(const_pointer const* const planes, size_type const n)
    {
        auto const start = samples();
        resize(start + (n * channels()));
        interleave_(planes, n, begin() + start, channels());
    }

    void set_channel_layout(uint32 const layout, uint32 const n) noexcept
    {
        AMP_ASSERT(n == popcnt(layout));
        channel_layout_ = layout;
        channels_ = n;
    }

    void set_channel_layout(uint32 const layout) noexcept
    {
        if (channel_layout_ != layout) {
            channel_layout_  = layout;
            channels_ = popcnt(layout);
        }
    }

    void set_bit_rate(uint32 const bit_rate) noexcept
    {
        bit_rate_ = bit_rate;
    }

private:
    static void interleave_(float const* AMP_RESTRICT src, size_type n,
                            float* dst, size_type const stride) noexcept
    {
        while (n-- != 0) {
            *dst = *src++;
            dst += stride;
        }
    }

    static void interleave_(float const* const* const src, size_type const n,
                            float* const dst, size_type const stride) noexcept
    {
        for (auto const c : xrange(stride)) {
            interleave_(src[c], n, dst + c, stride);
        }
    }

    audio::packet_buffer buffer_;
    uint32 bit_rate_{};
    uint32 channels_{};
    uint32 channel_layout_{};
};


AMP_INLINE void swap(packet& x, packet& y) noexcept
{
    x.swap(y);
}

}}    // namespace amp::audio


#endif  // AMP_INCLUDED_3CF5CAEB_70C6_4D5D_BEF2_4FF751DEB901

