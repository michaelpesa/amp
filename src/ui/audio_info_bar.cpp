////////////////////////////////////////////////////////////////////////////////
//
// ui/audio_info_bar.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/stddef.hpp>

#include "audio/player.hpp"
#include "media/tags.hpp"
#include "media/track.hpp"
#include "ui/audio_info_bar.hpp"
#include "ui/string.hpp"

#include <chrono>
#include <cstddef>
#include <iterator>
#include <utility>

#include <QtWidgets/QLabel>


namespace amp {
namespace ui {

using namespace std::chrono_literals;


void AudioInfoBar::refreshPlaybackPosition()
{
    char buf[tags::max_hms_length * 2 + 3];
    auto s = tags::format_hms(cached_duration, std::end(buf));
    *(--s) = ' ';
    *(--s) = '/';
    *(--s) = ' ';
    s = tags::format_hms(cached_position, s);

    auto const len = static_cast<int>(std::end(buf) - s);
    playback_position_label->setText(QString::fromLatin1(s, len));
}

void AudioInfoBar::refreshPlaybackBitRate()
{
    playback_bit_rate_label->setText(QStringLiteral("%1 %2")
                                     .arg(cached_bit_rate / 1000)
                                     .arg(tr("kbps")));
}

AudioInfoBar::AudioInfoBar(QWidget* const parent) :
    QStatusBar(parent),
    playlist_size_label(new QLabel(this)),
    playback_bit_rate_label(new QLabel(this)),
    playback_position_label(new QLabel(this))
{
    addWidget(playlist_size_label);
    addWidget(playback_bit_rate_label);
    addWidget(playback_position_label);
}

AudioInfoBar::~AudioInfoBar() = default;

void AudioInfoBar::onPlayerStateChanged(audio::player_state const state)
{
    if (state == audio::player_state::stopped) {
        playback_bit_rate_label->clear();
        playback_position_label->clear();
        cached_position = 0s;
        cached_duration = 0s;
        cached_bit_rate = 0;
    }
}

void AudioInfoBar::onPlaylistSizeChanged(std::size_t const size)
{
    playlist_size_label->setText(QStringLiteral("%1 %2")
                                 .arg(size)
                                 .arg(tr("tracks")));
}

void AudioInfoBar::onPositionChanged(std::chrono::nanoseconds const ns)
{
    auto const s = std::chrono::duration_cast<std::chrono::seconds>(ns);
    if (cached_position != s) {
        cached_position  = s;
        if (cached_duration < cached_position) {
            cached_duration = cached_position;
        }
        refreshPlaybackPosition();
    }
}

void AudioInfoBar::onPlayerTrackChanged(media::track const& track)
{
    cached_duration = track.length<std::chrono::seconds>();
    refreshPlaybackPosition();
}

void AudioInfoBar::onBitRateChanged(uint32 const bit_rate)
{
    if (cached_bit_rate != bit_rate) {
        cached_bit_rate  = bit_rate;
        refreshPlaybackBitRate();
    }
}

}}    // namespace amp::ui

