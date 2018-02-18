////////////////////////////////////////////////////////////////////////////////
//
// core/spin_mutex.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_9AA9DDFB_61E9_458F_8FAD_8B4D7C132AA7
#define AMP_INCLUDED_9AA9DDFB_61E9_458F_8FAD_8B4D7C132AA7


#include <amp/stddef.hpp>

#include <atomic>

#if defined(_MSC_VER)
# include <intrin.h>
# if defined(_M_IX86) || defined(_M_X64)
#  pragma intrinsic(_mm_pause)
# elif defined(_M_ARM) || defined(_M_ARM64) || defined(_M_IA64)
#  pragma intrinsic(__yield)
# endif
#elif defined(__ARM_ACLE)
# include <arm_acle.h>
#elif defined(__HP_aCC) && defined(__ia64)
# include <machine/sys/inline.h>
#endif


namespace amp {

// Pause for a brief period of time. In test-and-set / comare-and-swap loops,
// this decreases power consumption and improves performance by reducing memory
// ordering issues. Additionally, on architectures that implement simultaneous
// multithreading (e.g. Intel's hyper-threading, Sun's CMT, ...), this allows
// siblings of the blocked thread to execute.

AMP_INLINE void spin_pause() noexcept
{
#if defined(_MSC_VER)

    // From MSDN:
    //
    // x86 Intrinsics List
    // | ...
    // | _mm_pause    | void _mm_pause(void);
    //
    // ARM Support for Intrinsics from Other Architectures
    // | ...
    // | __yield  | void __yield(void);
    // |          | Note: On ARM platforms, this function generates the YIELD
    // |          | instruction, which indicates that the thread is performing
    // |          | a task that can be temporarily suspended from execution
    //
    // Windows no longer supports Itanium, but it is included here for
    // completeness (and because it uses the same intrinsic as ARM).

# if defined(_M_ARM64) || defined(_M_ARM) || defined(_M_IA64)
    __yield();
# elif defined(_M_X64) || defined(_M_IX86)
    _mm_pause();
# endif

#elif defined(__x86_64__) || defined(__i386)

    // From "Intel 64 and IA-32 Architectures Software Developer's Manual",
    // Vol.1, page 12, section 11 ("SSE2 Instructions"):
    //
    //     11.4.4.4 Pause
    //     The PAUSE instruction is provided to improve the performance of
    //     "spin-wait loops" ... It is recommended that a PAUSE instruction
    //     always be included in the code sequence for a spin-wait loop.

    asm volatile("pause");

# elif defined(__CC_ARM) || defined(__ARM_ACLE)

    __yield();

# elif defined(__aarch64__) || defined(__arm__)

    asm volatile("yield");

#elif defined(__sparc)

    // From the Linux kernel's <arch/sparc/include/asm/processor_64.h>.
    // This performs three dummy reads of the condition code register, each of
    // which blocks the current strand (hardware thread) for 40-50 cycles.

    asm volatile("rd %%ccr, %%g0\n\t"
                 "rd %%ccr, %%g0\n\t"
                 "rd %%ccr, %%g0");

#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)

    // Extracted from "Power ISA Version 2.06 Revision B":
    //
    //   "or" Shared Resource Hints
    // ...
    // or 27,27,27
    // |     This form of `or' provides a hint that performance will
    // |     probably be improved if shared resources dedicated to the
    // |     executing processor are released for use by other processors.

    asm volatile("or 27,27,27");

#elif defined(__HP_aCC) && defined(__ia64)

    // Extracted from "Inline Assembly for Itanium-based HP-UX":
    //
    // Table 1-27   _Asm_hint - legal hint operation type completers
    // typedef enum {
    //     _HINT_PAUSE = 0x0    /* pause */
    // } _Asm_hint_imm;
    //
    // Table 1-38   Miscellaneous Opcodes
    // ...
    // void _Asm_hint(_Asm_hint_imm,  [,_Asm_fence]);

    _Asm_hint(_HINT_PAUSE);

#elif defined(__itanium__) || defined(__ia64)

    // From "Intel Itanium Architecture Developer's Manual, Vol. 3", page 145:
    //
    // Table 2-31.  Hint Immediates
    // ------------------------------------------------------------------------
    // | 0x0 | @pause | Indicates to the processor that the currently executing
    // |     |        | stream is waiting, spinning, or performing low priority
    // |     |        | tasks. This hint can be used by the processor to
    // |     |        | allocate more resources or time to another executing
    // |     |        | stream on the same processor.

    asm volatile("hint @pause");

#endif
}


class spin_mutex
{
public:
    AMP_INLINE bool try_lock() noexcept
    {
        return !locked_.exchange(true, std::memory_order_acquire);
    }

    AMP_INLINE void lock() noexcept
    {
        do {
            while (locked_.load(std::memory_order_relaxed)) {
                spin_pause();
            }
        }
        while (!try_lock());
    }

    AMP_INLINE void unlock() noexcept
    {
        locked_.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool> locked_{false};
};

}     // namespace amp


// References
//
// MSVC
// http://msdn.microsoft.com/en-us/library/hh977023.aspx (x86)
// http://msdn.microsoft.com/en-us/library/hh875058.aspx (ARM)
//
// x86:
// http://stackoverflow.com/questions/7086220/what-does-rep-nop-mean-in-x86-assembly
//
// MIPS
// https://sourceware.org/ml/binutils/2011-12/msg00093.html
// http://courses.engr.illinois.edu/cs426/Resources/MIPS32BIS-AFP-03.02.pdf (page 201)
//
// Power
// https://www.power.org/documentation/power-instruction-set-architecture-version-2-06 (requires free account)
//
// Inline Assembly for Itanium-based HP-UX
// http://h21007.www2.hp.com/portal/download/files/unprot/Itanium/inline_assem_ERS.pdf
//
// Intel Itanium Architecture Developer's Manual, Vol. 3
// http://www.intel.com/content/www/us/en/processors/itanium/itanium-architecture-vol-3-manual.html


#endif  // AMP_INCLUDED_9AA9DDFB_61E9_458F_8FAD_8B4D7C132AA7

