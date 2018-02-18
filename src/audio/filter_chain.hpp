////////////////////////////////////////////////////////////////////////////////
//
// audio/filter_chain.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_F6E06AF5_8E91_40D9_B008_456CC77C1312
#define AMP_INCLUDED_F6E06AF5_8E91_40D9_B008_456CC77C1312


#include <amp/audio/filter.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include "audio/replaygain.hpp"

#include <utility>
#include <vector>


namespace amp {
namespace audio {

struct format;
class packet;


class filter_chain
{
public:
    void rebuild(std::vector<u8string> const&,
                 audio::replaygain_config const&);

    void calibrate(audio::format const&,
                   audio::format const&,
                   audio::replaygain_info const&);

    void process(audio::packet&);
    void drain(audio::packet&);
    void flush();

private:
    std::vector<ref_ptr<audio::filter>> elems_;
    audio::replaygain_filter rgain_;
};

}}    // namespace amp::audio


#endif  // AMP_INCLUDED_F6E06AF5_8E91_40D9_B008_456CC77C1312

