////////////////////////////////////////////////////////////////////////////////
//
// amp/ref_ptr.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_B44C358C_ECFC_4F09_A3AE_7C489B05186D
#define AMP_INCLUDED_B44C358C_ECFC_4F09_A3AE_7C489B05186D


#include <amp/aux/operators.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <atomic>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>


namespace amp {

constexpr struct acquire_ref_t {} acquire_ref{};
constexpr struct consume_ref_t {} consume_ref{};


template<typename T>
class ref_ptr :
    private totally_ordered<ref_ptr<T>>,
    private null_comparable<ref_ptr<T>, std::nullptr_t>
{
public:
    constexpr ref_ptr() noexcept :
        ptr_{nullptr}
    {}

    constexpr ref_ptr(std::nullptr_t) noexcept :
        ptr_{nullptr}
    {}

    explicit ref_ptr(T* const p, acquire_ref_t) noexcept :
        ptr_{p}
    {
        if (ptr_) {
            ptr_->add_ref();
        }
    }

    explicit ref_ptr(T* const p, consume_ref_t) noexcept :
        ptr_{p}
    {}

    ref_ptr(ref_ptr const& x) noexcept :
        ptr_{x.get()}
    {
        if (ptr_) {
            ptr_->add_ref();
        }
    }

    template<typename U, typename = enable_if_t<is_convertible_v<U*, T*>>>
    ref_ptr(ref_ptr<U> const& x) noexcept :
        ptr_{x.get()}
    {
        if (ptr_) {
            ptr_->add_ref();
        }
    }

    ref_ptr(ref_ptr&& x) noexcept :
        ptr_{x.release()}
    {}

    template<typename U, typename = enable_if_t<is_convertible_v<U*, T*>>>
    ref_ptr(ref_ptr<U>&& x) noexcept :
        ptr_{x.release()}
    {}

    ~ref_ptr()
    {
        if (ptr_) {
            ptr_->release();
        }
    }

    ref_ptr& operator=(std::nullptr_t) & noexcept
    {
        ref_ptr{nullptr}.swap(*this);
        return *this;
    }

    ref_ptr& operator=(ref_ptr const& x) & noexcept
    {
        ref_ptr{x}.swap(*this);
        return *this;
    }

    template<typename U, typename = enable_if_t<is_convertible_v<U*, T*>>>
    ref_ptr& operator=(ref_ptr<U> const& x) & noexcept
    {
        ref_ptr{x}.swap(*this);
        return *this;
    }

    ref_ptr& operator=(ref_ptr&& x) & noexcept
    {
        ref_ptr{std::move(x)}.swap(*this);
        return *this;
    }

    template<typename U, typename = enable_if_t<is_convertible_v<U*, T*>>>
    ref_ptr& operator=(ref_ptr<U>&& x) & noexcept
    {
        ref_ptr{std::move(x)}.swap(*this);
        return *this;
    }

    void swap(ref_ptr& x) noexcept
    {
        using std::swap;
        swap(ptr_, x.ptr_);
    }

    explicit operator bool() const noexcept
    { return (get() != nullptr); }

    T* get() const noexcept
    { return ptr_; }

    T* operator->() const noexcept
    { return get(); }

    T& operator*() const noexcept
    { return *get(); }

    T* release() noexcept
    { return std::exchange(ptr_, nullptr); }

    T** addressof() noexcept
    { return &ptr_; }

    void reset(std::nullptr_t = nullptr) noexcept
    { ref_ptr{nullptr}.swap(*this); }

    void reset(T* const p, consume_ref_t) noexcept
    { ref_ptr{p, consume_ref}.swap(*this); }

    void reset(T* const p, acquire_ref_t) noexcept
    { ref_ptr{p, acquire_ref}.swap(*this); }

    static ref_ptr acquire(T* const p) noexcept
    { return ref_ptr{p, acquire_ref}; }

    static ref_ptr consume(T* const p) noexcept
    { return ref_ptr{p, consume_ref}; }

private:
    friend bool operator==(ref_ptr const& x, ref_ptr const& y) noexcept
    { return std::equal_to<T*>{}(x.get(), y.get()); }

    friend bool operator<(ref_ptr const& x, ref_ptr const& y) noexcept
    { return std::less<T*>{}(x.get(), y.get()); }

    T* ptr_;
};


template<typename T>
inline void swap(ref_ptr<T>& x, ref_ptr<T>& y) noexcept
{
    x.swap(y);
}


template<typename T, typename U>
inline ref_ptr<T> static_pointer_cast(ref_ptr<U> x) noexcept
{
    return ref_ptr<T>::consume(static_cast<T*>(x.release()));
}

template<typename T, typename U>
inline ref_ptr<T> const_pointer_cast(ref_ptr<U> x) noexcept
{
    return ref_ptr<T>::consume(const_cast<T*>(x.release()));
}

template<typename T, typename U>
inline ref_ptr<T> reinterpret_pointer_cast(ref_ptr<U> x) noexcept
{
    return ref_ptr<T>::consume(reinterpret_cast<T*>(x.release()));
}


template<typename T, typename Base>
class implement_ref_count :
    public Base
{
public:
    void add_ref() noexcept override
    {
        ref_count_.fetch_add(1, std::memory_order_relaxed);
    }

    void release() noexcept override
    {
        auto remain = ref_count_.fetch_sub(1, std::memory_order_release) - 1;
        if (!remain) {
            std::atomic_thread_fence(std::memory_order_acquire);
            delete static_cast<T*>(this);
        }
    }

    template<typename... Args>
    static auto make(Args&&... args)
    {
        return ref_ptr<Base>::consume(new T(std::forward<Args>(args)...));
    }

protected:
    implement_ref_count() = default;
    ~implement_ref_count() = default;

private:
    std::atomic<uint32> ref_count_{1};
};

}     // namespace amp


namespace std {

template<typename T>
struct hash<::amp::ref_ptr<T>>
{
    using argument_type = ::amp::ref_ptr<T>;
    using result_type   = ::std::size_t;

    result_type operator()(argument_type const& x) const noexcept
    { return ::std::hash<T*>{}(x.get()); }
};

}     // namespace std


#endif  // AMP_INCLUDED_B44C358C_ECFC_4F09_A3AE_7C489B05186D

