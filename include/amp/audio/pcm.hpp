////////////////////////////////////////////////////////////////////////////////
//
// amp/audio/pcm.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_44171483_A0D5_4D49_92F6_21D481633F02
#define AMP_INCLUDED_44171483_A0D5_4D49_92F6_21D481633F02


#include <amp/net/endian.hpp>
#include <amp/stddef.hpp>

#include <cstddef>
#include <memory>


namespace amp {
namespace audio {

class packet;


namespace pcm {

enum : uint32 {
    ieee_float      = (1 << 0),
    big_endian      = (1 << 1),
    signed_int      = (1 << 2),
    aligned_high    = (1 << 3),
    non_interleaved = (1 << 4),
    host_endian     = (AMP_BIG_ENDIAN ? big_endian : 0),
};


struct spec
{
    uint32 bits_per_sample{};
    uint32 bytes_per_sample{};
    uint32 channels{};
    uint32 flags{};
};


class blitter
{
public:
    virtual ~blitter() = default;

    virtual void convert(void const*, std::size_t, audio::packet&) = 0;

    AMP_EXPORT
    static std::unique_ptr<blitter> create(pcm::spec const&);
};

}}}   // namespace amp::audio::pcm


#endif  // AMP_INCLUDED_44171483_A0D5_4D49_92F6_21D481633F02

