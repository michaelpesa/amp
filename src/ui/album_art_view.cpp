////////////////////////////////////////////////////////////////////////////////
//
// ui/album_art_view.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/media/image.hpp>
#include <amp/stddef.hpp>

#include "core/config.hpp"
#include "core/filesystem.hpp"
#include "media/track.hpp"
#include "ui/album_art_view.hpp"
#include "ui/image_server.hpp"
#include "ui/string.hpp"

#include <cmath>
#include <cstddef>

#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QImageWriter>
#include <QtGui/QPainter>
#include <QtWidgets/QAction>
#include <QtWidgets/QActionGroup>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>


namespace amp {
namespace ui {

AlbumArtView::AlbumArtView(QWidget* const parent) :
    QWidget{parent},
    server{new ImageServer(this)},
    image_type{media::image::type::front_cover}
{
    setContextMenuPolicy(Qt::ActionsContextMenu);
    setMinimumSize(96, 96);

    auto action = new QAction{tr("Save as..."), this};
    connect(action, &QAction::triggered, this, &AlbumArtView::onMenuSaveAs);
    addAction(action);

    connect(server, &ImageServer::recv,
            this, [=](QImage x) {
                if (cached_image != x) {
                    cached_image = std::move(x);
                    action->setEnabled(!cached_image.isNull());
                    QWidget::update();
                }
            });

    action = new QAction{this};
    action->setSeparator(true);
    addAction(action);

    auto group = new QActionGroup(this);

    auto addCoverTypeAction = [&](media::image::type const type,
                                  char const*        const text) {
        auto const checked = group->actions().empty();

        action = group->addAction(tr(text));
        action->setCheckable(true);
        action->setChecked(checked);

        connect(action, &QAction::triggered,
                this, [this, type]{
                    image_type = type;
                    if (!cached_image_uri.empty()) {
                        server->send(cached_image_uri, image_type);
                    }
                });
    };

    addCoverTypeAction(media::image::type::front_cover,     "Front cover");
    addCoverTypeAction(media::image::type::back_cover,      "Back cover");
    addCoverTypeAction(media::image::type::file_icon_32x32, "File icon");
    addCoverTypeAction(media::image::type::media,           "Media");
    addCoverTypeAction(media::image::type::artist,          "Artist");
    addActions(group->actions());
}

AlbumArtView::~AlbumArtView() = default;

void AlbumArtView::onTrackSelected(media::track const& track)
{
    cached_image_uri = track.location;
    if (!cached_image_uri.empty()) {
        server->send(cached_image_uri, image_type);
    }
}

void AlbumArtView::onPlaylistSizeChanged(std::size_t const size)
{
    if (size == 0) {
        cached_image = {};
        cached_image_uri = {};
        QWidget::update();
    }
}

void AlbumArtView::paintEvent(QPaintEvent*)
{
    auto image = QImage{};
    auto x = int{0};
    auto y = int{0};

    if (!cached_image.isNull()) {
        auto const device_pixel_ratio = qApp->devicePixelRatio();
        auto const w = static_cast<double>(width());
        auto const h = static_cast<double>(height());

        image = cached_image.scaled(
            int(std::lround(w * device_pixel_ratio)),
            int(std::lround(h * device_pixel_ratio)),
            Qt::KeepAspectRatio, Qt::SmoothTransformation);

        image.setDevicePixelRatio(device_pixel_ratio);

        // Calculate the center of the widget.
        x = int(std::lround((w - (image.width()  / device_pixel_ratio)) / 2));
        y = int(std::lround((h - (image.height() / device_pixel_ratio)) / 2));
    }

    QPainter painter{this};
    painter.drawImage(x, y, image);
}

void AlbumArtView::onMenuSaveAs()
{
    auto const selected = QFileDialog::getSaveFileName(
        this, tr("Save Image"),
        to_qstring(ui::save_album_art_history.load()),
        tr("Images (*.jpg *.jpeg *.png)"));

    if (selected.isEmpty()) {
        return;
    }

    QImageWriter writer{selected};
    if (!writer.write(cached_image)) {
        QMessageBox::critical(this,
                              tr("Failed to save image"),
                              writer.errorString());
    }
    else {
        auto const path = to_u8string(selected);
        ui::save_album_art_history.store(fs::parent_path(path));
    }
}

}}    // namespace amp::ui

