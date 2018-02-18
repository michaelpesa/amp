////////////////////////////////////////////////////////////////////////////////
//
// media/tags.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_995A379C_4CE1_42EA_9590_3136444A2126
#define AMP_INCLUDED_995A379C_4CE1_42EA_9590_3136444A2126


#include <amp/media/tags.hpp>
#include <amp/stddef.hpp>
#include <amp/string_view.hpp>
#include <amp/u8string.hpp>

#include <chrono>


namespace amp {
namespace media {
    class track;
}


namespace tags {

constexpr auto file_name    = "file name"_sv;
constexpr auto directory    = "directory"_sv;
constexpr auto disc_track   = "disc track"_sv;
constexpr auto length       = "length"_sv;
constexpr auto track_artist = "track artist"_sv;

constexpr auto max_hms_length = 23_sz;
char* format_hms(std::chrono::seconds, char*) noexcept;

enum class scope {
    smart = 0,
    exact = 1,
};

u8string find(media::track const&, string_view, scope = scope::smart);
bool contains(media::track const&, string_view) noexcept;
int compare(media::track const&, media::track const&, string_view);

AMP_READONLY char const* display_name(string_view) noexcept;

}}    // namespace amp::tags


#endif  // AMP_INCLUDED_995A379C_4CE1_42EA_9590_3136444A2126

