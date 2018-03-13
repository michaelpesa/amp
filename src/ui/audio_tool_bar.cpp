////////////////////////////////////////////////////////////////////////////////
//
// ui/audio_tool_bar.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/stddef.hpp>

#include "audio/replaygain.hpp"
#include "core/config.hpp"
#include "media/playlist.hpp"
#include "ui/audio_seek_bar.hpp"
#include "ui/audio_tool_bar.hpp"
#include "ui/string.hpp"
#include "ui/volume_button.hpp"

#include <chrono>
#include <cstddef>
#include <exception>
#include <utility>

#include <QtCore/QEvent>
#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtWidgets/QAction>
#include <QtWidgets/QMessageBox>


namespace amp {
namespace ui {
namespace {

class AudioEvent final :
    public QEvent
{
public:
    enum Type {
        TrackComplete = QEvent::User + 1000,
        ErrorOccurred,
    };

    explicit AudioEvent(AudioEvent::Type const type, std::exception_ptr ep) :
        QEvent(static_cast<QEvent::Type>(type)),
        except(std::move(ep))
    {}

    std::exception_ptr except;
};

void postAudioEvent(QObject* const target, AudioEvent::Type const type,
                    std::exception_ptr ep = nullptr)
{
    QCoreApplication::postEvent(target, new AudioEvent(type, std::move(ep)));
}

}     // namespace <unnamed>


AudioToolBar::AudioToolBar(QWidget* const parent) :
    QToolBar(tr("AudioToolBar"), parent),
    play_action(new QAction(this)),
    stop_action(new QAction(this)),
    prev_action(new QAction(this)),
    next_action(new QAction(this)),
    seek_bar(new ui::AudioSeekBar(this)),
    volume(new ui::VolumeButton(this))
{
    setObjectName(QStringLiteral("AudioToolBar"));
    setAllowedAreas(Qt::TopToolBarArea);
    setFloatable(false);
    setMovable(false);

    play_action->setIcon(QIcon{QStringLiteral(":/icons/play.svgz")});
    connect(play_action, &QAction::triggered, this, &AudioToolBar::play);
    addAction(play_action);

    stop_action->setIcon(QIcon{QStringLiteral(":/icons/stop.svgz")});
    connect(stop_action, &QAction::triggered, this, &AudioToolBar::stop);
    addAction(stop_action);

    prev_action->setIcon(QIcon{QStringLiteral(":/icons/prev.svgz")});
    connect(prev_action, &QAction::triggered, this, &AudioToolBar::prev);
    addAction(prev_action);

    next_action->setIcon(QIcon{QStringLiteral(":/icons/next.svgz")});
    connect(next_action, &QAction::triggered, this, &AudioToolBar::next);
    addAction(next_action);

    connect(this, &AudioToolBar::playerTrackChanged,
            seek_bar, &AudioSeekBar::onPlayerTrackChanged);
    connect(this, &AudioToolBar::positionChanged,
            seek_bar, &AudioSeekBar::onPositionChanged);
    connect(seek_bar, &AudioSeekBar::seek,
            this, [this](std::chrono::nanoseconds const pos) {
                player.seek(pos);
                Q_EMIT positionChanged(pos);
            });
    connect(volume, &VolumeButton::volumeChanged,
            this, [this](float const level) {
                player.set_volume(level);
            });

    addSeparator();
    addWidget(seek_bar);

    addSeparator();
    addWidget(volume);

    updatePlayerState();
    onConfigChanged();
}

AudioToolBar::~AudioToolBar()
{
    player.stop();
}

inline void AudioToolBar::startPlaybackAtTrack(std::size_t const index)
{
    track_change_index = index;
    player.insert_track(playlist->at(index));
    player.start();
}

void AudioToolBar::play()
{
    try {
        switch (player.state()) {
        case audio::player_state::stopped:
            Q_EMIT aboutToStart();
            player.stop();
            startPlaybackAtTrack(playlist->position());
            break;
        case audio::player_state::playing:
        case audio::player_state::paused:
            player.pause();
            break;
        }
    }
    catch (...) {
        notifyErrorOccurred(std::current_exception());
    }
    updatePlayerState();
}

void AudioToolBar::stop()
{
    player.stop();
    updatePlayerState();
}

void AudioToolBar::prev()
{
    try {
        player.stop();
        startPlaybackAtTrack(playlist->prev(playlist->position()));
    }
    catch (...) {
        notifyErrorOccurred(std::current_exception());
    }
    updatePlayerState();
}

void AudioToolBar::next()
{
    try {
        player.stop();
        startPlaybackAtTrack(playlist->next(playlist->position()));
    }
    catch (...) {
        notifyErrorOccurred(std::current_exception());
    }
    updatePlayerState();
}

void AudioToolBar::onPlaylistSizeChanged(std::size_t const size)
{
    if (has_media ^ (size > 0)) {
        has_media = (size > 0);
        updateAvailability();
    }
}

void AudioToolBar::onPlaylistChanged(media::playlist* const x)
{
    playlist = x;
    if (has_media ^ (playlist != nullptr && playlist->size() != 0)) {
        has_media = (playlist != nullptr && playlist->size() != 0);
        updateAvailability();
    }
}

inline void AudioToolBar::updatePlayerState()
{
    if (isPlaying()) {
        bit_rate_refresh = 4;
        if (refresh_timer_id == -1) {
            refresh_timer_id = QObject::startTimer(1000 / 5);
        }
        play_action->setIcon(QIcon{QStringLiteral(":/icons/pause.svgz")});
    }
    else {
        if (refresh_timer_id >= 0) {
            QObject::killTimer(std::exchange(refresh_timer_id, -1));
        }
        play_action->setIcon(QIcon{QStringLiteral(":/icons/play.svgz")});
    }

    stop_action->setDisabled(isStopped());
    seek_bar->setDisabled(isStopped());

    Q_EMIT playerStateChanged(player.state());
}

void AudioToolBar::timerEvent(QTimerEvent*)
{
    Q_EMIT positionChanged(player.position());

    if (++bit_rate_refresh == 5) {
        Q_EMIT bitRateChanged(player.bit_rate());
        bit_rate_refresh = 0;
    }
}

void AudioToolBar::customEvent(QEvent* const ev)
{
    switch (static_cast<int>(ev->type())) {
    case AudioEvent::TrackComplete:
        {
            auto const index = track_change_index;
            if (index != playlist->position()) {
                playlist->position(index);
            }

            track_change_index = playlist->next(index);
            player.insert_track(playlist->at(track_change_index));

            Q_EMIT bitRateChanged(player.bit_rate());
            Q_EMIT positionChanged(player.position());
            Q_EMIT playerTrackChanged(playlist->at(index));
            break;
        }
    case AudioEvent::ErrorOccurred:
        notifyErrorOccurred(static_cast<AudioEvent*>(ev)->except);
        break;
    default:
        QObject::customEvent(ev);
        break;
    }
}

inline void AudioToolBar::updateAvailability()
{
    play_action->setEnabled(has_audio && has_media);
    prev_action->setEnabled(has_audio && has_media);
    next_action->setEnabled(has_audio && has_media);
    seek_bar->setEnabled(has_audio && !isStopped());
    volume->setEnabled(has_audio);

    if (!has_media && !isStopped()) {
        stop();
    }
}

void AudioToolBar::track_complete()
{
    postAudioEvent(this, AudioEvent::TrackComplete);
}

void AudioToolBar::error_occurred()
{
    postAudioEvent(this, AudioEvent::ErrorOccurred, std::current_exception());
}

void AudioToolBar::notifyErrorOccurred(std::exception_ptr const ep)
{
    player.stop();
    updatePlayerState();

    try {
        std::rethrow_exception(ep);
    }
    catch (std::exception const& ex) {
        QMessageBox::critical(this,
                              tr("Audio player encountered an error"),
                              to_qstring(ex.what()));
    }
}

void AudioToolBar::onConfigChanged()
{
    has_audio = false;
    try {
        auto const rg_mode   = audio::replaygain_apply.load()
                             ? audio::replaygain_album.load()
                             ? audio::replaygain_mode::album
                             : audio::replaygain_mode::track
                             : audio::replaygain_mode::none;
        auto const rg_preamp = audio::replaygain_preamp.load();
        auto const rg_config = audio::replaygain_config{rg_mode, rg_preamp};

        player.set_preset(audio::active_filter_preset.load(), rg_config);
        player.set_output(audio::active_output_plugin.load(),
                          audio::active_output_device.load());
        player.set_volume(volume->volume());
        has_audio = true;
    }
    catch (...) {
        notifyErrorOccurred(std::current_exception());
    }

    updateAvailability();
    if (!isStopped()) {
        Q_EMIT playerTrackChanged(playlist->playing());
    }
}

}}    // namespace amp::ui

