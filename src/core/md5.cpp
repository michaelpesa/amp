////////////////////////////////////////////////////////////////////////////////
//
// core/md5.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/bitops.hpp>
#include <amp/io/memory.hpp>
#include <amp/md5.hpp>
#include <amp/net/endian.hpp>
#include <amp/stddef.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>


namespace amp {
namespace {

#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) (y ^ (z & (x ^ y)))
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

#define STEP(f, w, x, y, z, t, s) w = x + rol(w + f(x, y, z) + t, s)

void md5_transform(uint32(&state)[4], uint32(&block)[16]) noexcept
{
    std::transform(std::begin(block), std::end(block),
                   std::begin(block), net::to_host<LE>);

    auto a = state[0];
    auto b = state[1];
    auto c = state[2];
    auto d = state[3];

    STEP(F1, a, b, c, d, block[ 0] + 0xd76aa478,  7);
    STEP(F1, d, a, b, c, block[ 1] + 0xe8c7b756, 12);
    STEP(F1, c, d, a, b, block[ 2] + 0x242070db, 17);
    STEP(F1, b, c, d, a, block[ 3] + 0xc1bdceee, 22);
    STEP(F1, a, b, c, d, block[ 4] + 0xf57c0faf,  7);
    STEP(F1, d, a, b, c, block[ 5] + 0x4787c62a, 12);
    STEP(F1, c, d, a, b, block[ 6] + 0xa8304613, 17);
    STEP(F1, b, c, d, a, block[ 7] + 0xfd469501, 22);
    STEP(F1, a, b, c, d, block[ 8] + 0x698098d8,  7);
    STEP(F1, d, a, b, c, block[ 9] + 0x8b44f7af, 12);
    STEP(F1, c, d, a, b, block[10] + 0xffff5bb1, 17);
    STEP(F1, b, c, d, a, block[11] + 0x895cd7be, 22);
    STEP(F1, a, b, c, d, block[12] + 0x6b901122,  7);
    STEP(F1, d, a, b, c, block[13] + 0xfd987193, 12);
    STEP(F1, c, d, a, b, block[14] + 0xa679438e, 17);
    STEP(F1, b, c, d, a, block[15] + 0x49b40821, 22);

    STEP(F2, a, b, c, d, block[ 1] + 0xf61e2562,  5);
    STEP(F2, d, a, b, c, block[ 6] + 0xc040b340,  9);
    STEP(F2, c, d, a, b, block[11] + 0x265e5a51, 14);
    STEP(F2, b, c, d, a, block[ 0] + 0xe9b6c7aa, 20);
    STEP(F2, a, b, c, d, block[ 5] + 0xd62f105d,  5);
    STEP(F2, d, a, b, c, block[10] + 0x02441453,  9);
    STEP(F2, c, d, a, b, block[15] + 0xd8a1e681, 14);
    STEP(F2, b, c, d, a, block[ 4] + 0xe7d3fbc8, 20);
    STEP(F2, a, b, c, d, block[ 9] + 0x21e1cde6,  5);
    STEP(F2, d, a, b, c, block[14] + 0xc33707d6,  9);
    STEP(F2, c, d, a, b, block[ 3] + 0xf4d50d87, 14);
    STEP(F2, b, c, d, a, block[ 8] + 0x455a14ed, 20);
    STEP(F2, a, b, c, d, block[13] + 0xa9e3e905,  5);
    STEP(F2, d, a, b, c, block[ 2] + 0xfcefa3f8,  9);
    STEP(F2, c, d, a, b, block[ 7] + 0x676f02d9, 14);
    STEP(F2, b, c, d, a, block[12] + 0x8d2a4c8a, 20);

    STEP(F3, a, b, c, d, block[ 5] + 0xfffa3942,  4);
    STEP(F3, d, a, b, c, block[ 8] + 0x8771f681, 11);
    STEP(F3, c, d, a, b, block[11] + 0x6d9d6122, 16);
    STEP(F3, b, c, d, a, block[14] + 0xfde5380c, 23);
    STEP(F3, a, b, c, d, block[ 1] + 0xa4beea44,  4);
    STEP(F3, d, a, b, c, block[ 4] + 0x4bdecfa9, 11);
    STEP(F3, c, d, a, b, block[ 7] + 0xf6bb4b60, 16);
    STEP(F3, b, c, d, a, block[10] + 0xbebfbc70, 23);
    STEP(F3, a, b, c, d, block[13] + 0x289b7ec6,  4);
    STEP(F3, d, a, b, c, block[ 0] + 0xeaa127fa, 11);
    STEP(F3, c, d, a, b, block[ 3] + 0xd4ef3085, 16);
    STEP(F3, b, c, d, a, block[ 6] + 0x04881d05, 23);
    STEP(F3, a, b, c, d, block[ 9] + 0xd9d4d039,  4);
    STEP(F3, d, a, b, c, block[12] + 0xe6db99e5, 11);
    STEP(F3, c, d, a, b, block[15] + 0x1fa27cf8, 16);
    STEP(F3, b, c, d, a, block[ 2] + 0xc4ac5665, 23);

    STEP(F4, a, b, c, d, block[ 0] + 0xf4292244,  6);
    STEP(F4, d, a, b, c, block[ 7] + 0x432aff97, 10);
    STEP(F4, c, d, a, b, block[14] + 0xab9423a7, 15);
    STEP(F4, b, c, d, a, block[ 5] + 0xfc93a039, 21);
    STEP(F4, a, b, c, d, block[12] + 0x655b59c3,  6);
    STEP(F4, d, a, b, c, block[ 3] + 0x8f0ccc92, 10);
    STEP(F4, c, d, a, b, block[10] + 0xffeff47d, 15);
    STEP(F4, b, c, d, a, block[ 1] + 0x85845dd1, 21);
    STEP(F4, a, b, c, d, block[ 8] + 0x6fa87e4f,  6);
    STEP(F4, d, a, b, c, block[15] + 0xfe2ce6e0, 10);
    STEP(F4, c, d, a, b, block[ 6] + 0xa3014314, 15);
    STEP(F4, b, c, d, a, block[13] + 0x4e0811a1, 21);
    STEP(F4, a, b, c, d, block[ 4] + 0xf7537e82,  6);
    STEP(F4, d, a, b, c, block[11] + 0xbd3af235, 10);
    STEP(F4, c, d, a, b, block[ 2] + 0x2ad7d2bb, 15);
    STEP(F4, b, c, d, a, block[ 9] + 0xeb86d391, 21);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

}     // namespace <unnamed>


void md5::update(void const* const src, std::size_t len) noexcept
{
    auto msg = static_cast<uint8 const*>(src);
    auto const t = 64 - static_cast<std::size_t>(bytes_ & 63);
    bytes_ += len;

    if (t > len) {
        std::copy_n(msg, len, reinterpret_cast<uint8*>(block_) + 64 - t);
        return;
    }

    std::copy_n(msg, t, reinterpret_cast<uint8*>(block_) + 64 - t);
    md5_transform(state_, block_);
    msg += t;
    len -= t;

    for (; len >= 64; msg += 64, len -= 64) {
        std::copy_n(msg, 64, reinterpret_cast<uint8*>(block_));
        md5_transform(state_, block_);
    }
    std::copy_n(msg, len, reinterpret_cast<uint8*>(block_));
}

void md5::finish(uint8(&digest)[16]) noexcept
{
    auto n = static_cast<int>(bytes_ & 63);
    auto p = reinterpret_cast<uint8*>(block_) + n;

    *p++ = 0x80;
    n = 56 - 1 - n;

    if (n < 0) {
        std::fill_n(p, n + 8, uint8{});
        md5_transform(state_, block_);
        p = reinterpret_cast<uint8*>(block_);
        n = 56;
    }

    std::fill_n(p, n, uint8{});
    io::store<LE>(&block_[14], bytes_ << 3);
    md5_transform(state_, block_);
    io::scatter<LE>(digest, state_);
}

}     // namespace amp

