////////////////////////////////////////////////////////////////////////////////
//
// core/event.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_3870888E_1AA8_425C_BA52_53A71BD673A4
#define AMP_INCLUDED_3870888E_1AA8_425C_BA52_53A71BD673A4


#include <amp/error.hpp>
#include <amp/stddef.hpp>

#include <atomic>

#include <mach/mach_init.h>
#include <mach/mach_traps.h>
#include <mach/semaphore.h>
#include <mach/task.h>


namespace amp {
namespace aux {

class semaphore
{
public:
    semaphore(semaphore const&) = delete;
    semaphore& operator=(semaphore const&) = delete;

    explicit semaphore(int const init = 0)
    {
        auto const ret = ::semaphore_create(mach_task_self(), &sem_,
                                            SYNC_POLICY_FIFO, init);
        if (AMP_UNLIKELY(ret != KERN_SUCCESS)) {
            raise_bad_alloc();
        }
    }

    ~semaphore()
    {
        ::semaphore_destroy(mach_task_self(), sem_);
    }

    void wait()
    {
        auto const ret = ::semaphore_wait(sem_);
        AMP_ASSERT(ret == KERN_SUCCESS);
        (void)ret;
    }

    void post()
    {
        auto const ret = ::semaphore_signal(sem_);
        AMP_ASSERT(ret == KERN_SUCCESS);
        (void)ret;
    }

private:
    ::semaphore_t sem_;
};

}     // namespace aux


class auto_reset_event
{
public:
    explicit auto_reset_event(int const init = 0) :
        status_{init}
    {}

    void post()
    {
        int old_status = status_.load(std::memory_order_relaxed);
        while (!status_.compare_exchange_weak(
                old_status, (old_status < 1 ? old_status + 1 : 1),
                std::memory_order_release,
                std::memory_order_relaxed))
        {}

        if (old_status < 0) {
            sem_.post();
        }
    }

    void wait()
    {
        if (status_.fetch_sub(1, std::memory_order_acquire) < 1) {
            sem_.wait();
        }
    }

private:
    aux::semaphore sem_;
    std::atomic<int> status_;
};

}     // namespace amp


#endif  // AMP_INCLUDED_3870888E_1AA8_425C_BA52_53A71BD673A4

