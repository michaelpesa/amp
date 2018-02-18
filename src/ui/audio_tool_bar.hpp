////////////////////////////////////////////////////////////////////////////////
//
// ui/audio_tool_bar.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_971664C2_DD11_4650_96AB_24844E9E6576
#define AMP_INCLUDED_971664C2_DD11_4650_96AB_24844E9E6576


#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include "audio/player.hpp"

#include <chrono>
#include <cstddef>
#include <exception>

#include <QtWidgets/QToolBar>


QT_BEGIN_NAMESPACE
class QAction;
class QEvent;
class QTimerEvent;
QT_END_NAMESPACE


namespace amp {
namespace media {
    class playlist;
    class track;
}


namespace ui {

class AudioSeekBar;
class VolumeButton;


class AudioToolBar final :
    public QToolBar,
    private audio::player_delegate
{
    Q_OBJECT

public:
    explicit AudioToolBar(QWidget* = nullptr);
    virtual ~AudioToolBar();

    void play();
    void stop();
    void prev();
    void next();

    bool isPlaying() const noexcept
    { return player.is_playing(); }

    bool isPaused() const noexcept
    { return player.is_paused(); }

    bool isStopped() const noexcept
    { return player.is_stopped(); }

    bool canPlay() const noexcept
    { return has_audio && has_media; }

    void onPlaylistChanged(media::playlist*);
    void onPlaylistSizeChanged(std::size_t);
    void onConfigChanged();

Q_SIGNALS:
    void aboutToStart();
    void bitRateChanged(uint32);
    void positionChanged(std::chrono::nanoseconds);
    void playerTrackChanged(media::track const&);
    void playerStateChanged(audio::player_state);

protected:
    void customEvent(QEvent*) override;
    void timerEvent(QTimerEvent*) override;

private:
    void track_complete() override;
    void error_occurred() override;

    AMP_INTERNAL_LINKAGE void startPlaybackAtTrack(std::size_t);
    AMP_INTERNAL_LINKAGE void updateAvailability();
    AMP_INTERNAL_LINKAGE void updatePlayerState();
    AMP_INTERNAL_LINKAGE void notifyErrorOccurred(std::exception_ptr);

    QAction*          play_action;
    QAction*          stop_action;
    QAction*          prev_action;
    QAction*          next_action;
    ui::AudioSeekBar* seek_bar;
    ui::VolumeButton* volume;

    audio::player    player{*this};
    media::playlist* playlist{};
    std::size_t      track_change_index{};
    int              refresh_timer_id{-1};
    int              bit_rate_refresh{0};
    bool             has_audio{false};
    bool             has_media{false};
};

}}    // namespace amp::ui


#endif  // AMP_INCLUDED_971664C2_DD11_4650_96AB_24844E9E6576

