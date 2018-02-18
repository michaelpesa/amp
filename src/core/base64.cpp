////////////////////////////////////////////////////////////////////////////////
//
// core/base64.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/base64.hpp>
#include <amp/error.hpp>
#include <amp/io/memory.hpp>
#include <amp/stddef.hpp>

#include <array>
#include <cstddef>


namespace amp {
namespace base64 {
namespace {

constexpr std::array<uint8, 64> encode_table {{
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9','+','/',
}};

constexpr std::array<uint8, 256> decode_table {{
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff,
    0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
}};

}     // namespace <unnamed>


std::size_t stream::decode(void const* const src_, std::size_t len,
                           void* const dst_)
{
    auto src = static_cast<uint8 const*>(src_);
    auto dst = static_cast<uint8*>(dst_);
    auto const dst_start = dst;

    AMP_ASSERT(state_ <= 3);
    AMP_ASSUME(state_ <= 3);

    switch (state_) for (;;) {
        [[fallthrough]];
    case 0:
        for (; len >= 13; len -= 8, dst += 6) {
            uint64 d;
            if ((d = decode_table[*src++]) & 0x80) { goto fail; }

            auto v = d << 58;
            if ((d = decode_table[*src++]) & 0x80) { goto fail; }
            v |= d << 52;
            if ((d = decode_table[*src++]) & 0x80) { goto fail; }
            v |= d << 46;
            if ((d = decode_table[*src++]) & 0x80) { goto fail; }
            v |= d << 40;
            if ((d = decode_table[*src++]) & 0x80) { goto fail; }
            v |= d << 34;
            if ((d = decode_table[*src++]) & 0x80) { goto fail; }
            v |= d << 28;
            if ((d = decode_table[*src++]) & 0x80) { goto fail; }
            v |= d << 22;
            if ((d = decode_table[*src++]) & 0x80) { goto fail; }
            v |= d << 16;

            io::store<BE>(dst, v);
        }
        for (; len >= 8; len -= 4, dst += 3) {
            uint32 d;
            if ((d = decode_table[*src++]) & 0x80) { goto fail; }

            auto v = d << 26;
            if ((d = decode_table[*src++]) & 0x80) { goto fail; }
            v |= d << 20;
            if ((d = decode_table[*src++]) & 0x80) { goto fail; }
            v |= d << 14;
            if ((d = decode_table[*src++]) & 0x80) { goto fail; }
            v |= d << 8;

            io::store<BE>(dst, v);
        }
        if (len-- == 0) {
            break;
        }

        uint8 d;
        if ((d = decode_table[*src++]) & 0x80) {
            goto fail;
        }
        carry_ = static_cast<uint8>(d << 2);
        state_ = 1;
        [[fallthrough]];
    case 1:
        if (len-- == 0) {
            break;
        }
        if ((d = decode_table[*src++]) & 0x80) {
            goto fail;
        }
        *dst++ = static_cast<uint8>(carry_ | (d >> 4));
        carry_ = static_cast<uint8>(d << 4);
        state_ = 2;
        [[fallthrough]];
    case 2:
        if (len-- == 0) {
            break;
        }
        if ((d = decode_table[*src++]) & 0x80) {
            if (d == 0xfe) {
                break;
            }
            goto fail;
        }
        *dst++ = static_cast<uint8>(carry_ | (d >> 2));
        carry_ = static_cast<uint8>(d << 6);
        state_ = 3;
        [[fallthrough]];
    case 3:
        if (len-- == 0) {
            break;
        }
        if ((d = decode_table[*src++]) & 0x80) {
            if (d == 0xfe) {
                break;
            }
            goto fail;
        }
        *dst++ = static_cast<uint8>(carry_ | d);
        carry_ = 0;
        state_ = 0;
    }
    return static_cast<std::size_t>(dst - dst_start);

fail:
    raise(errc::invalid_argument, "invalid base64-encoded string");
}

std::size_t stream::encode(void const* const src_, std::size_t len,
                           void* const dst_) noexcept
{
    auto src = static_cast<uint8 const*>(src_);
    auto dst = static_cast<uint8*>(dst_);
    auto const dst_start = dst;

    AMP_ASSERT(state_ <= 2);
    AMP_ASSUME(state_ <= 2);

    switch (state_) for (;;) {
        [[fallthrough]];
    case 0:
        for (; len >= 8; len -= 6, src += 6) {
            auto const v = io::load<uint64,BE>(src);
            *dst++ = encode_table[(v >> 58) & 0x3f];
            *dst++ = encode_table[(v >> 52) & 0x3f];
            *dst++ = encode_table[(v >> 46) & 0x3f];
            *dst++ = encode_table[(v >> 40) & 0x3f];
            *dst++ = encode_table[(v >> 34) & 0x3f];
            *dst++ = encode_table[(v >> 28) & 0x3f];
            *dst++ = encode_table[(v >> 22) & 0x3f];
            *dst++ = encode_table[(v >> 16) & 0x3f];
        }
        for (; len >= 4; len -= 3, src += 3) {
            auto const v = io::load<uint32,BE>(src);
            *dst++ = encode_table[(v >> 26) & 0x3f];
            *dst++ = encode_table[(v >> 20) & 0x3f];
            *dst++ = encode_table[(v >> 14) & 0x3f];
            *dst++ = encode_table[(v >>  8) & 0x3f];
        }
        if (len-- == 0) {
            break;
        }
        *dst++ = encode_table[*src >> 2];
        carry_ = static_cast<uint8>((*src++ << 4) & 0x30);
        state_ = 1;
        [[fallthrough]];
    case 1:
        if (len-- == 0) {
            break;
        }
        *dst++ = encode_table[carry_ | (*src >> 4)];
        carry_ = static_cast<uint8>((*src++ << 2) & 0x3c);
        state_ = 2;
        [[fallthrough]];
    case 2:
        if (len-- == 0) {
            break;
        }
        *dst++ = encode_table[carry_ | (*src >> 6)];
        *dst++ = encode_table[*src++ & 0x3f];
        state_ = 0;
    }

    return static_cast<std::size_t>(dst - dst_start);
}

std::size_t stream::encode_finish(void* const dst_) noexcept
{
    auto dst = static_cast<uint8*>(dst_);

    AMP_ASSERT(state_ <= 2);
    AMP_ASSUME(state_ <= 2);

    switch (state_) {
    case 0:
        return 0;
    case 1:
        *dst++ = encode_table[carry_];
        *dst++ = '=';
        *dst++ = '=';
        return 3;
    case 2:
        *dst++ = encode_table[carry_];
        *dst++ = '=';
        return 2;
    default:
        AMP_UNREACHABLE();
    }
}

}}    // namespace amp::base64

