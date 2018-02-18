////////////////////////////////////////////////////////////////////////////////
//
// ui/audio_seek_bar.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/muldiv.hpp>
#include <amp/stddef.hpp>

#include "media/track.hpp"
#include "ui/audio_seek_bar.hpp"

#include <algorithm>
#include <chrono>

#include <QtCore/QEvent>


namespace amp {
namespace ui {
namespace {

using namespace std::chrono_literals;


constexpr auto slider_scale = 200000;

inline auto to_slider_value(std::chrono::nanoseconds const pos,
                            std::chrono::nanoseconds const len) noexcept
{
    return static_cast<int>(muldiv(pos.count(), slider_scale, len.count()));
}

inline auto to_duration(int64 const value,
                        std::chrono::nanoseconds const len) noexcept
{
    return std::chrono::nanoseconds{muldiv(value, len.count(), slider_scale)};
}

}     // namespace <unnamed>


AudioSeekBar::AudioSeekBar(QWidget* const parent) :
    QSlider(Qt::Horizontal, parent),
    length(1ns)
{
    QSlider::setRange(0, slider_scale);
    QSlider::setSliderPosition(0);
    QSlider::setSliderDown(false);

    connect(this, &QSlider::sliderReleased,
            this, [this]{
                if (!QSlider::isSliderDown()) {
                    Q_EMIT seek(to_duration(QSlider::value(), length));
                }
            });
}

void AudioSeekBar::onPlayerTrackChanged(media::track const& track)
{
    length = std::max(track.length<std::chrono::nanoseconds>(), 1ns);
}

void AudioSeekBar::onPositionChanged(std::chrono::nanoseconds pos)
{
    pos = std::max(pos, 0ns);
    pos = std::min(pos, length);

    auto const value = to_slider_value(pos, length);
    if (QSlider::value() != value && !QSlider::isSliderDown()) {
        QSlider::setValue(value);
    }
}

void AudioSeekBar::changeEvent(QEvent* const event)
{
    if (event->type() == QEvent::EnabledChange) {
        bool const disabled = !QWidget::isEnabled();
        QSlider::blockSignals(disabled);

        if (disabled) {
            QSlider::setSliderPosition(0);
            QSlider::setSliderDown(false);
        }
    }

    QWidget::changeEvent(event);
}

}}    // namespace amp::ui

