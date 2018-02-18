////////////////////////////////////////////////////////////////////////////////
//
// amp/media/id3.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_2077734B_1E84_47BA_8293_E752C54535C2
#define AMP_INCLUDED_2077734B_1E84_47BA_8293_E752C54535C2


#include <amp/media/image.hpp>
#include <amp/optional.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>


namespace amp {
namespace io {
    class stream;
}
namespace media {
    class dictionary;
}


namespace id3v1 {

AMP_EXPORT u8string get_genre_name(uint8);
AMP_EXPORT bool find(io::stream&);
AMP_EXPORT void read(io::stream&, media::dictionary&);

}     // namespace id3v1

namespace id3v2 {

AMP_EXPORT bool skip(io::stream&);
AMP_EXPORT void read(io::stream&, media::dictionary&);
AMP_EXPORT media::image find_image(io::stream&, media::image_type);

}}    // namespace amp::id3v2


#endif  // AMP_INCLUDED_2077734B_1E84_47BA_8293_E752C54535C2

