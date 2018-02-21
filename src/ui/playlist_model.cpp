////////////////////////////////////////////////////////////////////////////////
//
// ui/playlist_model.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/media/tags.hpp>
#include <amp/net/uri.hpp>
#include <amp/scope_guard.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>
#include <amp/utility.hpp>

#include "core/config.hpp"
#include "media/playlist.hpp"
#include "media/tags.hpp"
#include "media/track.hpp"
#include "ui/media_scanner.hpp"
#include "ui/playlist_model.hpp"
#include "ui/string.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QUndoCommand>
#include <QtWidgets/QUndoStack>


namespace amp {
namespace ui {

PlaylistModel::PlaylistModel(QUndoStack* const us, QObject* const parent) :
    QAbstractTableModel(parent),
    scanner(nullptr),
    undo_stack(us),
    columns{{
        { tags::album_artist, tr("Artist") },
        { tags::album,        tr("Album")  },
        { tags::disc_track,   tr("Track")  },
        { tags::title,        tr("Title")  },
        { tags::length,       tr("Length") },
    }}
{
}

PlaylistModel::~PlaylistModel()
{
    if (scanner) {
        scanner->cancel();
        delete scanner;
    }
}

int PlaylistModel::rowCount(QModelIndex const&) const
{
    return static_cast<int>(playlist->size());
}

int PlaylistModel::columnCount(QModelIndex const&) const
{
    return static_cast<int>(columns.size());
}

QVariant PlaylistModel::data(QModelIndex const& index, int const role) const
{
    if (index.isValid() && role == Qt::DisplayRole) {
        auto const key = columns[as_unsigned(index.column())].key;
        return to_qstring(tags::find(track(index), key));
    }
    return {};
}

QVariant PlaylistModel::headerData(int const section,
                                   Qt::Orientation const orientation,
                                   int const role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        AMP_ASSERT(section >= 0 && section < columnCount());
        return columns[as_unsigned(section)].name;
    }
    return {};
}

void PlaylistModel::sort(int const section, Qt::SortOrder const order)
{
    if (playlist != nullptr) {
        beginResetModel();
        AMP_SCOPE_EXIT { endResetModel(); };

        AMP_ASSERT(section >= 0 && section < columnCount());
        playlist->sort(columns[as_unsigned(section)].key,
                       (order == Qt::AscendingOrder
                        ? media::sort_order::ascending
                        : media::sort_order::descending));
    }
}

bool PlaylistModel::removeRows(int const start,
                               int const count,
                               QModelIndex const&)
{
    AMP_ASSERT(start >= 0 && "cannot remove negative row index");
    AMP_ASSERT(count >= 0 && "cannot remove negative row count");

    class RemoveCommand final :
        public QUndoCommand
    {
    public:
        RemoveCommand(int const s, int const n, PlaylistModel& m,
                      QUndoCommand* const parent = nullptr) :
            QUndoCommand(parent),
            tracks(as_unsigned(n)),
            model(m),
            start(s),
            stop(start + n)
        {
            setText(QObject::tr("Remove %1 tracks").arg(n));
        }

        void undo() override
        {
            {
                model.beginInsertRows(QModelIndex(), start, stop);
                AMP_SCOPE_EXIT { model.endInsertRows(); };
                model.playlist->insert(model.playlist->begin() + start,
                                       std::make_move_iterator(tracks.begin()),
                                       std::make_move_iterator(tracks.end()));
            }
            Q_EMIT model.playlistSizeChanged(model.playlist->size());
        }

        void redo() override
        {
            {
                model.beginRemoveRows(QModelIndex(), start, stop);
                AMP_SCOPE_EXIT { model.endRemoveRows(); };
                std::move(model.playlist->begin() + start,
                          model.playlist->begin() + stop,
                          tracks.begin());
                model.playlist->erase(start, stop);
            }
            Q_EMIT model.playlistSizeChanged(model.playlist->size());
        }

    private:
        std::vector<media::track> tracks;
        PlaylistModel& model;
        int const start;
        int const stop;
    };

    undo_stack->push(new RemoveCommand{start, count, *this});
    return true;
}

QModelIndex PlaylistModel::index(media::track const& x) const
{
    auto const row = static_cast<int>(&x - &(*playlist->cbegin()));
    return QAbstractTableModel::index(row, 0);
}

media::track const& PlaylistModel::track(QModelIndex const& index) const
{
    return playlist->at(static_cast<std::size_t>(index.row()));
}

QModelIndex PlaylistModel::currentIndex() const
{
    auto const row = static_cast<int>(playlist->position());
    return QAbstractTableModel::index(row, 0);
}

void PlaylistModel::setPlaylist(ref_ptr<media::playlist> x)
{
    {
        beginResetModel();
        AMP_SCOPE_EXIT { endResetModel(); };
        playlist = std::move(x);
    }
    Q_EMIT playlistSizeChanged(playlist->size());
}

void PlaylistModel::setPlaylistIndex(QModelIndex const& index)
{
    playlist->position(static_cast<std::size_t>(index.row()));
}

void PlaylistModel::setPlaybackOrder(media::playback_order const order)
{
    playlist->set_playback_order(order);
}

void PlaylistModel::addFiles(std::vector<net::uri> uris)
{
    launchScannerTask(std::move(uris), {});
}

void PlaylistModel::addFolder(u8string directory)
{
    launchScannerTask({}, std::move(directory));
}

AMP_NOINLINE
void PlaylistModel::launchScannerTask(std::vector<net::uri> uris,
                                      u8string directory)
{
    if (scanner) {
        scanner->cancel();
        delete std::exchange(scanner, nullptr);
    }

    scanner = new MediaScanner(this);
    connect(scanner, &MediaScanner::resultReady,
            this, [this](media::track x) {
                {
                    auto const start = rowCount();
                    beginInsertRows(QModelIndex(), start, start + 1);
                    AMP_SCOPE_EXIT { endInsertRows(); };
                    playlist->push_back(std::move(x));
                }
                Q_EMIT playlistSizeChanged(playlist->size());
            },
            Qt::QueuedConnection);

    auto dialog = new QProgressDialog;

    connect(dialog, &QProgressDialog::canceled,
            this, [this]() {
                scanner->cancel();
                delete std::exchange(scanner, nullptr);
            });

    connect(scanner, &MediaScanner::progressRangeChanged,
            dialog, &QProgressDialog::setRange,
            Qt::QueuedConnection);
    connect(scanner, &MediaScanner::progressValueChanged,
            dialog, &QProgressDialog::setValue,
            Qt::QueuedConnection);
    connect(scanner, &MediaScanner::progressTextChanged,
            dialog, &QProgressDialog::setLabelText,
            Qt::QueuedConnection);
    connect(scanner, &MediaScanner::finished,
            dialog, &QProgressDialog::deleteLater,
            Qt::QueuedConnection);

    dialog->setCancelButtonText(tr("Cancel"));
    dialog->setLabelText(tr("Loading tracks..."));
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->show();

    scanner->run(std::move(uris), std::move(directory));
    QCoreApplication::processEvents();
}

}}    // namespace amp::ui

