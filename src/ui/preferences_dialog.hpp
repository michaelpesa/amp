////////////////////////////////////////////////////////////////////////////////
//
// ui/preferences_dialog.hpp
//
////////////////////////////////////////////////////////////////////////////////


#ifndef AMP_INCLUDED_55423A86_176D_4839_AD98_6A6005AF5213
#define AMP_INCLUDED_55423A86_176D_4839_AD98_6A6005AF5213


#include <amp/stddef.hpp>
#include <amp/stddef.hpp>

#include <QtWidgets/QDialog>


namespace amp {

class u8string;


namespace ui {

class PreferencesDialog final :
    public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* = nullptr);
    virtual ~PreferencesDialog();

    void accept() override;
    void apply();

Q_SIGNALS:
    void configChanged();

private:
    class OutputTab;
    class AppearanceTab;
    class FiltersTab;

    OutputTab*     output;
    FiltersTab*    filters;
    AppearanceTab* appearance;
};

}}    // namespace amp::ui


#endif  // AMP_INCLUDED_55423A86_176D_4839_AD98_6A6005AF5213

