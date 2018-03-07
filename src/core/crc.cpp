////////////////////////////////////////////////////////////////////////////////
//
// core/crc.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/bitops.hpp>
#include <amp/crc.hpp>
#include <amp/io/memory.hpp>
#include <amp/stddef.hpp>

#include "core/cpu.hpp"

#include <array>
#include <cstddef>
#include <utility>

#if defined(AMP_HAS_X86) || defined(AMP_HAS_X64)
# include <immintrin.h>
#endif


namespace amp {
namespace {

#if defined(AMP_HAS_X86) || defined(AMP_HAS_X64)
AMP_TARGET("sse4.2")
inline uint32 crc32c_update_sse4_2(uint8 const* buf, std::size_t len,
                                   uint32 rem) noexcept
{
#if defined(AMP_HAS_X64)
    auto tmp = uint64{rem};
    for (auto i = len / sizeof(uint64); i != 0; --i) {
        tmp = _mm_crc32_u64(tmp, io::load<uint64,LE>(buf));
        buf = buf + sizeof(uint64);
    }
    rem = static_cast<uint32>(tmp);

    if (len & sizeof(uint32)) {
        rem = _mm_crc32_u32(rem, io::load<uint32,LE>(buf));
        buf = buf + sizeof(uint32);
    }
#else
    for (auto i = len / sizeof(uint32); i != 0; --i) {
        rem = _mm_crc32_u32(rem, io::load<uint32,LE>(buf));
        buf = buf + sizeof(uint32);
    }
#endif

    if (len & sizeof(uint16)) {
        rem = _mm_crc32_u16(rem, io::load<uint16,LE>(buf));
        buf = buf + sizeof(uint16);
    }
    if (len & sizeof(uint8)) {
        rem = _mm_crc32_u8(rem, *buf);
    }
    return rem;
}
#endif  // AMP_HAS_X86 || AMP_HAS_X64


using slicingby4_table = std::array<std::array<uint32, 256>, 4>;

template<uint32, typename = std::make_integer_sequence<uint32, 256>>
struct gen_slicingby4_table;

template<uint32 Poly, uint32... I>
struct gen_slicingby4_table<Poly, std::integer_sequence<uint32, I...>>
{
private:
    static constexpr uint32 precompute(uint32 const x, int const i) noexcept
    { return (i == 8) ? x : precompute((x >> 1) ^ (Poly & -(x & 1)), i + 1); }

    using Slice = std::array<uint32, 256>;

    static constexpr Slice s0 {{ precompute(I, 0)... }};
    static constexpr Slice s1 {{ ((s0[I] >> 8) ^ s0[s0[I] & 0xff])... }};
    static constexpr Slice s2 {{ ((s1[I] >> 8) ^ s0[s1[I] & 0xff])... }};
    static constexpr Slice s3 {{ ((s2[I] >> 8) ^ s0[s2[I] & 0xff])... }};

public:
    constexpr slicingby4_table operator()() const noexcept
    { return {{ s0, s1, s2, s3, }}; }
};


uint32 slicingby4_update(uint8 const* buf, std::size_t len, uint32 rem,
                         slicingby4_table const& table) noexcept
{
    for (; len != 0 && !is_aligned(buf, alignof(uint32)); --len) {
        rem = (rem >> 8) ^ table[0][(rem ^ *buf++) & 0xff];
    }

    for (auto i = len / sizeof(uint32); i != 0; --i) {
        rem = rem ^ io::load_aligned<uint32,LE>(buf);
        rem = table[3][(rem >>  0) & 0xff]
            ^ table[2][(rem >>  8) & 0xff]
            ^ table[1][(rem >> 16) & 0xff]
            ^ table[0][(rem >> 24) & 0xff];
        buf = buf + sizeof(uint32);
    }

    if (len & sizeof(uint16)) {
        rem = rem ^ io::load_aligned<uint16,LE>(buf);
        rem = (rem >> 16)
            ^ table[1][(rem >> 0) & 0xff]
            ^ table[0][(rem >> 8) & 0xff];
        buf = buf + sizeof(uint16);
    }
    if (len & sizeof(uint8)) {
        rem = (rem >> 8) ^ table[0][(rem ^ *buf) & 0xff];
    }
    return rem;
}

}     // namespace <unnamed>


uint32 crc32::update(void const* const buf, std::size_t const len,
                     uint32 const rem) noexcept
{
    static constexpr auto table = gen_slicingby4_table<0xedb88320>{}();
    return slicingby4_update(static_cast<uint8 const*>(buf), len, rem, table);
}

uint32 crc32c::update(void const* const buf, std::size_t const len,
                      uint32 const rem) noexcept
{
#if defined(AMP_HAS_X86) || defined(AMP_HAS_X64)
    if (cpu::has_sse4_2()) {
        return crc32c_update_sse4_2(static_cast<uint8 const*>(buf), len, rem);
    }
#endif
    static constexpr auto table = gen_slicingby4_table<0x82f63b78>{}();
    return slicingby4_update(static_cast<uint8 const*>(buf), len, rem, table);
}

}     // namespace amp

