////////////////////////////////////////////////////////////////////////////////
//
// ui/media_library.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_763E0CD0_1450_4380_AE54_42427DA0C65D
#define AMP_INCLUDED_763E0CD0_1450_4380_AE54_42427DA0C65D


#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>

#include "media/playlist.hpp"

#include <vector>

#include <QtWidgets/QListWidget>


namespace amp {
namespace ui {

class MediaLibrary final :
    public QListWidget
{
    Q_OBJECT

public:
    explicit MediaLibrary(QWidget* = nullptr);

    ref_ptr<media::playlist> selectedPlaylist();

Q_SIGNALS:
    void selectedPlaylistChanged();

private:
    AMP_INTERNAL_LINKAGE uint32 newPlaylistID() const noexcept;

    void onAboutToQuit();
    void onNewPlaylist();
    void onRemove();
    void onRename();

    std::vector<ref_ptr<media::playlist>> playlists;
};

}}    // namespace amp::ui


#endif  // AMP_INCLUDED_763E0CD0_1450_4380_AE54_42427DA0C65D

