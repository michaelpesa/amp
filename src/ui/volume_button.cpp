////////////////////////////////////////////////////////////////////////////////
//
// ui/volume_button.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/stddef.hpp>

#include "core/config.hpp"
#include "ui/volume_button.hpp"

#include <cmath>

#include <QtCore/QCoreApplication>
#include <QtCore/QSize>
#include <QtWidgets/QSizePolicy>


namespace amp {
namespace ui {

VolumeButton::VolumeButton(QWidget* const parent) :
    QSlider(Qt::Horizontal, parent),
    level_(audio::output_level.load())
{
    setPageStep(1);
    setMaximumSize(QSize{50, 20});
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    setRange(0, 100);

    auto const percent = static_cast<int>(std::lround(level_ * 100.f));
    setValue(percent);

    connect(this, &QSlider::valueChanged,
            this, &VolumeButton::onSliderValueChanged);
    connect(qApp, &QCoreApplication::aboutToQuit,
            this, &VolumeButton::onAboutToQuit);
}

void VolumeButton::onSliderValueChanged(int const percent)
{
    AMP_ASSERT(percent >= 0 && percent <= 100);
    level_ = static_cast<float>(percent) / 100.f;

    Q_EMIT volumeChanged(level_);
    setToolTip(tr("Volume: %1%").arg(percent));
}

void VolumeButton::onAboutToQuit()
{
    audio::output_level.store(level_);
}

}}    // namespace amp::ui

