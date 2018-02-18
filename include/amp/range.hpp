////////////////////////////////////////////////////////////////////////////////
//
// amp/range.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_F5F0014B_E641_4B8B_9567_2770F8E4ED1B
#define AMP_INCLUDED_F5F0014B_E641_4B8B_9567_2770F8E4ED1B


#include <amp/aux/iterator.hpp>
#include <amp/aux/operators.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <iterator>
#include <tuple>
#include <utility>


namespace amp {

template<typename T>
class iterator_range
{
public:
    using iterator        = T;
    using value_type      = iterator_value_t<iterator>;
    using pointer         = iterator_pointer_t<iterator>;
    using reference       = iterator_reference_t<iterator>;
    using difference_type = iterator_difference_t<iterator>;
    using size_type       = make_unsigned_t<difference_type>;

    iterator_range() = default;
    iterator_range(iterator_range&&) = default;
    iterator_range(iterator_range const&) = default;
    iterator_range& operator=(iterator_range&&) & = default;
    iterator_range& operator=(iterator_range const&) & = default;

    constexpr iterator_range(iterator b, iterator e)
    noexcept(is_nothrow_move_constructible_v<iterator>) :
        beg_(std::move(b)),
        end_(std::move(e))
    {}

    constexpr iterator_range(std::pair<iterator, iterator> p)
    noexcept(is_nothrow_move_constructible_v<iterator>) :
        beg_(std::move(p.first)),
        end_(std::move(p.second))
    {}

    template<
        typename Container,
        typename = enable_if_t<
            !is_same_v<iterator_range, decay_t<Container>> &&
            !is_base_of_v<iterator_range, decay_t<Container>> &&
            is_convertible_v<iterator_type_t<Container>, iterator>
        >
    >
    constexpr iterator_range(Container&& c) :
        beg_(adl_begin(c)),
        end_(adl_end(c))
    {}

    constexpr iterator begin() const
    noexcept(is_nothrow_copy_constructible_v<iterator>)
    {
        return beg_;
    }

    constexpr iterator end() const
    noexcept(is_nothrow_copy_constructible_v<iterator>)
    {
        return end_;
    }

    constexpr bool empty() const
    noexcept(noexcept(std::declval<iterator>() == std::declval<iterator>()))
    {
        return (beg_ == end_);
    }

    constexpr size_type size() const
    noexcept(noexcept(std::distance(std::declval<iterator>(),
                                    std::declval<iterator>())))
    {
        static_assert(is_forward_iterator_v<iterator>, "");
        return static_cast<size_type>(std::distance(beg_, end_));
    }

    constexpr auto split(difference_type const n) const
    {
        auto const mid = std::next(beg_, n);
        return std::pair<iterator_range, iterator_range> {
            std::piecewise_construct,
            std::forward_as_tuple(beg_, mid),
            std::forward_as_tuple(mid, end_),
        };
    }

    void swap(iterator_range& x)
    noexcept(is_nothrow_swappable_v<iterator>)
    {
        using std::swap;
        swap(beg_, x.beg_);
        swap(end_, x.end_);
    }

private:
    iterator beg_;
    iterator end_;
};

template<typename T>
AMP_INLINE void swap(iterator_range<T>& x, iterator_range<T>& y)
noexcept(noexcept(x.swap(y)))
{
    x.swap(y);
}


template<typename T>
AMP_INLINE constexpr auto make_range(T first, T last)
noexcept(is_nothrow_constructible_v<iterator_range<T>, T, T>)
{
    return iterator_range<T>(std::move(first), std::move(last));
}

template<typename T>
AMP_INLINE constexpr auto make_range(std::pair<T, T> p)
noexcept(is_nothrow_constructible_v<iterator_range<T>, T, T>)
{
    return iterator_range<T>(std::move(p.first), std::move(p.second));
}

template<typename T, typename Size>
AMP_INLINE constexpr auto make_range(T const first, Size const n)
noexcept(is_nothrow_constructible_v<iterator_range<T>, T, T> &&
         is_convertible_v<Size, iterator_difference_t<T>>)
{
    return iterator_range<T>(first, std::next(first, n));
}

template<typename Container>
AMP_INLINE constexpr auto make_range(Container&& c)
AMP_RETURNS(make_range(adl_begin(c), adl_end(c)));


namespace aux {

template<typename T>
class integral_iterator_ :
    private totally_ordered<integral_iterator_<T>>
{
public:
    static_assert(is_integral_v<T>, "");

    using iterator_category = std::random_access_iterator_tag;
    using difference_type   = make_signed_t<T>;
    using value_type        = T;
    using reference         = T const&;
    using pointer           = T const*;

    constexpr explicit integral_iterator_(T const v) noexcept :
        val_(v)
    {}

    constexpr reference operator*() const noexcept
    { return val_; }

    constexpr pointer operator->() const noexcept
    { return &val_; }

    constexpr auto operator++(int) noexcept
    {
        auto ret = *this;
        return (++(*this), ret);
    }

    constexpr auto operator--(int) noexcept
    {
        auto ret = *this;
        return (--(*this), ret);
    }

    constexpr auto& operator++() noexcept
    { return (++val_, *this); }

    constexpr auto& operator--() noexcept
    { return (--val_, *this); }

    constexpr auto& operator+=(difference_type const n) noexcept
    { return (val_ += n, *this); }

    constexpr auto& operator-=(difference_type const n) noexcept
    { return (val_ -= n, *this); }

    constexpr auto operator+(difference_type const n) const noexcept
    { return integral_iterator_{**this + n}; }

    constexpr auto operator-(difference_type const n) const noexcept
    { return integral_iterator_{**this - n}; }

    constexpr auto operator+() const noexcept
    { return integral_iterator_{+(**this)}; }

    constexpr auto operator-() const noexcept
    { return integral_iterator_{-(**this)}; }

    constexpr auto operator-(integral_iterator_ const x) const noexcept
    { return static_cast<difference_type>(**this - *x); }

private:
    constexpr friend bool operator==(integral_iterator_ const x,
                                     integral_iterator_ const y) noexcept
    { return (*x == *y); }

    constexpr friend bool operator<(integral_iterator_ const x,
                                    integral_iterator_ const y) noexcept
    { return (*x < *y); }

    value_type val_;
};

}     // namespace aux


template<typename T>
AMP_INLINE constexpr auto xrange(T const start, T const stop) noexcept
{
    return make_range(aux::integral_iterator_<T>{start},
                      aux::integral_iterator_<T>{stop});
}

template<typename T>
AMP_INLINE constexpr auto xrange(T const stop) noexcept
{
    return xrange(T{0}, stop);
}

}     // namespace amp


#endif  // AMP_INCLUDED_F5F0014B_E641_4B8B_9567_2770F8E4ED1B

