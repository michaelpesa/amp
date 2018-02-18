////////////////////////////////////////////////////////////////////////////////
//
// core/config_macos.mm
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/io/buffer.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include <cstddef>
#include <cstdio>
#include <vector>

#include "audio/replaygain.hpp"
#include "core/config.hpp"
#include "media/title_format.hpp"

#import <Foundation/Foundation.h>


namespace amp {
namespace ui {

// '•' is U+2022
char const default_window_title_format[] =
    u8"[%artist%  •  ][%album%  •  ]%title%";


#define DEFINE_SETTING(name, key) decltype(name) name{key}

DEFINE_SETTING(main_window_title,      @"ui.mainwindow.title");
DEFINE_SETTING(main_window_state,      @"ui.mainwindow.state");
DEFINE_SETTING(main_window_geometry,   @"ui.mainwindow.geometry");
DEFINE_SETTING(playlist_header_state,  @"ui.playlist.headerstate");
DEFINE_SETTING(preferences_geometry,   @"ui.preferences.geometry");
DEFINE_SETTING(track_info_geometry,    @"ui.trackinfo.geometry");
DEFINE_SETTING(add_files_history,      @"ui.history.addfiles");
DEFINE_SETTING(add_folder_history,     @"ui.history.addfolder");
DEFINE_SETTING(save_album_art_history, @"ui.history.savealbumart");

}     // namespace ui

namespace audio {

DEFINE_SETTING(active_filter_preset, @"audio.filter.preset");
DEFINE_SETTING(active_output_plugin, @"audio.output.plugin");
DEFINE_SETTING(active_output_device, @"audio.output.device");
DEFINE_SETTING(output_level,         @"audio.output.level");
DEFINE_SETTING(replaygain_apply,     @"audio.replaygain.apply");
DEFINE_SETTING(replaygain_album,     @"audio.replaygain.album");
DEFINE_SETTING(replaygain_preamp,    @"audio.replaygain.preamp");

}     // namespace audio


namespace config {

bool entry<bool>::load() const
{ return [[NSUserDefaults standardUserDefaults] boolForKey:key]; }

void entry<bool>::store(bool const x) const
{ [[NSUserDefaults standardUserDefaults] setBool:x forKey:key]; }


float entry<float>::load() const
{ return [[NSUserDefaults standardUserDefaults] floatForKey:key]; }

void entry<float>::store(float const x) const
{ [[NSUserDefaults standardUserDefaults] setFloat:x forKey:key]; }


u8string entry<u8string>::load() const
{
    auto const s = [[NSUserDefaults standardUserDefaults] stringForKey:key];
    return s ? u8string::from_utf8_unchecked([s UTF8String]) : u8string{};
}

void entry<u8string>::store(u8string const& x) const
{
    [[NSUserDefaults standardUserDefaults] setObject:(x ? @(x.c_str()) : nil)
                                              forKey:key];
}


std::vector<u8string> entry<std::vector<u8string>>::load() const
{
    std::vector<u8string> out;

    auto const saved = [[NSUserDefaults standardUserDefaults] arrayForKey:key];
    if (saved != nil) {
        out.resize([saved count]);
        for (auto const i : xrange(out.size())) {
            out[i] = u8string::from_utf8_unchecked([saved[i] UTF8String]);
        }
    }
    return out;
}

void entry<std::vector<u8string>>::store(std::vector<u8string> const& x) const
{
    auto array = [NSMutableArray<NSString*> arrayWithCapacity:x.size()];
    for (auto const i : xrange(x.size())) {
        array[i] = @(x[i].c_str());
    }
    [[NSUserDefaults standardUserDefaults] setObject:array forKey:key];
}


io::buffer entry<io::buffer>::load() const
{
    io::buffer out;

    auto const saved = [[NSUserDefaults standardUserDefaults] dataForKey:key];
    if (saved != nil) {
        out.resize([saved length]);
        [saved getBytes:out.data() length:out.size()];
    }
    return out;
}

void entry<io::buffer>::store(void const* const p, std::size_t const n) const
{
    auto const data = [NSData dataWithBytes:p length:n];
    [[NSUserDefaults standardUserDefaults] setObject:data forKey:key];
}


void register_defaults()
{
    [[NSUserDefaults standardUserDefaults] registerDefaults:@{
        audio::output_level.key     : @1.0,
        audio::replaygain_apply.key : @YES,
        audio::replaygain_album.key : @YES,
        ui::main_window_title.key   : @(ui::default_window_title_format),
    }];
}

}}    // namespace amp::config

