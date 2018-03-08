////////////////////////////////////////////////////////////////////////////////
//
// ui/album_art_view.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_B1D95D6E_950F_4FA6_8590_9D161F308E5F
#define AMP_INCLUDED_B1D95D6E_950F_4FA6_8590_9D161F308E5F


#include <amp/media/image.hpp>
#include <amp/net/uri.hpp>
#include <amp/stddef.hpp>

#include <cstddef>

#include <QtGui/QImage>
#include <QtWidgets/QWidget>


QT_BEGIN_NAMESPACE
class QImage;
class QPaintEvent;
QT_END_NAMESPACE


namespace amp {
namespace media {
    class track;
}


namespace ui {

class ImageServer;

class AlbumArtView final :
    public QWidget
{
    Q_OBJECT

public:
    explicit AlbumArtView(QWidget* = nullptr);
    virtual ~AlbumArtView();

    void onTrackSelected(media::track const&);
    void onPlaylistSizeChanged(std::size_t);

Q_SIGNALS:
    void availabilityChanged(bool);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void onMenuSaveAs();

    ImageServer* server;
    QImage cached_image;
    net::uri cached_image_uri;
    media::image::type image_type;
};

}}    // namespace amp::ui


#endif  // ANDROMEDIA_UI_ALBUM_ART_HPP

