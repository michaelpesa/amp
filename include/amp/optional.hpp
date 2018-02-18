////////////////////////////////////////////////////////////////////////////////
//
// amp/optional.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_ED80FEE9_3B48_46F6_9371_DE1EE367805F
#define AMP_INCLUDED_ED80FEE9_3B48_46F6_9371_DE1EE367805F


#include <amp/aux/operators.hpp>
#include <amp/error.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <new>
#include <utility>


namespace amp {

struct in_place_t {};
constexpr in_place_t in_place{};

struct nullopt_t
{
    enum class construct_token_t {};
    constexpr explicit nullopt_t(nullopt_t::construct_token_t) {}
};
constexpr nullopt_t nullopt{nullopt_t::construct_token_t()};


namespace aux {

template<typename T, bool = is_trivially_destructible_v<T>>
struct optional_destruct_
{
    using value_type = T;
    union {
        char nil_;
        value_type val_;
    };
    bool engaged_;

    constexpr optional_destruct_() noexcept :
        nil_(),
        engaged_(false)
    {}

    template<typename... Args>
    constexpr explicit optional_destruct_(in_place_t, Args&&... args)
    noexcept(is_nothrow_constructible_v<T, Args...>) :
        val_(std::forward<Args>(args)...),
        engaged_(true)
    {}

    optional_destruct_(optional_destruct_ const& x)
    noexcept(is_nothrow_copy_constructible_v<T>) :
        engaged_(x.engaged_)
    {
        if (engaged_) {
            ::new(std::addressof(val_)) T(x.val_);
        }
    }

    optional_destruct_(optional_destruct_&& x)
    noexcept(is_nothrow_move_constructible_v<T>) :
        engaged_(x.engaged_)
    {
        if (engaged_) {
            ::new(std::addressof(val_)) T(std::move(x.val_));
        }
    }

    ~optional_destruct_()
    {
        if (engaged_) {
            val_.~value_type();
        }
    }

    void reset() noexcept
    {
        if (engaged_) {
            val_.~value_type();
            engaged_ = false;
        }
    }
};

template<typename T>
struct optional_destruct_<T, true>
{
    using value_type = T;
    union {
        char nil_;
        value_type val_;
    };
    bool engaged_;

    constexpr optional_destruct_() noexcept :
        nil_(),
        engaged_(false)
    {}

    template<typename... Args>
    constexpr explicit optional_destruct_(in_place_t, Args&&... args)
    noexcept(is_nothrow_constructible_v<T, Args...>) :
        val_(std::forward<Args>(args)...),
        engaged_(true)
    {}

    optional_destruct_(optional_destruct_ const& x)
    noexcept(is_nothrow_copy_constructible_v<T>) :
        engaged_(x.engaged_)
    {
        if (engaged_) {
            ::new(std::addressof(val_)) T(x.val_);
        }
    }

    optional_destruct_(optional_destruct_&& x)
    noexcept(is_nothrow_move_constructible_v<T>) :
        engaged_(x.engaged_)
    {
        if (engaged_) {
            ::new(std::addressof(val_)) T(std::move(x.val_));
        }
    }

    void reset() noexcept
    { engaged_ = false; }
};

}     // namespace aux


template<typename T>
class optional :
    private aux::optional_destruct_<T>,
    private totally_ordered<optional<T>>,
    private totally_ordered<optional<T>, T>,
    private null_comparable<optional<T>, nullopt_t>
{
    using base_type = aux::optional_destruct_<T>;
    using base_type::val_;
    using base_type::engaged_;

public:
    using value_type = T;

    static_assert(!is_same_v<remove_cv_t<value_type>, in_place_t>,
                  "optional<in_place_t> is ill-formed");
    static_assert(!is_same_v<remove_cv_t<value_type>, nullopt_t>,
                  "optional<nullopt_t> is ill-formed");
    static_assert(!is_reference_v<value_type>,
                  "optional<T>: T must not be a reference type");
    static_assert(is_object_v<value_type>,
                  "optional<T>: T must be an object type");
    static_assert(is_nothrow_destructible_v<value_type>,
                  "optional<T>: T must have a non-throwing destructor");

    constexpr optional(nullopt_t) noexcept {}
    constexpr optional() = default;
    constexpr optional(optional const&) = default;
    constexpr optional(optional&&) = default;

    constexpr optional(value_type const& v)
    noexcept(is_nothrow_copy_constructible_v<T>) :
        base_type(in_place, v)
    {}

    constexpr optional(value_type&& v)
    noexcept(is_nothrow_move_constructible_v<T>) :
        base_type(in_place, std::move(v))
    {}

    template<
        typename... Args,
        typename = enable_if_t<is_constructible_v<T, Args...>>
    >
    constexpr explicit optional(in_place_t, Args&&... args)
    noexcept(is_nothrow_constructible_v<T, Args...>) :
        base_type(in_place, std::forward<Args>(args)...)
    {}

    template<
        typename E,
        typename... Args,
        typename IList = std::initializer_list<E>,
        typename = enable_if_t<is_constructible_v<T, IList&, Args...>>
    >
    constexpr explicit optional(in_place_t, IList ilist, Args&&... args)
    noexcept(is_nothrow_constructible_v<T, IList&, Args...>) :
        base_type(in_place, ilist, std::forward<Args>(args)...)
    {}

    optional& operator=(nullopt_t) & noexcept
    {
        reset();
        return *this;
    }

    optional& operator=(optional const& x) &
    noexcept(is_nothrow_copy_assignable_v<T> &&
             is_nothrow_copy_constructible_v<T>)
    {
        optional(x).swap(*this);
        return *this;
    }

    optional& operator=(optional&& x)
    noexcept(is_nothrow_move_assignable_v<T> &&
             is_nothrow_move_constructible_v<T>)
    {
        optional(std::move(x)).swap(*this);
        return *this;
    }

    template<
        typename U,
        typename = enable_if_t<
            is_same_v<remove_reference_t<U>, T> &&
            is_constructible_v<T, U> &&
            is_assignable_v<T&, U>
        >
    >
    optional& operator=(U&& u) &
    noexcept(is_nothrow_constructible_v<T, U> &&
             is_nothrow_assignable_v<T&, U>)
    {
        if (has_value()) {
            val_ = std::forward<U>(u);
        }
        else {
            construct_(std::forward<U>(u));
        }
        return *this;
    }

    template<
        typename... Args,
        typename = enable_if_t<is_constructible_v<T, Args...>>
    >
    void emplace(Args&&... args)
    noexcept(is_nothrow_constructible_v<T, Args...>)
    {
        reset();
        construct_(std::forward<Args>(args)...);
    }

    template<
        typename E,
        typename... Args,
        typename IList = std::initializer_list<E>,
        typename = enable_if_t<is_constructible_v<T, IList&, Args...>>
    >
    void emplace(IList ilist, Args&&... args)
    noexcept(is_nothrow_constructible_v<T, IList&, Args...>)
    {
        reset();
        construct_(ilist, std::forward<Args>(args)...);
    }

    void swap(optional& x)
    noexcept(is_nothrow_move_constructible_v<T> && is_nothrow_swappable_v<T>)
    {
        static_assert(is_move_constructible_v<T>,
                      "optional<T>::swap: T must be move constructible");
        static_assert(is_swappable_v<T>,
                      "optional<T>::swap: T must be swappable");

        if (has_value() == x.has_value()) {
            if (has_value()) {
                using std::swap;
                swap(val_, x.val_);
            }
        }
        else if (has_value()) {
            x.construct_(std::move(val_));
            reset();
        }
        else {
            construct_(std::move(x.val_));
            x.reset();
        }
    }

    using base_type::reset;

    constexpr bool has_value() const noexcept
    { return base_type::engaged_; }

    constexpr explicit operator bool() const noexcept
    { return has_value(); }

    constexpr T& operator*() & noexcept
    {
        AMP_ASSERT(has_value() && "dereferenced a null optional");
        return val_;
    }

    constexpr T const& operator*() const& noexcept
    {
        AMP_ASSERT(has_value() && "dereferenced a null optional");
        return val_;
    }

    constexpr T&& operator*() && noexcept
    {
        AMP_ASSERT(has_value() && "dereferenced a null optional");
        return std::move(val_);
    }

    constexpr T const&& operator*() const&& noexcept
    {
        AMP_ASSERT(has_value() && "dereferenced a null optional");
        return std::move(val_);
    }

    constexpr T* operator->() noexcept
    {
        AMP_ASSERT(has_value() && "dereferenced a null optional");
        return std::addressof(val_);
    }

    constexpr T const* operator->() const noexcept
    {
        AMP_ASSERT(has_value() && "dereferenced a null optional");
        return std::addressof(val_);
    }

    constexpr value_type& value() &
    {
        if (has_value()) {
            return val_;
        }
        raise(errc::object_disposed);
    }

    constexpr value_type const& value() const&
    {
        if (has_value()) {
            return val_;
        }
        raise(errc::object_disposed);
    }

    constexpr value_type&& value() &&
    {
        if (has_value()) {
            return std::move(val_);
        }
        raise(errc::object_disposed);
    }

    constexpr value_type const&& value() const&&
    {
        if (has_value()) {
            return std::move(val_);
        }
        raise(errc::object_disposed);
    }

    template<typename U = T>
    constexpr value_type value_or(U&& u) const&
    noexcept(is_nothrow_copy_constructible_v<T> &&
             is_nothrow_constructible_v<T, U>)
    {
        static_assert(is_copy_constructible_v<T>,
                      "optional<T>::value_or: T must be copy constructible");
        static_assert(is_convertible_v<U, T>,
                      "optional<T>::value_or: U must be convertible to T");

        return has_value()
             ? val_
             : static_cast<T>(std::forward<U>(u));
    }

    template<typename U = T>
    constexpr value_type value_or(U&& u) &&
    noexcept(is_nothrow_move_constructible_v<T> &&
             is_nothrow_constructible_v<T, U>)
    {
        static_assert(is_move_constructible_v<T>,
                      "optional<T>::value_or: T must be move constructible");
        static_assert(is_convertible_v<U, T>,
                      "optional<T>::value_or: U must be convertible to T");

        return has_value()
             ? std::move(val_)
             : static_cast<T>(std::forward<U>(u));
    }

private:
    template<typename... Args>
    void construct_(Args&&... args)
    noexcept(is_nothrow_constructible_v<T, Args...>)
    {
        AMP_ASSERT(!has_value());

        auto const p = static_cast<void*>(std::addressof(val_));
        ::new(p) T(std::forward<Args>(args)...);
        engaged_ = true;
    }

    constexpr friend bool operator==(optional const& x, optional const& y)
    noexcept(noexcept(*x == *y))
    { return ((x && y) || (x || y)) && (!x || *x == *y); }

    constexpr friend bool operator<(optional const& x, optional const& y)
    noexcept(noexcept(*x < *y))
    { return y && (!x || *x < *y); }

    constexpr friend bool operator==(optional const& x, T const& y)
    noexcept(noexcept(*x == y))
    { return x && (*x == y); }

    constexpr friend bool operator<(optional const& x, T const& y)
    noexcept(noexcept(*x < y))
    { return !x || (*x < y); }

    constexpr friend bool operator>(optional const& x, T const& y)
    noexcept(noexcept(y < *x))
    { return x && (y < *x); }
};


template<typename T>
inline void swap(optional<T>& x, optional<T>& y)
noexcept(noexcept(x.swap(y)))
{
    x.swap(y);
}


template<typename T>
constexpr auto make_optional(T&& t)
noexcept(is_nothrow_constructible_v<optional<decay_t<T>>, T>)
{
    return optional<decay_t<T>>(std::forward<T>(t));
}

}     // namespace amp


namespace std {

template<typename T>
struct hash<amp::optional<T>>
{
    using argument_type = amp::optional<T>;
    using result_type   = std::size_t;

    constexpr result_type operator()(argument_type const& x) const noexcept
    { return x ? std::hash<T>()(*x) : 0; }
};

}     // namespace std


#endif  // AMP_INCLUDED_ED80FEE9_3B48_46F6_9371_DE1EE367805F

