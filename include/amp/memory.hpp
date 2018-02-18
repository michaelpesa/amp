////////////////////////////////////////////////////////////////////////////////
//
// amp/memory.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_03A5A8C4_4A42_44B6_B9D6_950F09AFEEF6
#define AMP_INCLUDED_03A5A8C4_4A42_44B6_B9D6_950F09AFEEF6


#include <amp/bitops.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
# include <malloc.h>
#elif defined(__APPLE__) && defined(__MACH__)
# include <AvailabilityMacros.h>
# if (MAC_OS_X_VERSION_MIN_REQUIRED >= 1060)
#  include <stdlib.h>
#  define AMP_HAS_POSIX_MEMALIGN
#  if (MAC_OS_X_VERSION_MIN_REQUIRED < 1090)
#   define AMP_HAS_POSIX_MEMALIGN_ZERO_SIZE_BUG
#  endif
# endif
#elif defined(AMP_HAS_POSIX)
# include <unistd.h>
# if (_POSIX_C_SOURCE >= 200112L) || (_XOPEN_SOURCE >= 600)
#  include <stdlib.h>
#  define AMP_HAS_POSIX_MEMALIGN
# endif
#endif

#if !defined(_WIN32) && !defined(AMP_HAS_POSIX_MEMALIGN)
# include <memory>
#endif


namespace amp {
namespace mem {

[[nodiscard]]
AMP_INLINE void* aligned_alloc(std::size_t alignment,
                               std::size_t const size) noexcept
{
    AMP_ASSERT(is_pow2(alignment) && "alignment must be a power of 2");
    AMP_ASSUME(is_pow2(alignment));

    if (alignment < alignof(void*)) {
        alignment = alignof(void*);
    }

#if defined(_WIN32)
    return ::_aligned_malloc(alignment, size);
#elif defined(AMP_HAS_POSIX_MEMALIGN)
# if defined(AMP_HAS_POSIX_MEMALIGN_ZERO_SIZE_BUG)
    if (size == 0) {
        return nullptr;
    }
# endif
    void* p;
    if (::posix_memalign(&p, alignment, size) != 0) {
        p = nullptr;
    }
    return p;
#else
    auto space = size + alignment - alignof(void*);
    void* p1 = nullptr;
    void* p2 = std::malloc(space + sizeof(void*));

    if (p2 != nullptr) {
        p1 = static_cast<char*>(p2) + sizeof(void*);
        std::align(alignment, size, p1, space);
        *(static_cast<void**>(p1) - 1) = p2;
    }
    return p1;
#endif
}

AMP_INLINE void aligned_free(void* const p) noexcept
{
#if defined(_WIN32)
    ::aligned_free(p);
#elif defined(AMP_HAS_POSIX_MEMALIGN)
    std::free(p);
#else
    if (p != nullptr) {
        std::free(*(static_cast<void**>(p) - 1));
    }
#endif
}


template<typename T, typename Size>
AMP_READONLY
AMP_INLINE int compare(T const* const p1, T const* const p2,
                       Size const n) noexcept
{
    static_assert(is_trivially_copyable_v<T> && is_integral_v<Size>, "");
    return std::memcmp(p1, p2, static_cast<std::size_t>(n) * sizeof(T));
}

template<typename T, typename Size>
AMP_READONLY
AMP_INLINE int compare(T const* const p1, Size const n1,
                       T const* const p2, Size const n2) noexcept
{
    auto ret = mem::compare(p1, p2, std::min(n1, n2));
    if (ret == 0) {
        ret = int{n1 > n2} - int{n1 < n2};
    }
    return ret;
}

template<typename T, std::size_t N>
AMP_READONLY
AMP_INLINE int compare(T const(&a1)[N], T const(&a2)[N]) noexcept
{
    return mem::compare(a1, a2, N);
}


template<typename T, typename Size>
AMP_READONLY
AMP_INLINE bool equal(T const* const p1, T const* const p2,
                      Size const n) noexcept
{
    return (mem::compare(p1, p2, n) == 0);
}

template<typename T, typename Size>
AMP_READONLY
AMP_INLINE bool equal(T const* const p1, Size const n1,
                      T const* const p2, Size const n2) noexcept
{
    return (n1 == n2) && (mem::compare(p1, p2, n1) == 0);
}

template<typename T, std::size_t N>
AMP_READONLY
AMP_INLINE bool equal(T const(&a1)[N], T const(&a2)[N]) noexcept
{
    return (mem::compare(a1, a2, N) == 0);
}

}}    // namespace amp::mem


#endif  // AMP_INCLUDED_03A5A8C4_4A42_44B6_B9D6_950F09AFEEF6

