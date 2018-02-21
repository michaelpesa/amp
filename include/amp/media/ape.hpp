////////////////////////////////////////////////////////////////////////////////
//
// amp/media/ape.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_DF6728E4_88A6_410E_A69D_631975CC2959
#define AMP_INCLUDED_DF6728E4_88A6_410E_A69D_631975CC2959


#include <amp/media/image.hpp>
#include <amp/stddef.hpp>


namespace amp {
namespace io {
    class stream;
}
namespace media {
    class dictionary;
}


namespace ape {

AMP_EXPORT bool find(io::stream&);
AMP_EXPORT void read(io::stream&, media::dictionary&);
AMP_EXPORT void read_no_preamble(void const*, std::size_t, media::dictionary&);
AMP_EXPORT media::image find_image(io::stream&, media::image_type);

}}    // namespace amp::ape


#endif  // AMP_INCLUDED_DF6728E4_88A6_410E_A69D_631975CC2959

