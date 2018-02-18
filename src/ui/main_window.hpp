////////////////////////////////////////////////////////////////////////////////
//
// ui/main_window.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_1534E07C_4A61_475A_B3A7_9C5C71267D5A
#define AMP_INCLUDED_1534E07C_4A61_475A_B3A7_9C5C71267D5A


#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>

#include "media/title_format.hpp"

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QWidget>


QT_BEGIN_NAMESPACE
class QCloseEvent;
class QModelIndex;
class QUndoStack;
QT_END_NAMESPACE


namespace amp {
namespace media {
    class playlist;
    class track;
}
namespace audio {
    enum class player_state : uint8;
}

namespace ui {

class AlbumArtView;
class AudioToolBar;
class AudioInfoBar;
class MainMenuBar;
class MediaLibrary;
class PlaylistModel;
class PlaylistView;


class MainWindow final :
    public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* = nullptr);

protected:
    void closeEvent(QCloseEvent*) override;

private:
    void onFilePreferences();
    void onPlayerAboutToStart();
    void onPlayerStateChanged(audio::player_state);
    void onPlayerTrackChanged(media::track const&);
    void onPlaylistViewActivated(QModelIndex const&);
    void onSelectedPlaylistChanged();
    void onConfigChanged();

    void connectSignals();

    QUndoStack*          undo_stack_;
    ui::MainMenuBar*     menu_bar_;
    ui::AudioToolBar*    tool_bar_;
    ui::AudioInfoBar*    info_bar_;
    ui::MediaLibrary*    media_lib_;
    ui::PlaylistModel*   pl_model_;
    ui::PlaylistView*    pl_view_;
    ui::AlbumArtView*    album_art_;
    media::title_format  title_format_;
};

}}    // namespace amp::ui


#endif  // AMP_INCLUDED_1534E07C_4A61_475A_B3A7_9C5C71267D5A

