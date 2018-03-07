////////////////////////////////////////////////////////////////////////////////
//
// amp/md5.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_8B1301D8_CD75_4839_94A9_42ADFBB87FF1
#define AMP_INCLUDED_8B1301D8_CD75_4839_94A9_42ADFBB87FF1


#include <amp/stddef.hpp>

#include <cstddef>


namespace amp {

class md5
{
public:
    md5() noexcept :
        bytes_{0},
        state_{0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476}
    {}

    AMP_EXPORT void update(void const*, std::size_t) noexcept;
    AMP_EXPORT void finish(uint8(&)[16]) noexcept;

    static void sum(void const* const msg, std::size_t const len,
                    uint8(&digest)[16]) noexcept
    {
        md5 ctx;
        ctx.update(msg, len);
        ctx.finish(digest);
    }

private:
    uint64 bytes_;
    uint32 state_[4];
    uint32 block_[16];
};

}     // namespace amp


#endif  // AMP_INCLUDED_8B1301D8_CD75_4839_94A9_42ADFBB87FF1

