////////////////////////////////////////////////////////////////////////////////
//
// ui/main_menu_bar.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/net/uri.hpp>
#include <amp/optional.hpp>
#include <amp/stddef.hpp>

#include "core/config.hpp"
#include "core/filesystem.hpp"
#include "core/registry.hpp"
#include "media/playlist.hpp"
#include "ui/main_menu_bar.hpp"
#include "ui/string.hpp"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include <QtCore/QString>
#include <QtGui/QKeySequence>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QAction>
#include <QtWidgets/QActionGroup>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QMenu>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QUndoStack>


namespace amp {
namespace ui {
namespace {

inline QString supportedAudioFormats()
{
    return QObject::tr("Audio files (%1)")
        .arg(to_qstring(audio::get_input_file_filter()));
}


optional<net::uri> getOpenURI(QWidget* const parent = nullptr)
{
    QDialog dialog(parent);

    auto const buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok|QDialogButtonBox::Cancel,
        &dialog);

    QObject::connect(buttons, &QDialogButtonBox::accepted,
                     &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected,
                     &dialog, &QDialog::reject);

    auto edit = new QLineEdit(&dialog);
    edit->setFont(QFont{QStringLiteral("Menlo")});

    auto label = new QLabel(dialog.tr("URL"), &dialog);
    label->setBuddy(edit);

    auto grid = new QGridLayout(&dialog);
    grid->addWidget(label,   0, 0);
    grid->addWidget(edit,    0, 1);
    grid->addWidget(buttons, 1, 0);

    dialog.setLayout(grid);
    dialog.setWindowTitle(dialog.tr("Open URL"));

    if (dialog.exec() == dialog.Accepted) {
        auto const text = to_u8string(edit->text());
        return make_optional(net::uri::from_string(text));
    }
    return nullopt;
}

}     // namespace <unnamed>


MainMenuBar::MainMenuBar(QUndoStack* const us, QWidget* const parent) :
    QMenuBar(parent),
    undo_stack(us)
{
    createFileMenu();
    createPlaybackMenu();
}

inline void MainMenuBar::createFileMenu()
{
    auto menu = new QMenu(tr("&File"), this);
    auto action = undo_stack->createUndoAction(this, tr("&Undo"));
    action->setShortcut(QKeySequence::Undo);
    menu->addAction(action);

    action = undo_stack->createRedoAction(this, tr("&Redo"));
    action->setShortcut(QKeySequence::Redo);
    menu->addAction(action);
    menu->addSeparator();

    action = new QAction(tr("Add file(s)"), this);
    connect(action, &QAction::triggered, this, &MainMenuBar::onFileAddFiles);
    menu->addAction(action);

    action = new QAction(tr("Add folder"), this);
    action->setShortcut(QKeySequence::Open);
    connect(action, &QAction::triggered, this, &MainMenuBar::onFileAddFolder);
    menu->addAction(action);

    action = new QAction(tr("Add network"), this);
    connect(action, &QAction::triggered, this, &MainMenuBar::onFileAddNetwork);
    menu->addAction(action);
    menu->addSeparator();

    action = new QAction(tr("Preferences"), this);
    action->setShortcut(QKeySequence::Preferences);
    connect(action, &QAction::triggered, this, &MainMenuBar::filePreferences);
    menu->addAction(action);
    menu->addSeparator();

    action = new QAction(tr("Quit"), this);
    action->setShortcut(QKeySequence::Quit);
    connect(action, &QAction::triggered, this, &MainMenuBar::fileQuit);
    menu->addAction(action);
    addMenu(menu);
}

inline void MainMenuBar::createPlaybackMenu()
{
    using media::playback_order;

    auto group = new QActionGroup(this);
    auto add = [&](playback_order const order,
                   char const*    const text,
                   bool           const checked = false) {
        auto action = group->addAction(tr(text));
        action->setCheckable(true);
        action->setChecked(checked);

        connect(action, &QAction::triggered,
                this, [=]{ Q_EMIT playbackOrderChanged(order); });
    };

    add(playback_order::linear, "Linear", true);
    add(playback_order::random, "Random");
    add(playback_order::repeat, "Repeat");

    auto menu = new QMenu(tr("&Playback"), this);
    menu->addActions(group->actions());
    addMenu(menu);
}

inline void MainMenuBar::onFileAddFiles()
{
    auto const selected = QFileDialog::getOpenFileNames(
        this, tr("Add file(s)"),
        to_qstring(ui::add_files_history.load()),
        supportedAudioFormats());

    if (selected.isEmpty()) {
        return;
    }

    auto to_uri = [](QString const& s) {
        return net::uri::from_file_path(to_u8string(s));
    };

    std::vector<net::uri> uris(static_cast<std::size_t>(selected.size()));
    std::transform(selected.cbegin(), selected.cend(), uris.begin(), to_uri);

    ui::add_files_history.store(fs::parent_path(uris.front().get_file_path()));
    Q_EMIT fileAddFiles(std::move(uris));
}

inline void MainMenuBar::onFileAddFolder()
{
    auto const selected = QFileDialog::getExistingDirectory(
        this, tr("Add directory"),
        to_qstring(ui::add_folder_history.load()));

    if (selected.isEmpty()) {
        return;
    }

    auto path = to_u8string(selected);
    ui::add_folder_history.store(path);
    Q_EMIT fileAddFolder(std::move(path));
}

inline void MainMenuBar::onFileAddNetwork()
{
    if (auto location = getOpenURI(this)) {
        Q_EMIT fileAddFiles(std::vector<net::uri>{std::move(*location)});
    }
}

}}    // namespace amp::ui

