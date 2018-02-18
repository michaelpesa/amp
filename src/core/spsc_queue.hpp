////////////////////////////////////////////////////////////////////////////////
//
// core/spsc_queue.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_42D15164_A82B_429E_82CE_95E6D303E262
#define AMP_INCLUDED_42D15164_A82B_429E_82CE_95E6D303E262


#include <amp/aux/compressed_pair.hpp>
#include <amp/functional.hpp>
#include <amp/optional.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>


namespace amp {
namespace spsc {

// -- Overview --
//
// Unbounded lock-free queue that is safe for a single producer and a single
// consumer. Its implementation consists of a basic singly-linked list of the
// enqueued nodes, along with a cache of reusable nodes.
//
//
// -- Features --
//
// - The queue can store types that are neither copyable or movable. They must
//   be constructed inplace via queue::emplace, and can be manipulated inplace
//   via queue::front before being removed with queue::pop(std::ignore).
//
// - queue::push is overloaded to accept an iterator pair [first, last). This
//   is preferred over pushing each element individually, as the overload
//   will perform only one atomic store.
//
//
// -- Variables --
//
// -        tail: Back of the queue. This is only accessed by the producer
//                thread and, as such, is not an atomic variable.
//
// - before_head: Sentinel node before the front of the queue. This is only
//                written to by the consumer thread but is read by both, so
//                it is loaded with std::memory_order_consume by the producer
//                while the consumer only needs std::memory_order_relaxed.
//
// -  cache_tail: Cached copy of `before_head' that is only manipulated by the
//                producer thread. Whenever this node equals `cache', it is
//                synced with the current contents of `before_head' to avoid
//                needless atomic operations and memory fences.
//
// -  cache_head: Top of the cache cache. This is safe to reuse instead of
//                allocating a new node so long as it's at least one node
//                behind `before_head'.
//
//
// -- Invariants --
//
// - Except in the queue's constructor and destructor, the queue's member
//    variables (listed above) are never equal to nullptr.
//
// - cache_tail always points somewhere in [cache, before_head].
//
//
// -- Caveats --
//
// - Pointers obtained from spsc::queue::front are invalidated immediately
//   after either overload of spsc::queue::pop is called.
//
// - Likewise, spsc::queue::for_each assumes that its function argument will
//   not call spsc::queue::pop.
//
//
// -- Node Layout --
//
//  [~] -> [~] -> [~] -> [~] -> [!] -> [!] -> [!] -> [!] -> nil
//   \____________________/      \____________________/
//       node cache                 enqueued nodes
//
// - `cache_tail' is a cached copy of before_head, so it always points
//   somewhere in the range [cache, before_head] and is updated as needed.


template<typename T, typename Alloc = std::allocator<T>>
class queue
{
private:
    struct node_type
    {
        node_type() = delete;
        ~node_type() = delete;

        T data;
        std::atomic<node_type*> next;
    };

    using alloc_traits      = std::allocator_traits<Alloc>;
    using node_allocator    = typename alloc_traits::
                              template rebind_alloc<node_type>;
    using node_alloc_traits = typename alloc_traits::
                              template rebind_traits<node_type>;

    explicit queue(node_allocator&& a) :
        tail_(node_alloc_traits::allocate(a, 1)),
        before_head_(tail_),
        cache_tail_(tail_),
        cache_head_and_alloc_(tail_, std::move(a))
    {
        node_alloc_traits::construct(alloc_(), std::addressof(tail_->next),
                                     nullptr);
    }

public:
    using allocator_type  = Alloc;
    using value_type      = T;
    using pointer         = typename alloc_traits::pointer;
    using const_pointer   = typename alloc_traits::const_pointer;
    using size_type       = typename alloc_traits::size_type;
    using difference_type = typename alloc_traits::difference_type;

    explicit queue(allocator_type const& a = allocator_type()) :
        queue(node_allocator(a))
    {}

    queue(queue const&) = delete;
    queue& operator=(queue const&) = delete;

    ~queue()
    {
        auto first = cache_head_();
        auto const pivot = head_();

        // Deallocate cached nodes.
        while (first != pivot) {
            auto const x = queue::advance_(first);
            node_alloc_traits::deallocate(alloc_(), x, 1);
        }

        // Destroy and deallocate remaining nodes.
        while (first != nullptr) {
            auto const x = queue::advance_(first);
            node_alloc_traits::destroy(alloc_(), std::addressof(x->data));
            node_alloc_traits::deallocate(alloc_(), x, 1);
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // Producer functions
    ////////////////////////////////////////////////////////////////////////////

    template<typename... Args>
    void emplace(Args&&... args)
    {
        auto const x = make_node_(std::forward<Args>(args)...);
        tail_->next.store(x, std::memory_order_release);
        tail_ = x;
    }

    void push(value_type&& v)
    {
        emplace(std::move(v));
    }

    void push(value_type const& v)
    {
        emplace(v);
    }

    template<typename InIt>
    void push(InIt first, InIt const last)
    {
        if (first == last) {
            return;
        }

        auto list_head = make_node_(*first);
        auto list_tail = list_head;

        try {
            while (++first != last) {
                auto const x = make_node_(*first);
                list_tail->next.store(x, std::memory_order_relaxed);
                list_tail = x;
            }
        }
        catch (...) {
            while (list_head != nullptr) {
                auto const x = queue::advance_(list_head);
                node_alloc_traits::destroy(alloc_(), std::addressof(x->data));
                node_alloc_traits::deallocate(alloc_(), x, 1);
            }
            throw;
        }

        tail_->next.store(list_head, std::memory_order_release);
        tail_ = list_tail;
    }

    ////////////////////////////////////////////////////////////////////////////
    // Consumer functions
    ////////////////////////////////////////////////////////////////////////////

    void clear() noexcept
    {
        auto last = before_head_.load(std::memory_order_relaxed);
        while (auto const next = last->next.load(std::memory_order_consume)) {
            node_alloc_traits::destroy(alloc_(), std::addressof(next->data));
            last = next;
        }
        before_head_.store(last, std::memory_order_relaxed);
    }

    template<typename U>
    bool pop(U&& out)
    noexcept(is_nothrow_assignable_v<U, T&&>)
    {
        if (auto const x = head_()) {
            out = std::move(x->data);
            node_alloc_traits::destroy(alloc_(), std::addressof(x->data));
            before_head_.store(x, std::memory_order_release);
            return true;
        }
        return false;
    }

    optional<T> pop()
    noexcept(is_nothrow_move_constructible_v<T>)
    {
        if (auto const x = head_()) {
            optional<T> ret{std::move(x->data)};
            node_alloc_traits::destroy(alloc_(), std::addressof(x->data));
            before_head_.store(x, std::memory_order_release);
            return ret;
        }
        return nullopt;
    }

    [[carries_dependency]]
    pointer front() noexcept
    {
        auto const x = head_();
        return x ? std::addressof(x->data) : nullptr;
    }

    [[carries_dependency]]
    const_pointer front() const noexcept
    {
        auto const x = head_();
        return x ? std::addressof(x->data) : nullptr;
    }

    bool empty() const noexcept
    {
        return std::kill_dependency(head_() == nullptr);
    }

    template<typename F>
    size_type for_each(F&& f)
    noexcept(is_nothrow_invocable_v<F, T&>)
    {
        auto count = size_type{0};
        auto first = front();

        for (; first != nullptr; first = front(), void(), ++count) {
            std::invoke(f, *first);
            pop(std::ignore);
        }
        return count;
    }

    bool is_lock_free() const noexcept
    { return before_head_.is_lock_free(); }

    allocator_type get_allocator() const
    { return allocator_type(alloc_()); }

    size_type max_size() const noexcept
    { return node_alloc_traits::max_size(alloc_()); }

private:
    [[carries_dependency]]
    node_type* head_() const noexcept
    {
        auto const x = before_head_.load(std::memory_order_relaxed);
        AMP_ASSERT(x && "'before_head' node is never null");
        return x->next.load(std::memory_order_consume);
    }

    template<typename... Args>
    node_type* make_node_(Args&&... args)
    {
        if (cache_tail_ != cache_head_()) {
remove_from_cache:
            auto const x = cache_head_();
            node_alloc_traits::construct(alloc_(), std::addressof(x->data),
                                         std::forward<Args>(args)...);

            // Unlink 'x' from the queue before resetting its 'next' pointer.
            queue::advance_(cache_head_());
            node_alloc_traits::construct(alloc_(), std::addressof(x->next),
                                         nullptr);
            return x;
        }

        cache_tail_ = before_head_.load(std::memory_order_consume);
        if (cache_tail_ != cache_head_()) {
            goto remove_from_cache;
        }

        auto const x = node_alloc_traits::allocate(alloc_(), 1);
        try {
            node_alloc_traits::construct(alloc_(), std::addressof(x->data),
                                         std::forward<Args>(args)...);
            node_alloc_traits::construct(alloc_(), std::addressof(x->next),
                                         nullptr);
        }
        catch (...) {
            node_alloc_traits::deallocate(alloc_(), x, 1);
            throw;
        }
        return x;
    }

    AMP_INLINE static node_type* advance_(node_type*& x) noexcept
    { return std::exchange(x, x->next.load(std::memory_order_relaxed)); }

    AMP_INLINE node_type* cache_head_() const& noexcept
    { return cache_head_and_alloc_.first(); }

    AMP_INLINE node_type*& cache_head_() & noexcept
    { return cache_head_and_alloc_.first(); }

    AMP_INLINE node_allocator const& alloc_() const& noexcept
    { return cache_head_and_alloc_.second(); }

    AMP_INLINE node_allocator& alloc_() & noexcept
    { return cache_head_and_alloc_.second(); }

    node_type* tail_;
    std::atomic<node_type*> before_head_;
    node_type* cache_tail_;
    compressed_pair<node_type*, node_allocator> cache_head_and_alloc_;
};

}}    // namespace amp::spsc


#endif  // AMP_INCLUDED_42D15164_A82B_429E_82CE_95E6D303E262

