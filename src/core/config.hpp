////////////////////////////////////////////////////////////////////////////////
//
// core/config.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_E793B5F7_F72C_48B8_9D8E_6B54DC864B50
#define AMP_INCLUDED_E793B5F7_F72C_48B8_9D8E_6B54DC864B50


#include <amp/io/buffer.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include <cstddef>
#include <vector>

#if (defined(__APPLE__) && defined(__MACH__)) && defined(__OBJC__)
@class NSString;
#endif


namespace amp {
namespace config {

#if (defined(__APPLE__) && defined(__MACH__)) && defined(__OBJC__)
using entry_key = NSString __unsafe_unretained*;
#else
using entry_key = void*;
#endif


template<typename T>
struct entry;

template<>
struct entry<bool>
{
    bool load() const;
    void store(bool) const;

    entry_key key;
};

template<>
struct entry<float>
{
    float load() const;
    void store(float) const;

    entry_key key;
};

template<>
struct entry<u8string>
{
    u8string load() const;
    void store(u8string const&) const;

    entry_key key;
};

template<>
struct entry<std::vector<u8string>>
{
    std::vector<u8string> load() const;
    void store(std::vector<u8string> const&) const;

    entry_key key;
};

template<>
struct entry<io::buffer>
{
    io::buffer load() const;
    void store(void const*, std::size_t) const;

    void store(iterator_range<uchar const*> const v) const
    { store(v.begin(), v.size()); }

    void store(iterator_range<char const*> const v) const
    { store(v.begin(), v.size()); }

    entry_key key;
};


extern void register_defaults();

}   // namespace config


namespace ui {

extern char const default_window_title_format[];

extern config::entry<u8string>   const main_window_title;
extern config::entry<io::buffer> const main_window_state;
extern config::entry<io::buffer> const main_window_geometry;
extern config::entry<io::buffer> const playlist_header_state;
extern config::entry<io::buffer> const preferences_geometry;
extern config::entry<io::buffer> const track_info_geometry;
extern config::entry<u8string>   const add_files_history;
extern config::entry<u8string>   const add_folder_history;
extern config::entry<u8string>   const save_album_art_history;

}     // namespace ui

namespace audio {

extern config::entry<std::vector<u8string>> const active_filter_preset;
extern config::entry<u8string>              const active_output_plugin;
extern config::entry<u8string>              const active_output_device;
extern config::entry<float>                 const output_level;
extern config::entry<bool>                  const replaygain_apply;
extern config::entry<bool>                  const replaygain_album;
extern config::entry<float>                 const replaygain_preamp;

}}    // namespace amp::audio


#endif  // AMP_INCLUDED_E793B5F7_F72C_48B8_9D8E_6B54DC864B50

