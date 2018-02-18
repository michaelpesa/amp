////////////////////////////////////////////////////////////////////////////////
//
// ui/main_window.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/stddef.hpp>

#include "audio/player.hpp"
#include "core/config.hpp"
#include "media/track.hpp"
#include "ui/album_art_view.hpp"
#include "ui/audio_info_bar.hpp"
#include "ui/audio_tool_bar.hpp"
#include "ui/main_menu_bar.hpp"
#include "ui/main_window.hpp"
#include "ui/media_library.hpp"
#include "ui/playlist_model.hpp"
#include "ui/playlist_view.hpp"
#include "ui/preferences_dialog.hpp"
#include "ui/string.hpp"

#include <QtCore/QByteArray>
#include <QtCore/QModelIndex>
#include <QtCore/QTimer>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QUndoStack>


namespace amp {
namespace ui {

MainWindow::MainWindow(QWidget* const parent) :
    QMainWindow(parent),
    undo_stack_(new QUndoStack(this)),
    menu_bar_(new ui::MainMenuBar(undo_stack_, this)),
    tool_bar_(new ui::AudioToolBar(this)),
    info_bar_(new ui::AudioInfoBar(this)),
    media_lib_(new ui::MediaLibrary(this)),
    pl_model_(new ui::PlaylistModel(undo_stack_, this)),
    pl_view_(new ui::PlaylistView(this)),
    album_art_(new ui::AlbumArtView(this))
{
    auto createAndAddDockWidget = [this](QWidget* const widget,
                                         QString const& text,
                                         QString const& name) {
        auto dock = new QDockWidget{text, this};
        dock->setObjectName(name);
        dock->setWidget(widget);
        dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        dock->setFeatures(QDockWidget::DockWidgetMovable);
        addDockWidget(Qt::LeftDockWidgetArea, dock);
    };

    createAndAddDockWidget(album_art_, tr("Album Art"),
                           QStringLiteral("albumArtDockWidget"));
    createAndAddDockWidget(media_lib_, tr("Playlists"),
                           QStringLiteral("playlistsDockWidget"));

    auto center = new QWidget(this);
    auto layout = new QGridLayout(center);
    layout->addWidget(pl_view_, 0, 0, 1, 1);
    center->setLayout(layout);

    setCentralWidget(center);
    setStatusBar(info_bar_);
    setMenuBar(menu_bar_);
    addToolBar(tool_bar_);

    connectSignals();

    restoreGeometry(to_qbytearray(ui::main_window_geometry.load()));
    restoreState(to_qbytearray(ui::main_window_state.load()));

    pl_view_->setModel(pl_model_);
    pl_view_->setFocus();

    onSelectedPlaylistChanged();
    onPlayerAboutToStart();
    onConfigChanged();

    auto timer = new QTimer(this);
    connect(timer,   &QTimer::timeout,
            pl_view_, &PlaylistView::syncWithModel);

    timer->setSingleShot(true);
    timer->start(0);
}

void MainWindow::closeEvent(QCloseEvent* const event)
{
    ui::main_window_geometry.store(saveGeometry());
    ui::main_window_state.store(saveState());

    QMainWindow::closeEvent(event);
}

void MainWindow::onFilePreferences()
{
    PreferencesDialog prefs{this};
    connect(&prefs, &PreferencesDialog::configChanged,
            this,   &MainWindow::onConfigChanged);
    prefs.exec();
}

void MainWindow::onConfigChanged()
{
    try {
        title_format_.compile(ui::main_window_title.load());
    }
    catch (std::exception const& ex) {
        auto const text = u8string{default_window_title_format};

        title_format_.compile(text);
        ui::main_window_title.store(text);
        QMessageBox::critical(this,
                              tr("Invalid title format string"),
                              to_qstring(ex.what()));
    }
    tool_bar_->onConfigChanged();
}

void MainWindow::onPlayerAboutToStart()
{
    auto selected = media_lib_->selectedPlaylist();
    tool_bar_->onPlaylistChanged(selected.get());
}

void MainWindow::onPlayerStateChanged(audio::player_state const state)
{
    if (state == audio::player_state::stopped) {
        setWindowTitle(QString{});
    }
}

void MainWindow::onPlayerTrackChanged(media::track const& track)
{
    setWindowTitle(to_qstring(title_format_(track)));
}

void MainWindow::onPlaylistViewActivated(QModelIndex const&)
{
    if (tool_bar_->canPlay()) {
        tool_bar_->stop();
        tool_bar_->play();
    }
}

void MainWindow::onSelectedPlaylistChanged()
{
    pl_model_->setPlaylist(media_lib_->selectedPlaylist());
    pl_view_->syncWithModel();
}

inline void MainWindow::connectSignals()
{
    connect(tool_bar_,  &AudioToolBar::positionChanged,
            info_bar_,  &AudioInfoBar::onPositionChanged);
    connect(tool_bar_,  &AudioToolBar::bitRateChanged,
            info_bar_,  &AudioInfoBar::onBitRateChanged);
    connect(tool_bar_,  &AudioToolBar::playerStateChanged,
            this,       &MainWindow::onPlayerStateChanged);
    connect(tool_bar_,  &AudioToolBar::playerStateChanged,
            info_bar_,  &AudioInfoBar::onPlayerStateChanged);
    connect(tool_bar_,  &AudioToolBar::playerTrackChanged,
            this,       &MainWindow::onPlayerTrackChanged);
    connect(tool_bar_,  &AudioToolBar::playerTrackChanged,
            info_bar_,  &AudioInfoBar::onPlayerTrackChanged);
    connect(tool_bar_,  &AudioToolBar::playerTrackChanged,
            pl_view_,   &PlaylistView::onPlayerTrackChanged);
    connect(tool_bar_,  &AudioToolBar::aboutToStart,
            pl_view_,   &PlaylistView::onAboutToStart);
    connect(tool_bar_,  &AudioToolBar::aboutToStart,
            this,       &MainWindow::onPlayerAboutToStart);

    connect(menu_bar_,  &MainMenuBar::fileQuit,
            this,       &MainWindow::close);
    connect(menu_bar_,  &MainMenuBar::filePreferences,
            this,       &MainWindow::onFilePreferences);

    connect(pl_model_,  &PlaylistModel::playlistSizeChanged,
            album_art_, &AlbumArtView::onPlaylistSizeChanged);
    connect(pl_model_,  &PlaylistModel::playlistSizeChanged,
            tool_bar_,  &AudioToolBar::onPlaylistSizeChanged);
    connect(pl_model_,  &PlaylistModel::playlistSizeChanged,
            info_bar_,  &AudioInfoBar::onPlaylistSizeChanged);
    connect(menu_bar_,  &MainMenuBar::fileAddFiles,
            pl_model_,  &PlaylistModel::addFiles);
    connect(menu_bar_,  &MainMenuBar::fileAddFolder,
            pl_model_,  &PlaylistModel::addFolder);
    connect(menu_bar_,  &MainMenuBar::playbackOrderChanged,
            pl_model_,  &PlaylistModel::setPlaybackOrder);

    connect(pl_view_,   &PlaylistView::activated,
            this,       &MainWindow::onPlaylistViewActivated);
    connect(pl_view_,   &PlaylistView::trackSelected,
            album_art_, &AlbumArtView::onTrackSelected);
    connect(media_lib_, &MediaLibrary::selectedPlaylistChanged,
            this,       &MainWindow::onSelectedPlaylistChanged);
}

}}    // namespace amp::ui

