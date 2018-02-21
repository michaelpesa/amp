////////////////////////////////////////////////////////////////////////////////
//
// ui/playlist_model.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_C2810301_F27B_4459_9CE8_B6433DA1A046
#define AMP_INCLUDED_C2810301_F27B_4459_9CE8_B6433DA1A046


#include <amp/net/uri.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include "media/track.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

#include <QtCore/QAbstractTableModel>
#include <QtCore/QModelIndex>
#include <QtCore/QModelIndexList>


QT_BEGIN_NAMESPACE
class QVariant;
class QUndoStack;
QT_END_NAMESPACE


namespace amp {
namespace media {
    enum class playback_order : int;
    class playlist;
}

namespace ui {

class MediaScanner;

class PlaylistModel final :
    public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit PlaylistModel(QUndoStack*, QObject* = nullptr);
    virtual ~PlaylistModel();

    // ------------------------------------------------------------------------
    // QAbstractTableModel-specific functions.
    // ------------------------------------------------------------------------

    int rowCount(QModelIndex const& = QModelIndex{}) const override;
    int columnCount(QModelIndex const& = QModelIndex{}) const override;

    QVariant data(QModelIndex const&, int) const override;
    QVariant headerData(int, Qt::Orientation, int) const override;

    void sort(int, Qt::SortOrder = Qt::AscendingOrder) override;
    bool removeRows(int, int, QModelIndex const& = QModelIndex{}) override;


    // ------------------------------------------------------------------------
    // PlaylistModel-specific functions.
    // ------------------------------------------------------------------------

    using QAbstractTableModel::index;

    QModelIndex index(media::track const&) const;
    QModelIndex currentIndex() const;

    media::track const& track(QModelIndex const&) const;

    void setPlaylist(ref_ptr<media::playlist>);
    void setPlaylistIndex(QModelIndex const&);
    void setPlaybackOrder(media::playback_order);

    void addFiles(std::vector<net::uri>);
    void addFolder(u8string);

Q_SIGNALS:
    void playlistSizeChanged(std::size_t);

private:
    void launchScannerTask(std::vector<net::uri>, u8string);

    struct Column
    {
        std::string_view key;
        QString name;
    };

    MediaScanner* scanner;
    ref_ptr<media::playlist> playlist;
    QUndoStack* undo_stack;
    std::array<Column, 5> columns;
};

}}    // namespace amp::ui


#endif  // AMP_INCLUDED_C2810301_F27B_4459_9CE8_B6433DA1A046

