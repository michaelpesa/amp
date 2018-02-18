////////////////////////////////////////////////////////////////////////////////
//
// main.cpp
//
////////////////////////////////////////////////////////////////////////////////


#include <amp/stddef.hpp>

#include "core/config.hpp"
#include "core/registry.hpp"
#include "ui/main_window.hpp"

#include <QtWidgets/QApplication>


int main(int argc, char** argv)
{
    using namespace ::amp;

    config::register_defaults();
    load_plugins();

    QApplication app{argc, argv};
    app.setAttribute(Qt::AA_UseHighDpiPixmaps);

    ui::MainWindow mainwin;
    mainwin.show();
    return app.exec();
}

