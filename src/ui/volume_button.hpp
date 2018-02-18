////////////////////////////////////////////////////////////////////////////////
//
// ui/volume_button.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_BEA2BFCE_7081_4F4B_8477_D5E60D063704
#define AMP_INCLUDED_BEA2BFCE_7081_4F4B_8477_D5E60D063704


#include <amp/stddef.hpp>

#include <QtWidgets/QSlider>


namespace amp {
namespace ui {

class VolumeButton final :
    public QSlider
{
    Q_OBJECT

public:
    explicit VolumeButton(QWidget* = nullptr);

    float volume() const
    {
        return level_;
    }

Q_SIGNALS:
    void volumeChanged(float);

private:
    void onSliderValueChanged(int);
    void onAboutToQuit();

    float level_{};
};

}}    // namespace amp::ui


#endif  // AMP_INCLUDED_BEA2BFCE_7081_4F4B_8477_D5E60D063704

