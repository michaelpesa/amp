////////////////////////////////////////////////////////////////////////////////
//
// ui/playlist_view.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/error.hpp>
#include <amp/stddef.hpp>

#include "core/config.hpp"
#include "media/track.hpp"
#include "ui/playlist_model.hpp"
#include "ui/playlist_view.hpp"
#include "ui/string.hpp"
#include "ui/track_info_dialog.hpp"

#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QModelIndex>
#include <QtCore/QModelIndexList>
#include <QtCore/QSize>
#include <QtGui/QKeyEvent>
#include <QtGui/QKeySequence>
#include <QtWidgets/QAction>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QStyledItemDelegate>


namespace amp {
namespace ui {
namespace {

class PlaylistItemDelegate final :
    public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(QStyleOptionViewItem const&,
                   QModelIndex const&) const override
    {
        return QSize{0, 20};
    }
};

}     // namespace <unnamed>


PlaylistView::PlaylistView(QWidget* const parent) :
    QTreeView(parent)
{
    setItemDelegate(new ui::PlaylistItemDelegate(this));
    setSelectionMode(QAbstractItemView::ContiguousSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setContextMenuPolicy(Qt::ActionsContextMenu);

    setSortingEnabled(true);
    setUniformRowHeights(true);

    setRootIsDecorated(false);
    setItemsExpandable(false);
    setExpandsOnDoubleClick(false);

    header()->setSortIndicatorShown(true);
    header()->setStretchLastSection(true);

    auto action = new QAction(tr("Get Info"), this);
    action->setShortcut(Qt::CTRL + Qt::Key_I);
    connect(action, &QAction::triggered, this, &PlaylistView::onGetInfo);
    addAction(action);

#if defined(__APPLE__) && defined(__MACH__)
    action = new QAction(tr("Show in Finder"), this);
    connect(action, &QAction::triggered, this, &PlaylistView::onShowInFinder);
    addAction(action);
#endif

    action = new QAction(tr("Remove"), this);
    action->setShortcut(Qt::Key_Delete);
    action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(action, &QAction::triggered, this, &PlaylistView::onRemove);
    addAction(action);

    header()->restoreState(to_qbytearray(ui::playlist_header_state.load()));

    connect(this, &PlaylistView::clicked,
            this, &PlaylistView::selectIndex);
    connect(qApp, &QCoreApplication::aboutToQuit,
            this, [this]() {
                ui::playlist_header_state.store(header()->saveState());
            });
}

void PlaylistView::syncWithModel()
{
    auto const index = model()->currentIndex();
    if (index.isValid()) {
        setCurrentIndex(index);
        scrollTo(index, QAbstractItemView::PositionAtCenter);
        selectIndex(index);
    }
}

void PlaylistView::onAboutToStart()
{
    auto const index = currentIndex();
    if (index.isValid()) {
        model()->setPlaylistIndex(index);
        selectIndex(index);
    }
}

void PlaylistView::onPlayerTrackChanged(media::track const& x)
{
    auto const index = model()->index(x);
    if (index.isValid() && index != currentIndex()) {
        setCurrentIndex(index);
        selectIndex(index);
    }
}

void PlaylistView::keyPressEvent(QKeyEvent* const ev)
{
    auto const prev = currentIndex();
    QTreeView::keyPressEvent(ev);
    auto const curr = currentIndex();

    if (curr.isValid()) {
        if (ev->key() == Qt::Key_Enter || ev->key() == Qt::Key_Return) {
            model()->setPlaylistIndex(curr);
            Q_EMIT activated(curr);
        }
        else if (prev != curr) {
            selectIndex(curr);
        }
    }
}

void PlaylistView::selectIndex(QModelIndex const& index)
{
    Q_EMIT trackSelected(model()->track(index));
}

void PlaylistView::onGetInfo()
{
    auto const index = currentIndex();
    if (index.isValid()) {
        auto const& track = model()->track(index);
        auto const dialog = new ui::TrackInfoDialog(track, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose, true);
        dialog->show();
    }
}

void PlaylistView::onRemove()
{
    auto const indices = selectionModel()->selectedRows();
    if (!indices.empty()) {
        model()->removeRows(indices.front().row(), indices.size());
    }
}

AMP_INLINE PlaylistModel* PlaylistView::model() const noexcept
{
    return static_cast<PlaylistModel*>(QTreeView::model());
}

}}    // namespace amp::ui

