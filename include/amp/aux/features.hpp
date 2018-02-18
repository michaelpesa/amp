////////////////////////////////////////////////////////////////////////////////
//
// amp/aux/features.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_09786A69_99C3_4FF0_8FEE_1DEF3F855A43
#define AMP_INCLUDED_09786A69_99C3_4FF0_8FEE_1DEF3F855A43


#if defined(_MSC_VER)
# ifndef _USE_ATTRIBUTES_FOR_SAL
# define _USE_ATTRIBUTES_FOR_SAL 1
# endif
# include <sal.h>
#endif


////////////////////////////////////////////////////////////////////////////////
// Preprocessor utilities.
////////////////////////////////////////////////////////////////////////////////

#define AMP_PP_CAT_AUX(a, b) a ## b
#define AMP_PP_CAT(a, b) AMP_PP_CAT_AUX(a, b)
#define AMP_PP_ANON(tag) AMP_PP_CAT(tag, __LINE__)


////////////////////////////////////////////////////////////////////////////////
// Feature detection.
////////////////////////////////////////////////////////////////////////////////

#ifndef __has_cpp_attribute
#define __has_cpp_attribute(x) 0
#endif
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif
#ifndef __has_feature
#define __has_feature(x) 0
#endif
#ifndef __has_warning
#define __has_warning(x) 0
#endif

#if defined(__GNUC__) && defined(__GNUC_MINOR__)
# define AMP_GCC_PREREQ(major, minor) \
    ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((major) << 16) + (minor))
#else
# define AMP_GCC_PREREQ(major, minor) false
#endif

#if defined(__unix) \
 || defined(__unix__) \
 || (defined(__APPLE__) && defined(__MACH__))
# define AMP_HAS_POSIX
#endif


////////////////////////////////////////////////////////////////////////////////
// Debug.
////////////////////////////////////////////////////////////////////////////////

#if defined(AMP_DEBUG)
# include <cassert>
# define AMP_ASSERT(...) assert(__VA_ARGS__)
#else
# define AMP_ASSERT(...)
#endif


////////////////////////////////////////////////////////////////////////////////
// Optimization hints.
////////////////////////////////////////////////////////////////////////////////

#if __has_builtin(__builtin_assume)
# define AMP_ASSUME(...) __builtin_assume(__VA_ARGS__)
#elif defined(_MSC_VER)
# define AMP_ASSUME(...) __assume(__VA_ARGS__)
#else
# define AMP_ASSUME(...)
#endif

#if __has_builtin(__builtin_unreachable) || AMP_GCC_PREREQ(4, 5)
# define AMP_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
# define AMP_UNREACHABLE() __assume(false)
#else
# define AMP_UNREACHABLE()
#endif

#if __has_builtin(__builtin_expect) || AMP_GCC_PREREQ(4, 0)
# define AMP_LIKELY(...)   __builtin_expect(!!(__VA_ARGS__), true)
# define AMP_UNLIKELY(...) __builtin_expect(!!(__VA_ARGS__), false)
#else
# define AMP_LIKELY(...)   !!(__VA_ARGS__)
# define AMP_UNLIKELY(...) !!(__VA_ARGS__)
#endif

// Avoid using __restrict when possible since Apple's sys/cdefs.h defines it
// to nothing when compiling as C++.
#if defined(__clang__) || defined(__GNUC__)
# define AMP_RESTRICT __restrict__
#elif defined(_MSC_VER)
# define AMP_RESTRICT __restrict
#else
# define AMP_RESTRICT
#endif


////////////////////////////////////////////////////////////////////////////////
// Attributes.
////////////////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
# if defined(AMP_CORE_BUILD)
#  define AMP_EXPORT __declspec(dllexport)
# else
#  define AMP_EXPORT __declspec(dllimport)
# endif
#elif __has_attribute(visibility) || AMP_GCC_PREREQ(4, 0)
# define AMP_EXPORT __attribute__((visibility("default")))
#else
# define AMP_EXPORT
#endif

#if __has_attribute(internal_linkage)
# define AMP_INTERNAL_LINKAGE __attribute__((internal_linkage, unused))
#else
# define AMP_INTERNAL_LINKAGE
#endif

#if __has_attribute(always_inline) || AMP_GCC_PREREQ(3, 1)
# define AMP_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
# define AMP_INLINE __forceinline
#else
# define AMP_INLINE inline
#endif

#if __has_attribute(noinline) || AMP_GCC_PREREQ(3, 1)
# define AMP_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
# define AMP_NOINLINE __declspec(noinline)
#else
# define AMP_NOINLINE
#endif

#if __has_attribute(format) || AMP_GCC_PREREQ(4, 4)
# define AMP_PRINTF_FORMAT(...) __attribute__((format(printf, __VA_ARGS__)))
#else
# define AMP_PRINTF_FORMAT(...)
#endif

#if defined(_MSC_VER)
# define AMP_PRINTF_FORMAT_STRING _Printf_format_string
#else
# define AMP_PRINTF_FORMAT_STRING
#endif

#if defined(__clang__) || AMP_GCC_PREREQ(2, 5)
# define AMP_READNONE __attribute__((const))
#elif defined(_MSC_VER)
# define AMP_READNONE __declspec(noalias)
#else
# define AMP_READNONE
#endif

#if __has_attribute(pure) || AMP_GCC_PREREQ(3, 0)
# define AMP_READONLY __attribute__((pure))
#else
# define AMP_READONLY
#endif

#if __has_attribute(target) || AMP_GCC_PREREQ(4, 9)
# define AMP_TARGET(...) __attribute__((target(__VA_ARGS__)))
#else
# define AMP_TARGET(...)
#endif


#endif  // AMP_INCLUDED_09786A69_99C3_4FF0_8FEE_1DEF3F855A43

