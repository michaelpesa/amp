////////////////////////////////////////////////////////////////////////////////
//
// amp/aux/operators.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_2BD7F9A4_89AB_41AA_92D7_2CF380DEF83E
#define AMP_INCLUDED_2BD7F9A4_89AB_41AA_92D7_2CF380DEF83E


#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>


namespace amp {

template<typename T, typename U = T>
struct equality_comparable
{
    constexpr friend bool operator==(U const& x, T const& y)
    noexcept(noexcept(y == x))
    { return (y == x); }

    constexpr friend bool operator!=(U const& x, T const& y)
    noexcept(noexcept(!(y == x)))
    { return !(y == x); }

    constexpr friend bool operator!=(T const& x, U const& y)
    noexcept(noexcept(!(x == y)))
    { return !(x == y); }
};

template<typename T>
struct equality_comparable<T, T>
{
    constexpr friend bool operator!=(T const& x, T const& y)
    noexcept(noexcept(!(x == y)))
    { return !(x == y); }
};


template<typename T, typename U = T>
struct less_than_comparable
{
    constexpr friend bool operator<=(T const& x, U const& y)
    noexcept(noexcept(!(x > y)))
    { return !(x > y); }

    constexpr friend bool operator>=(T const& x, U const& y)
    noexcept(noexcept(!(x < y)))
    { return !(x < y); }

    constexpr friend bool operator>(U const& x, T const& y)
    noexcept(noexcept(y < x))
    { return (y < x); }

    constexpr friend bool operator<(U const& x, T const& y)
    noexcept(noexcept(y > x))
    { return (y > x); }

    constexpr friend bool operator<=(U const& x, T const& y)
    noexcept(noexcept(!(y < x)))
    { return !(y < x); }

    constexpr friend bool operator>=(U const& x, T const& y)
    noexcept(noexcept(!(y > x)))
    { return !(y > x); }
};

template<typename T>
struct less_than_comparable<T, T>
{
    constexpr friend bool operator>(T const& x, T const& y)
    noexcept(noexcept(y < x))
    { return (y < x); }

    constexpr friend bool operator<=(T const& x, T const& y)
    noexcept(noexcept(!(y < x)))
    { return !(y < x); }

    constexpr friend bool operator>=(T const& x, T const& y)
    noexcept(noexcept(!(x < y)))
    { return !(x < y); }
};


template<typename T, typename U = T>
struct totally_ordered :
    public equality_comparable<T, U>,
    public less_than_comparable<T, U>
{};


// Helper for classes that are comparable with a null object (for instance,
// 'unique_ptr<T, D> [OP] nullptr', or 'optional<T> [OP] nullopt').

template<typename T, typename Null>
struct null_comparable
{
    constexpr friend bool operator==(T const& x, Null)
    noexcept(noexcept(!x))
    { return !x; }

    constexpr friend bool operator==(Null, T const& x)
    noexcept(noexcept(!x))
    { return !x; }

    constexpr friend bool operator!=(T const& x, Null)
    noexcept(noexcept(static_cast<bool>(x)))
    { return static_cast<bool>(x); }

    constexpr friend bool operator!=(Null, T const& x)
    noexcept(noexcept(static_cast<bool>(x)))
    { return static_cast<bool>(x); }

    constexpr friend bool operator<(T const&, Null) noexcept
    { return false; }

    constexpr friend bool operator<(Null, T const& x)
    noexcept(noexcept(static_cast<bool>(x)))
    { return static_cast<bool>(x); }

    constexpr friend bool operator>(T const& x, Null)
    noexcept(noexcept(static_cast<bool>(x)))
    { return static_cast<bool>(x); }

    constexpr friend bool operator>(Null, T const&) noexcept
    { return false; }

    constexpr friend bool operator<=(T const& x, Null)
    noexcept(noexcept(!x))
    { return !x; }

    constexpr friend bool operator<=(Null, T const&) noexcept
    { return true; }

    constexpr friend bool operator>=(T const&, Null) noexcept
    { return true; }

    constexpr friend bool operator>=(Null, T const& x)
    noexcept(noexcept(!x))
    { return !x; }
};


template<typename T>
struct incrementable
{
    constexpr friend T operator++(T& x, int)
    noexcept(noexcept(++x) && is_nothrow_copy_constructible_v<T>)
    {
        auto ret = x;
        ++x;
        return ret;
    }
};

template<typename T>
struct decrementable
{
    constexpr friend T operator--(T& x, int)
    noexcept(noexcept(--x) && is_nothrow_copy_constructible_v<T>)
    {
        auto ret = x;
        --x;
        return ret;
    }
};


template<typename T>
struct output_iteratable :
    incrementable<T>
{};

template<typename T>
struct input_iteratable :
    equality_comparable<T>,
    incrementable<T>
{};

template<typename T>
struct forward_iteratable :
    input_iteratable<T>
{};

template<typename T>
struct bidirection_iteratable :
    forward_iteratable<T>,
    decrementable<T>
{};

}     // namespace amp


#endif  // AMP_INCLUDED_2BD7F9A4_89AB_41AA_92D7_2CF380DEF83E

