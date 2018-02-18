////////////////////////////////////////////////////////////////////////////////
//
// ui/media_library.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/error.hpp>
#include <amp/range.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include "core/filesystem.hpp"
#include "ui/media_library.hpp"
#include "ui/string.hpp"

#include <cinttypes>
#include <cstddef>
#include <iterator>

#include <QtCore/QCoreApplication>
#include <QtGui/QKeySequence>
#include <QtWidgets/QAction>
#include <QtWidgets/QListWidgetItem>


namespace amp {
namespace ui {
namespace {

inline auto playlist_directory()
{
    auto const directory = fs::get_user_directory(fs::user_directory::data);
    fs::create_directory(directory);
    return directory;
}

inline auto make_playlist(u8string const& directory, uint32 const id)
{
    auto path = u8format("%s/%08" PRIx32 ".ampl", directory.c_str(), id);
    return media::playlist::make(std::move(path), id);
}

inline auto make_playlist(uint32 const id)
{
    return make_playlist(playlist_directory(), id);
}

}     // namespace <unnamed>


MediaLibrary::MediaLibrary(QWidget* const parent) :
    QListWidget(parent)
{
    auto const directory = get_user_directory(fs::user_directory::data);
    media::playlist_index index;
    try {
        index.load(directory + "/index.dat");
    }
    catch (std::exception const& ex) {
        std::fprintf(stderr, "[AMP] failed to load playlist index: %s\n",
                     ex.what());
    }

    if (index.entries.empty()) {
        index.entries = {{ 1, 0, "Default" }};
    }

    playlists.resize(index.entries.size());
    for (auto const i : xrange(index.entries.size())) {
        auto&& entry = index.entries[i];

        playlists[i] = make_playlist(entry.uid);
        if (playlists[i]->size() > entry.pos) {
            playlists[i]->position(entry.pos);
        }

        auto item = new QListWidgetItem(to_qstring(entry.name), this);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        addItem(item);
    }

    setCurrentRow(static_cast<int>(index.selection));
    setContextMenuPolicy(Qt::ActionsContextMenu);

    connect(this, &QListWidget::itemSelectionChanged,
            this, &MediaLibrary::selectedPlaylistChanged);

    auto action = new QAction(tr("New Playlist"), this);
    action->setShortcut(Qt::CTRL + Qt::Key_N);
    connect(action, &QAction::triggered, this, &MediaLibrary::onNewPlaylist);
    addAction(action);

    action = new QAction(tr("Remove"), this);
    action->setShortcut(Qt::Key_Delete);
    action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(action, &QAction::triggered, this, &MediaLibrary::onRemove);
    addAction(action);

    action = new QAction(tr("Rename"), this);
    connect(action, &QAction::triggered, this, &MediaLibrary::onRename);
    addAction(action);

    connect(qApp, &QCoreApplication::aboutToQuit,
            this, &MediaLibrary::onAboutToQuit);
}

ref_ptr<media::playlist> MediaLibrary::selectedPlaylist()
{
    auto row = currentRow();
    if (row < 0) {
        row = 0;
    }
    if (row > static_cast<int>(playlists.size() - 1)) {
        row = static_cast<int>(playlists.size() - 1);
    }
    return playlists[static_cast<std::size_t>(row)];
}

inline uint32 MediaLibrary::newPlaylistID() const noexcept
{
    if (playlists.empty()) {
        return 1;
    }
    return playlists.back()->id() + 1;
}

void MediaLibrary::onAboutToQuit()
{
    media::playlist_index index;
    index.selection = static_cast<uint32>(currentRow());
    index.entries.resize(playlists.size());

    for (auto const i : xrange(index.entries.size())) {
        playlists[i]->save();

        auto&& entry = index.entries[i];
        entry.uid  = playlists[i]->id();
        entry.pos  = static_cast<uint32>(playlists[i]->position());
        entry.name = to_u8string(item(static_cast<int>(i))->text());
    }
    index.save(playlist_directory() + "/index.dat");
}

void MediaLibrary::onNewPlaylist()
{
    playlists.push_back(make_playlist(newPlaylistID()));

    auto item = new QListWidgetItem(tr("New Playlist"), this);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    addItem(item);
    setCurrentRow(row(item));
    editItem(item);

    Q_EMIT selectedPlaylistChanged();
}

void MediaLibrary::onRemove()
{
    for (auto const& index : selectionModel()->selectedRows()) {
        auto const pos = std::next(playlists.begin(), index.row());
        (**pos).remove();
        playlists.erase(pos);

        if (playlists.empty()) {
            playlists.push_back(make_playlist(1));
            item(index.row())->setText(tr("Default"));
            Q_EMIT selectedPlaylistChanged();
        }
        else {
            delete takeItem(index.row());
        }
    }
}

void MediaLibrary::onRename()
{
    auto const selected = selectedItems();
    if (!selected.empty()) {
        editItem(selected.front());
    }
}

}}    // namespace amp::ui

