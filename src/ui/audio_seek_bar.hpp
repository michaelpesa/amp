////////////////////////////////////////////////////////////////////////////////
//
// ui/audio_seek_bar.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_35BFB3C5_8C52_4B3C_99D1_A2FCDCA00D07
#define AMP_INCLUDED_35BFB3C5_8C52_4B3C_99D1_A2FCDCA00D07


#include <amp/stddef.hpp>

#include <chrono>

#include <QtWidgets/QSlider>
#include <QtWidgets/QWidget>


QT_BEGIN_NAMESPACE
class QEvent;
QT_END_NAMESPACE


namespace amp {
namespace media {
    class track;
}


namespace ui {

class AudioSeekBar final :
    public QSlider
{
    Q_OBJECT

public:
    explicit AudioSeekBar(QWidget* = nullptr);

    void onPlayerTrackChanged(media::track const&);
    void onPositionChanged(std::chrono::nanoseconds);

Q_SIGNALS:
    void seek(std::chrono::nanoseconds);

protected:
    void changeEvent(QEvent*) override;

private:
    std::chrono::nanoseconds length;
};

}}    // namespace amp::ui


#endif  // AMP_INCLUDED_35BFB3C5_8C52_4B3C_99D1_A2FCDCA00D07

