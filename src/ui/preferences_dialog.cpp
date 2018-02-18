////////////////////////////////////////////////////////////////////////////////
//
// ui/preferences_dialog.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/audio/filter.hpp>
#include <amp/audio/output.hpp>
#include <amp/functional.hpp>
#include <amp/optional.hpp>
#include <amp/range.hpp>
#include <amp/ref_ptr.hpp>
#include <amp/stddef.hpp>
#include <amp/u8string.hpp>

#include "audio/replaygain.hpp"
#include "core/config.hpp"
#include "core/registry.hpp"
#include "media/title_format.hpp"
#include "ui/preferences_dialog.hpp"
#include "ui/string.hpp"

#include <algorithm>
#include <vector>

#include <QtCore/QMetaType>
#include <QtCore/QStringList>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>


namespace amp {
namespace ui {

class PreferencesDialog::OutputTab final :
    public QWidget,
    private audio::output_session_delegate
{
public:
    explicit OutputTab(QWidget* const parent = nullptr) :
        QWidget(parent),
        output_combo(new QComboBox(this)),
        device_combo(new QComboBox(this))
    {
        output_combo->setEditable(false);
        device_combo->setEditable(false);

        void(QComboBox::* signal)(int) = &QComboBox::currentIndexChanged;
        connect(output_combo, signal,
                this, [this](int const index) {
                    auto&& data = output_combo->itemData(index);
                    setCurrentSession(data.value<u8string>());
                });

        fillOutputCombo();

        auto hbox = new QHBoxLayout;
        hbox->addWidget(new QLabel(tr("Output plugin"), this), 1);
        hbox->addWidget(output_combo, 2);

        auto grid = new QGridLayout(this);
        grid->addLayout(hbox, 0, 0);

        hbox = new QHBoxLayout;
        hbox->addWidget(new QLabel(tr("Output device"), this), 1);
        hbox->addWidget(device_combo, 2);

        grid->addLayout(hbox, 1, 0);
        setLayout(grid);
    }

    void apply()
    {
        auto const plugin_id = output_combo->currentData().value<u8string>();
        auto const device_id = device_combo->currentData().value<u8string>();
        audio::active_output_plugin.store(plugin_id);
        audio::active_output_device.store(device_id);
    }

private:
    void device_added(audio::output_device const& device) override
    {
        device_combo->addItem(to_qstring(device.name),
                              QVariant::fromValue(device.uid));
    }

    void device_removed(u8string const& uid) override
    {
        auto const index = device_combo->findData(QVariant::fromValue(uid));
        if (index != -1) {
            device_combo->removeItem(index);
        }
    }

    void default_device_changed() override
    {
    }

    void setCurrentSession(u8string const& id)
    {
        auto const factory = audio::output_factories.find(id);
        AMP_ASSERT(factory != audio::output_factories.cend());

        session = factory->create();
        session->set_delegate(this);

        auto const devices = session->get_devices();
        auto const default_device = devices->get_default_device();
        auto const current_device = audio::active_output_device.load();

        device_combo->clear();
        device_combo->addItem(tr("Default (%1)")
                              .arg(to_qstring(default_device.name)),
                              QVariant::fromValue(u8string{}));

        auto current_index = 0;
        for (auto const i : xrange(devices->get_count())) {
            auto const device = devices->get_device(i);
            device_combo->addItem(to_qstring(device.name),
                                  QVariant::fromValue(device.uid));
            if (current_device == device.uid) {
                current_index = static_cast<int>(i) + 1;
            }
        }
        device_combo->setCurrentIndex(current_index);
    }

    void fillOutputCombo()
    {
        auto const active_id = audio::active_output_plugin.load();

        for (auto&& factory : audio::output_factories) {
            output_combo->addItem(tr(factory.display_name),
                                  QVariant::fromValue(u8string{factory.id}));
            if (factory.id == active_id) {
                auto const index = output_combo->currentIndex();
                output_combo->setCurrentIndex(index);
            }
        }
    }

    QComboBox* output_combo;
    QComboBox* device_combo;
    ref_ptr<audio::output_session> session;
};


class PreferencesDialog::AppearanceTab final :
    public QWidget
{
public:
    explicit AppearanceTab(QWidget* const parent = nullptr) :
        QWidget(parent),
        title_format_edit(new QPlainTextEdit(this))
    {
        auto title_format_reset = new QPushButton(tr("Default"), this);
        connect(title_format_reset, &QPushButton::released,
                this, [this]() {
                    title_format_edit->setPlainText(
                        to_qstring(default_window_title_format));
                });

        auto const text = ui::main_window_title.load();
        title_format_edit->setPlainText(to_qstring(text));
        title_format_edit->setFont(QFont{QStringLiteral("Menlo")});

        auto title_format_label = new QLabel(tr("Title format"), this);
        title_format_label->setBuddy(title_format_edit);

        auto grid = new QGridLayout(this);
        grid->addWidget(title_format_label, 0, 0);
        grid->addWidget(title_format_reset, 0, 1);
        grid->addWidget(title_format_edit,  1, 0);
        setLayout(grid);
    }

    void apply()
    {
        auto const text = title_format_edit->toPlainText();
        ui::main_window_title.store(to_u8string(text));
    }

private:
    QPlainTextEdit* title_format_edit;
};


class PreferencesDialog::FiltersTab final :
    public QWidget
{
public:
    explicit FiltersTab(QWidget* const parent = nullptr) :
        QWidget(parent),
        filter_list(new QListWidget(this)),
        rg_preamp(new QSlider(Qt::Vertical, this)),
        rg_group(new QGroupBox(tr("Enable ReplayGain"), this)),
        rg_album(new QRadioButton(tr("Album mode"), this)),
        rg_track(new QRadioButton(tr("Track mode"), this))
    {
        auto const preset = audio::active_filter_preset.load();
        auto const end = preset.end();

        for (auto&& factory : audio::filter_factories) {
            auto item = new QListWidgetItem;
            item->setText(tr(factory.display_name));
            item->setData(Qt::UserRole,
                          QVariant::fromValue(u8string{factory.id}));

            auto const pos = std::find(preset.begin(), end, factory.id);
            item->setCheckState((pos != end) ? Qt::Checked : Qt::Unchecked);
            filter_list->addItem(item);
        }
        filter_list->setSortingEnabled(false);

        auto preamp = audio::replaygain_preamp.load();
        preamp = std::max(preamp, -15.f);
        preamp = std::min(preamp, +15.f);

        rg_preamp->setRange(-15, +15);
        rg_preamp->setSliderPosition(static_cast<int>(preamp));
        rg_preamp->setTickInterval(5);
        rg_preamp->setTickPosition(QSlider::TicksRight);

        rg_group->setCheckable(true);
        rg_group->setChecked(audio::replaygain_apply.load());

        if (audio::replaygain_album.load()) {
            rg_album->setChecked(true);
        }
        else {
            rg_track->setChecked(true);
        }

        auto vbox = new QVBoxLayout;
        vbox->addWidget(rg_track);
        vbox->addWidget(rg_album);
        rg_group->setLayout(vbox);

        auto hbox = new QHBoxLayout;
        hbox->addWidget(rg_group);
        hbox->addWidget(rg_preamp);

        vbox = new QVBoxLayout;
        vbox->addWidget(filter_list);
        vbox->addLayout(hbox);
        setLayout(vbox);
    }

    void apply()
    {
        auto const preamp = rg_preamp->sliderPosition();

        audio::replaygain_preamp.store(static_cast<float>(preamp));
        audio::replaygain_apply.store(rg_group->isChecked());
        audio::replaygain_album.store(rg_album->isChecked());

        std::vector<u8string> preset;
        for (auto const i : xrange(filter_list->count())) {
            auto&& item = *filter_list->item(i);
            if (item.checkState() == Qt::Checked) {
                preset.push_back(item.data(Qt::UserRole).value<u8string>());
            }
        }
        audio::active_filter_preset.store(preset);
    }

private:
    QListWidget*  filter_list;
    QSlider*      rg_preamp;
    QGroupBox*    rg_group;
    QRadioButton* rg_album;
    QRadioButton* rg_track;
};


PreferencesDialog::PreferencesDialog(QWidget* const parent) :
    QDialog(parent),
    output(new OutputTab(this)),
    filters(new FiltersTab(this)),
    appearance(new AppearanceTab(this))
{
    auto tabs = new QTabWidget(this);
    tabs->addTab(output,     tr("Output"));
    tabs->addTab(filters,    tr("Filters"));
    tabs->addTab(appearance, tr("Appearance"));

    auto buttons = new QDialogButtonBox{Qt::Horizontal, this};
    buttons->addButton(buttons->Ok);
    buttons->addButton(buttons->Apply);
    buttons->addButton(buttons->Cancel);

    connect(buttons, &QDialogButtonBox::clicked,
            this, [=](QAbstractButton* const which) {
                auto const role = buttons->buttonRole(which);
                if (role == QDialogButtonBox::AcceptRole) {
                    accept();
                }
                else if (role == QDialogButtonBox::ApplyRole) {
                    apply();
                }
                else if (role == QDialogButtonBox::RejectRole) {
                    reject();
                }
            });

    auto grid = new QGridLayout(this);
    grid->addWidget(tabs);
    grid->addWidget(buttons);
    grid->setSizeConstraint(QLayout::SetNoConstraint);

    setLayout(grid);
    setWindowTitle(tr("Preferences"));

    restoreGeometry(to_qbytearray(ui::preferences_geometry.load()));
    connect(this, &PreferencesDialog::finished,
            this, [this](int) {
                ui::preferences_geometry.store(saveGeometry());
            });
}

PreferencesDialog::~PreferencesDialog() = default;

void PreferencesDialog::accept()
{
    apply();
    QDialog::accept();
}

void PreferencesDialog::apply()
{
    output->apply();
    filters->apply();
    appearance->apply();

    Q_EMIT configChanged();
}

}}    // namespace amp::ui

