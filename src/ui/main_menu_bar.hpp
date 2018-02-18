////////////////////////////////////////////////////////////////////////////////
//
// ui/main_menu_bar.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_77855531_F561_4F1E_845D_598730A0BEBD
#define AMP_INCLUDED_77855531_F561_4F1E_845D_598730A0BEBD


#include <amp/net/uri.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include <vector>

#include <QtWidgets/QMenuBar>


QT_BEGIN_NAMESPACE
class QUndoStack;
QT_END_NAMESPACE


namespace amp {
namespace media {
    enum class playback_order : int;
}


namespace ui {

class MainMenuBar final :
    public QMenuBar
{
    Q_OBJECT

public:
    explicit MainMenuBar(QUndoStack*, QWidget* = nullptr);

Q_SIGNALS:
    void fileQuit();
    void filePreferences();
    void fileAddFiles(std::vector<net::uri>);
    void fileAddFolder(u8string);

    void playbackOrderChanged(media::playback_order);

private:
    void createFileMenu();
    void createPlaybackMenu();

    void onFileAddFiles();
    void onFileAddFolder();
    void onFileAddNetwork();

    QUndoStack* undo_stack;
};

}}    // namespace amp::ui


#endif  // AMP_INCLUDED_77855531_F561_4F1E_845D_598730A0BEBD

