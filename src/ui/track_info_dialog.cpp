////////////////////////////////////////////////////////////////////////////////
//
// ui/track_info_dialog.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/flat_map.hpp>
#include <amp/media/tags.hpp>
#include <amp/stddef.hpp>
#include <amp/type_traits.hpp>

#include "core/config.hpp"
#include "media/tags.hpp"
#include "media/track.hpp"
#include "ui/string.hpp"
#include "ui/track_info_dialog.hpp"

#include <iterator>
#include <utility>

#include <QtCore/QAbstractTableModel>
#include <QtCore/QLocale>
#include <QtCore/QString>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QTreeView>


namespace amp {
namespace ui {
namespace {

class TableModel final :
    public QAbstractTableModel
{
public:
    explicit TableModel(flat_map<QString, QString> t,
                        QObject* const parent = nullptr) :
        QAbstractTableModel{parent},
        table_{std::move(t)}
    {}

    int columnCount(QModelIndex const& = QModelIndex()) const override
    { return 2; }

    int rowCount(QModelIndex const& = QModelIndex()) const override
    { return static_cast<int>(table_.size()); }

    QVariant data(QModelIndex const& index, int const role) const override
    {
        if (role == Qt::DisplayRole && index.isValid()) {
            auto&& row = *std::next(table_.begin(), index.row());
            return index.column() == 0 ? row.first : row.second;
        }
        return QVariant{};
    }

private:
    flat_map<QString, QString> table_;
};


inline QString to_display(u8string const& key)
{
    if (auto const name = tags::display_name(key)) {
        return QObject::tr(name);
    }
    return QStringLiteral("«%1»").arg(to_qstring(key));
}


QWidget* makeModelTab(flat_map<QString, QString> table,
                      QWidget* const parent = nullptr)
{
    auto widget = new QWidget(parent);
    auto tree = new QTreeView(widget);

    tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree->setHeaderHidden(true);
    tree->setUniformRowHeights(true);
    tree->setSortingEnabled(false);
    tree->setItemsExpandable(false);
    tree->setExpandsOnDoubleClick(false);
    tree->setModel(new TableModel(std::move(table)));
    tree->resizeColumnToContents(0);
    tree->resizeColumnToContents(1);

    auto layout = new QGridLayout(widget);
    layout->addWidget(tree);
    widget->setLayout(layout);
    return widget;
}

QWidget* makeFieldTab(u8string const& text, QFont const& font,
                      QWidget* const parent = nullptr)
{
    auto widget = new QWidget(parent);
    auto edit = new QTextEdit(widget);

    edit->setReadOnly(true);
    edit->setFont(font);
    edit->setText(to_qstring(text));

    auto layout = new QGridLayout(widget);
    layout->addWidget(edit);
    widget->setLayout(layout);
    return widget;
}

}     // namespace <unnamed>


void TrackInfoDialog::addMetadataTab(media::track const& track)
{
    flat_map<QString, QString> table;
    for (auto&& entry : track.tags) {
        if (entry.first != tags::cue_sheet &&
            entry.first != tags::lyrics) {
            table.emplace(to_display(entry.first),
                          to_qstring(entry.second));
        }
    }

    if (table.empty()) {
        return;
    }

    auto merge = [&](auto const key_part, auto const key_total) {
        auto part = table.find(to_display(key_part));
        if (part != table.cend()) {
            auto total = table.find(to_display(key_total));
            if (total != table.cend()) {
                part->second += u'/';
                part->second += total->second;
                table.erase(total);
            }
        }
    };

    merge(tags::track_number, tags::track_total);
    merge(tags::disc_number,  tags::disc_total);
    tabs->addTab(makeModelTab(std::move(table), tabs), tr("Metadata"));
}

void TrackInfoDialog::addPropertyTab(media::track const& track)
{
    flat_map<QString, QString> table;
    auto const& loc = QLocale::system();

    table.emplace(QObject::tr("Channel count"),
                  to_qstring(popcnt(track.channel_layout)));

    table.emplace(QObject::tr("Sample rate"),
                  QObject::tr("%1 Hz")
                  .arg(loc.toString(track.sample_rate)));

    table.emplace(QObject::tr("Length"),
                  QObject::tr("%1 (%2 frames)")
                  .arg(to_qstring(tags::find(track, tags::length)))
                  .arg(loc.toString(track.frames)));

    table.emplace(QObject::tr("File name"),
                  to_qstring(tags::find(track, tags::file_name)));

    table.emplace(QObject::tr("Directory"),
                  to_qstring(tags::find(track, tags::directory)));

    for (auto&& entry : track.info) {
        table.emplace(to_display(entry.first),
                      to_qstring(entry.second));
    }
    tabs->addTab(makeModelTab(std::move(table), tabs), tr("Properties"));
}

void TrackInfoDialog::addCueSheetTab(media::track const& track)
{
    if (auto const found = tags::find(track, tags::cue_sheet)) {
        auto const monospace = QFont{QStringLiteral("Menlo")};
        tabs->addTab(makeFieldTab(found, monospace, tabs), tr("Cue sheet"));
    }
}

void TrackInfoDialog::addLyricsTab(media::track const& track)
{
    if (auto const found = tags::find(track, tags::lyrics)) {
        tabs->addTab(makeFieldTab(found, QFont(), tabs), tr("Lyrics"));
    }
}


TrackInfoDialog::TrackInfoDialog(media::track const& track,
                                 QWidget* const parent) :
    QDialog(parent),
    tabs(new QTabWidget{this})
{
    tabs->setMovable(true);

    addMetadataTab(track);
    addPropertyTab(track);
    addCueSheetTab(track);
    addLyricsTab(track);

    auto grid = new QGridLayout{this};
    grid->addWidget(tabs);
    setLayout(grid);
    setWindowTitle(tr("Track Info"));

    restoreGeometry(to_qbytearray(ui::track_info_geometry.load()));

    connect(this, &QDialog::finished,
            this, [this](int) {
                ui::track_info_geometry.store(saveGeometry());
            });
}

}}    // namespace amp::ui

