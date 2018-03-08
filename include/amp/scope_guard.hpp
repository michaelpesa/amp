////////////////////////////////////////////////////////////////////////////////
//
// amp/scope_guard.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_8D141994_3E94_4130_B7EE_144B64F98BCD
#define AMP_INCLUDED_8D141994_3E94_4130_B7EE_144B64F98BCD


#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <utility>


namespace amp {

template<typename F>
class scope_guard
{
public:
    static_assert(is_nothrow_destructible_v<F>, "");
    static_assert(is_nothrow_move_constructible_v<F>, "");

    scope_guard(scope_guard const&) = delete;
    scope_guard& operator=(scope_guard&&) = delete;
    scope_guard& operator=(scope_guard const&) = delete;

    explicit scope_guard(F const& f)
    noexcept(is_nothrow_copy_constructible_v<F>) :
        func_(f)
    {}

    explicit scope_guard(F&& f)
    noexcept(is_nothrow_move_constructible_v<F>) :
        func_(std::move(f))
    {}

    scope_guard(scope_guard&& x) noexcept :
        func_(std::move(x.func_)),
        dismissed_(std::exchange(x.dismissed_, true))
    {}

    ~scope_guard()
    {
        if (!dismissed_) {
            func_();
        }
    }

    void dismiss() noexcept
    {
        dismissed_ = true;
    }

private:
    F func_;
    bool dismissed_{false};
};


namespace aux {

enum class scope_exit {};

template<typename F>
AMP_INLINE auto operator+(aux::scope_exit, F&& f) noexcept
{ return scope_guard<decay_t<F>>(std::forward<F>(f)); }

}}    // namespace amp::aux


#define AMP_SCOPE_EXIT                                                      \
    [[maybe_unused]] auto const& AMP_PP_ANON(amp_scope_exit_) =             \
        ::amp::aux::scope_exit() + [&]() noexcept


#endif  // AMP_INCLUDED_8D141994_3E94_4130_B7EE_144B64F98BCD

