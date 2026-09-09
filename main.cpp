#include <QApplication>
#include <QIcon>

#include "gui/MainWindow.h"

int main(
    int argc,
    char* argv[]
) {
    QApplication app{argc, argv};

    app.setWindowIcon(
        QIcon{":/icons/brownian-motor.png"}
    );

    MainWindow window;
    window.show();

    return app.exec();
}