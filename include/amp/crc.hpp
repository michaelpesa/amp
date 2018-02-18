////////////////////////////////////////////////////////////////////////////////
//
// amp/crc.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_BE28D5F7_B210_4BAE_A7C7_89BE04F56880
#define AMP_INCLUDED_BE28D5F7_B210_4BAE_A7C7_89BE04F56880


#include <amp/stddef.hpp>

#include <cstddef>


namespace amp {

struct crc32
{
    AMP_EXPORT AMP_READONLY
    static uint32 update(void const*, std::size_t, uint32) noexcept;

    AMP_READONLY
    AMP_INLINE static uint32 compute(void const* const buf,
                                     std::size_t const len) noexcept
    { return ~update(buf, len, ~uint32{0}); }
};

struct crc32c
{
    AMP_EXPORT AMP_READONLY
    static uint32 update(void const*, std::size_t, uint32) noexcept;

    AMP_READONLY
    AMP_INLINE static uint32 compute(void const* const buf,
                                     std::size_t const len) noexcept
    { return ~update(buf, len, ~uint32{0}); }
};

}     // namespace amp


#endif  // AMP_INCLUDED_BE28D5F7_B210_4BAE_A7C7_89BE04F56880

