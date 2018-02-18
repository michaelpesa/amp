////////////////////////////////////////////////////////////////////////////////
//
// ui/playlist_view.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_8F651A09_96C0_4884_AD85_E17A2A85278F
#define AMP_INCLUDED_8F651A09_96C0_4884_AD85_E17A2A85278F


#include <amp/stddef.hpp>

#include <QtWidgets/QTreeView>


QT_BEGIN_NAMESPACE
class QAbstractItemModel;
class QKeyEvent;
class QModelIndex;
QT_END_NAMESPACE


namespace amp {
namespace media {
    class track;
}


namespace ui {

class PlaylistModel;

class PlaylistView final :
    public QTreeView
{
    Q_OBJECT

public:
    explicit PlaylistView(QWidget* = nullptr);

    void syncWithModel();

    void onAboutToStart();
    void onPlayerTrackChanged(media::track const&);

Q_SIGNALS:
    void trackSelected(media::track const&);

protected:
    void keyPressEvent(QKeyEvent*) override;

private:
    void onGetInfo();
    void onRemove();
#if defined(__APPLE__) && defined(__MACH__)
    void onShowInFinder();
#endif

    void selectIndex(QModelIndex const& index);

    PlaylistModel* model() const noexcept;
};

}}    // namespace amp::ui


#endif  // AMP_INCLUDED_8F651A09_96C0_4884_AD85_E17A2A85278F

