////////////////////////////////////////////////////////////////////////////////
//
// amp/io/buffer.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_A3445AF4_2C93_4824_98AD_F6269115197C
#define AMP_INCLUDED_A3445AF4_2C93_4824_98AD_F6269115197C


#include <amp/aux/operators.hpp>
#include <amp/error.hpp>
#include <amp/io/stream.hpp>
#include <amp/memory.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <utility>


namespace amp {
namespace io {

class buffer :
    private totally_ordered<buffer>
{
public:
    using value_type     = uint8;
    using iterator       = uint8*;
    using const_iterator = uint8 const*;

    explicit buffer(std::size_t const n, uninitialized_t) :
        data_{n ? static_cast<uint8*>(std::malloc(n)) : nullptr},
        size_{n},
        capacity_{n}
    {
        if (data() == nullptr && size() != 0) {
            raise_bad_alloc();
        }
    }

    explicit buffer(std::size_t const n) :
        buffer{n, uninitialized}
    {
        std::fill(begin(), end(), uint8{0});
    }

    explicit buffer(void const* const src, std::size_t const n) :
        buffer{n, uninitialized}
    {
        std::copy_n(static_cast<uint8 const*>(src), n, data());
    }

    explicit buffer(io::stream& file, std::size_t const n) :
        buffer{n, uninitialized}
    {
        file.read(data(), n);
    }

    template<typename T, typename = enable_if_t<is_byte_v<T>>>
    explicit buffer(T const* const first, T const* const last) :
        buffer{first, static_cast<std::size_t>(last - first)}
    {}

    constexpr buffer() noexcept :
        data_{nullptr},
        size_{0},
        capacity_{0}
    {}

    buffer(buffer const& x) :
        buffer{x.data(), x.size()}
    {}

    buffer(buffer&& x) noexcept :
        data_{std::exchange(x.data_, nullptr)},
        size_{std::exchange(x.size_, 0)},
        capacity_{std::exchange(x.capacity_, 0)}
    {}

    ~buffer()
    {
        std::free(data_);
    }

    buffer& operator=(buffer const& x) &
    {
        buffer{x}.swap(*this);
        return *this;
    }

    buffer& operator=(buffer&& x) & noexcept
    {
        buffer{std::move(x)}.swap(*this);
        return *this;
    }

    void swap(buffer& x) noexcept
    {
        using std::swap;
        swap(data_,     x.data_);
        swap(size_,     x.size_);
        swap(capacity_, x.capacity_);
    }

    uint8* data() noexcept
    { return data_; }

    uint8 const* data() const noexcept
    { return data_; }

    std::size_t size() const noexcept
    { return size_; }

    std::size_t capacity() const noexcept
    { return capacity_; }

    bool empty() const noexcept
    { return (size() == 0); }

    iterator begin() noexcept
    { return data(); }

    iterator end() noexcept
    { return data() + size(); }

    const_iterator begin() const noexcept
    { return cbegin(); }

    const_iterator end() const noexcept
    { return cend(); }

    const_iterator cbegin() const noexcept
    { return data(); }

    const_iterator cend() const noexcept
    { return data() + size(); }

    uint8& operator[](std::size_t const i) noexcept
    { return data()[i]; }

    uint8 const& operator[](std::size_t const i) const noexcept
    { return data()[i]; }

    int compare(buffer const& x) const noexcept
    { return mem::compare(data(), size(), x.data(), x.size()); }

    void clear() noexcept
    { size_ = 0; }

    void pop_back(std::size_t const n) noexcept
    { size_ -= std::min(size_, n); }

    void pop_front(std::size_t const n) noexcept
    {
        if (n >= size()) {
            clear();
        }
        else if (n != 0) {
            std::copy(cbegin() + n, cend(), begin());
            size_ -= n;
        }
    }

    iterator insert(const_iterator const cpos, void const* const src,
                    std::size_t const n)
    {
        auto pos = const_cast<iterator>(cpos);
        if (n != 0) {
            auto const saved = pos - cbegin();
            auto const old_size = size();
            resize(old_size + n, uninitialized);

            pos = begin() + saved;
            std::copy(pos, begin() + old_size, pos + n);
            std::copy_n(static_cast<const_iterator>(src), n, pos);
        }
        return pos;
    }

    void reserve(std::size_t const n)
    {
        if (n > capacity()) {
            reallocate_(n);
        }
    }

    void resize(std::size_t const n)
    {
        if (n > size()) {
            if (n > capacity()) {
                reallocate_(n);
            }
            std::fill(begin() + size(), begin() + n, uint8{});
        }
        size_ = n;
    }

    void resize(std::size_t const n, uninitialized_t)
    {
        if (n > capacity()) {
            reallocate_(n);
        }
        size_ = n;
    }

    void grow(std::size_t const n)
    {
        auto const new_size = size() + n;
        if (new_size > capacity()) {
            reallocate_(std::max(capacity() * 2, new_size));
        }
        std::fill(begin() + size(), begin() + new_size, uint8{});
        size_ = new_size;
    }

    void grow(std::size_t const n, uninitialized_t)
    {
        auto const new_size = size() + n;
        if (new_size > capacity()) {
            reallocate_(std::max(capacity() * 2, new_size));
        }
        size_ = new_size;
    }

    void assign(void const* const src, std::size_t const n)
    {
        resize(n, uninitialized);
        std::copy_n(static_cast<uint8 const*>(src), n, data());
    }

    void assign(io::stream& file, std::size_t const n)
    {
        resize(n, uninitialized);
        file.read(data(), n);
    }

    void append(void const* const src, std::size_t const n)
    {
        if (n != 0) {
            auto const new_size = size() + n;
            if (new_size > capacity()) {
                reallocate_(std::max(capacity() * 2, new_size));
            }
            std::copy_n(static_cast<uint8 const*>(src), n, end());
            size_ = new_size;
        }
    }

private:
    void reallocate_(std::size_t const n)
    {
        auto const tmp = std::realloc(data_, n);
        if (tmp == nullptr) {
            raise_bad_alloc();
        }
        data_ = static_cast<uint8*>(tmp);
        capacity_ = n;
    }

    friend void swap(buffer& x, buffer& y) noexcept
    { x.swap(y); }

    friend bool operator==(buffer const& x, buffer const& y) noexcept
    { return mem::equal(x.data(), x.size(), y.data(), y.size()); }

    friend bool operator<(buffer const& x, buffer const& y) noexcept
    { return (x.compare(y) < 0); }

    uint8*      data_;
    std::size_t size_;
    std::size_t capacity_;
};

}}    // namespace amp::io


#endif  // AMP_INCLUDED_A3445AF4_2C93_4824_98AD_F6269115197C

