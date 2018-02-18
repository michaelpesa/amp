////////////////////////////////////////////////////////////////////////////////
//
// amp/functional.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_1621EAE7_62AA_478E_A5CA_402F7897ACC3
#define AMP_INCLUDED_1621EAE7_62AA_478E_A5CA_402F7897ACC3


#include <amp/aux/operators.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>


namespace amp {
namespace aux {

template<typename F, typename... Args>
using invoke_of_ =
    decltype(std::invoke(std::declval<F>(), std::declval<Args>()...));

template<typename, typename F, typename... Args>
struct invoke_result_ {};

template<typename F, typename... Args>
struct invoke_result_<void_t<invoke_of_<F, Args...>>, F, Args...>
{
    using type = invoke_of_<F, Args...>;
};

template<typename, typename F, typename... Args>
struct is_invocable_ : std::false_type {};
template<typename F, typename... Args>
struct is_invocable_<void_t<invoke_of_<F, Args...>>, F, Args...> :
    std::true_type
{};

template<typename, typename R, typename F, typename... Args>
struct is_invocable_r_ : std::false_type {};
template<typename R, typename F, typename... Args>
struct is_invocable_r_<void_t<invoke_of_<F, Args...>>, R, F, Args...> :
    std::is_convertible<invoke_of_<F, Args...>, R>
{};

template<typename, typename F, typename... Args>
struct is_nothrow_invocable_ : std::false_type {};
template<typename F, typename... Args>
struct is_nothrow_invocable_<void_t<invoke_of_<F, Args...>>, F, Args...> :
    bool_constant<
        noexcept(std::invoke(std::declval<F>(), std::declval<Args>()...))
    >
{};

template<typename, typename R, typename F, typename... Args>
struct is_nothrow_invocable_r_ : std::false_type {};
template<typename R, typename F, typename... Args>
struct is_nothrow_invocable_r_<void_t<invoke_of_<F, Args...>>, R, F, Args...> :
    bool_constant<
        is_convertible_v<invoke_of_<F, Args...>, R> &&
        noexcept(std::invoke(std::declval<F>(), std::declval<Args>()...))
    >
{};

}     // namespace aux

template<typename F, typename... Args>
struct invoke_result : aux::invoke_result_<void, F, Args...> {};
template<typename F, typename... Args>
using invoke_result_t = typename invoke_result<F, Args...>::type;

template<typename F, typename... Args>
struct is_invocable : aux::is_invocable_<void, F, Args...> {};
template<typename R, typename F, typename... Args>
struct is_invocable_r : aux::is_invocable_r_<void, R, F, Args...> {};

template<typename F, typename... Args>
struct is_nothrow_invocable : aux::is_nothrow_invocable_<void, F, Args...> {};
template<typename R, typename F, typename... Args>
struct is_nothrow_invocable_r :
    aux::is_nothrow_invocable_r_<void, R, F, Args...> {};

template<typename F, typename... Args>
constexpr bool is_invocable_v = is_invocable<F, Args...>::value;
template<typename R, typename F, typename... Args>
constexpr bool is_invocable_r_v = is_invocable_r<R, F, Args...>::value;

template<typename F, typename... Args>
constexpr bool is_nothrow_invocable_v =
    is_nothrow_invocable<F, Args...>::value;
template<typename R, typename F, typename... Args>
constexpr bool is_nothrow_invocable_r_v =
    is_nothrow_invocable_r<R, F, Args...>::value;


template<typename>
class function_view;

template<typename Ret, typename... Args>
class function_view<Ret(Args...)> :
    private null_comparable<function_view<Ret(Args...)>, std::nullptr_t>
{
public:
    constexpr function_view() noexcept :
        func_{nullptr},
        data_{nullptr}
    {}

    constexpr function_view(Ret (*const f)(void*, Args...),
                            void* const d) noexcept :
        func_{f},
        data_{d}
    {}

    template<
        typename F,
        typename = enable_if_t<
            !is_same_v<function_view, decay_t<F>> &&
            !is_base_of_v<function_view, decay_t<F>> &&
            is_invocable_v<F(Args...)>
        >
    >
    constexpr function_view(F&& f) noexcept :
        func_{function_view::thunk_<remove_reference_t<F>>},
        data_{const_cast<decay_t<F>*>(std::addressof(f))}
    {}

    constexpr explicit operator bool() const noexcept
    { return (func_ != nullptr); }

    AMP_INLINE Ret operator()(Args... args) const
    {
        AMP_ASSERT(*this && "attempted to invoke a null function_view");
        return (*func_)(data_, std::forward<Args>(args)...);
    }

    void swap(function_view& x) noexcept
    {
        using std::swap;
        swap(func_, x.func_);
        swap(data_, x.data_);
    }

    // Delegates hold a pointer to a proxy function, not the actual target
    // function itself; as such, they cannot be meaningfully compared.
    template<typename Ret2, typename... Args2>
    bool operator==(function_view<Ret2(Args2...)> const&) const = delete;
    template<typename Ret2, typename... Args2>
    bool operator!=(function_view<Ret2(Args2...)> const&) const = delete;

private:
    template<typename F>
    AMP_INLINE static Ret thunk_(void* const p, Args... args)
    { return std::invoke(*static_cast<F*>(p), std::forward<Args>(args)...); }

    Ret (*func_)(void*, Args...);
    void* data_;
};

template<typename Ret, typename... Args>
inline void swap(function_view<Ret(Args...)>& x,
                 function_view<Ret(Args...)>& y) noexcept
{
    x.swap(y);
}

}     // namespace amp


#endif  // AMP_INCLUDED_1621EAE7_62AA_478E_A5CA_402F7897ACC3

