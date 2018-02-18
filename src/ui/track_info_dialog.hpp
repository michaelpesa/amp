////////////////////////////////////////////////////////////////////////////////
//
// ui/track_info_dialog.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_40B4C450_4A78_43A8_A3A3_D8669FE62895
#define AMP_INCLUDED_40B4C450_4A78_43A8_A3A3_D8669FE62895


#include <amp/stddef.hpp>

#include <QtWidgets/QDialog>


QT_BEGIN_NAMESPACE
class QTabWidget;
class QWidget;
QT_END_NAMESPACE


namespace amp {
namespace media {
    class track;
}

namespace ui {

class TrackInfoDialog final :
    public QDialog
{
    Q_OBJECT

public:
    explicit TrackInfoDialog(media::track const&, QWidget* = nullptr);

private:
    void addMetadataTab(media::track const&);
    void addPropertyTab(media::track const&);
    void addCueSheetTab(media::track const&);
    void addLyricsTab(media::track const&);

    QTabWidget* tabs;
};

}}    // namespace amp::ui


#endif  // AMP_INCLUDED_40B4C450_4A78_43A8_A3A3_D8669FE62895

