////////////////////////////////////////////////////////////////////////////////
//
// ui/audio_info_bar.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_22BB4271_6D0E_41CD_A6CE_92BEAAC81992
#define AMP_INCLUDED_22BB4271_6D0E_41CD_A6CE_92BEAAC81992


#include <amp/stddef.hpp>

#include <chrono>
#include <cstddef>

#include <QtWidgets/QStatusBar>


QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE


namespace amp {
namespace audio {
    enum class player_state : uint8;
}
namespace media {
    class track;
}

namespace ui {

class AudioInfoBar final :
    public QStatusBar
{
    Q_OBJECT

public:
    explicit AudioInfoBar(QWidget* = nullptr);
    virtual ~AudioInfoBar();

    void onPlayerStateChanged(audio::player_state);
    void onPlayerTrackChanged(media::track const&);
    void onPlaylistSizeChanged(std::size_t);

    void onPositionChanged(std::chrono::nanoseconds);
    void onBitRateChanged(uint32);

private:
    void refreshPlaybackPosition();
    void refreshPlaybackBitRate();

    QLabel* playlist_size_label;
    QLabel* playback_bit_rate_label;
    QLabel* playback_position_label;

    std::chrono::seconds cached_duration{};
    std::chrono::seconds cached_position{};
    uint32               cached_bit_rate{};
};

}}    // namespace amp::ui


#endif  // AMP_INCLUDED_22BB4271_6D0E_41CD_A6CE_92BEAAC81992

