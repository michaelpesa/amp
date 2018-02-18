////////////////////////////////////////////////////////////////////////////////
//
// media/cue_sheet.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_D3F73A4B_F891_4889_B546_C7464823CC24
#define AMP_INCLUDED_D3F73A4B_F891_4889_B546_C7464823CC24


#include <amp/media/dictionary.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include <chrono>
#include <ratio>
#include <vector>


namespace amp {
namespace cue {

using frames = std::chrono::duration<uint64, std::ratio<1, 75>>;

struct track
{
    cue::frames start;
    u8string file;
    media::dictionary tags;
};

std::vector<cue::track> parse(u8string);

}}    // namespace amp::media


#endif  // AMP_INCLUDED_D3F73A4B_F891_4889_B546_C7464823CC24

