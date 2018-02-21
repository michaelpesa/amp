////////////////////////////////////////////////////////////////////////////////
//
// media/tags.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_995A379C_4CE1_42EA_9590_3136444A2126
#define AMP_INCLUDED_995A379C_4CE1_42EA_9590_3136444A2126


#include <amp/media/tags.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include <chrono>
#include <string_view>


namespace amp {
namespace media {
    class track;
}


namespace tags {

using namespace ::std::literals;


constexpr auto file_name    = "file name"sv;
constexpr auto directory    = "directory"sv;
constexpr auto disc_track   = "disc track"sv;
constexpr auto length       = "length"sv;
constexpr auto track_artist = "track artist"sv;

constexpr auto max_hms_length = 23_sz;
char* format_hms(std::chrono::seconds, char*) noexcept;

enum class scope {
    smart = 0,
    exact = 1,
};

u8string find(media::track const&, std::string_view, scope = scope::smart);
bool contains(media::track const&, std::string_view) noexcept;
int compare(media::track const&, media::track const&, std::string_view);

AMP_READONLY char const* display_name(std::string_view) noexcept;

}}    // namespace amp::tags


#endif  // AMP_INCLUDED_995A379C_4CE1_42EA_9590_3136444A2126

