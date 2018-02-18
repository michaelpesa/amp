////////////////////////////////////////////////////////////////////////////////
//
// plugins/demux/mp4_audio.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_657BB616_6CD8_44AA_9A8B_CD2C4252D898
#define AMP_INCLUDED_657BB616_6CD8_44AA_9A8B_CD2C4252D898


#include <amp/stddef.hpp>

#include <array>


namespace amp {
namespace audio {
    struct codec_format;
}


namespace mp4 {

extern std::array<uint32, 16> const sample_rates;
extern std::array<uint8, 8> const channels;

void parse_audio_specific_config(audio::codec_format&);

}}    // namespace amp::mp4


#endif  // AMP_INCLUDED_657BB616_6CD8_44AA_9A8B_CD2C4252D898

