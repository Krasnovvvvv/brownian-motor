#include <QApplication>
#include <QStyleFactory>
#include <QIcon>

#include "gui/MainWindow.h"

int main(
    int argc,
    char* argv[]
) {
    QApplication app{argc, argv};

    if (auto *style = QStyleFactory::create("windows11")) {
        QApplication::setStyle(style);
    }

    QApplication::setWindowIcon(
        QIcon{":/icons/brownian-motor.png"}
    );

    MainWindow window;
    window.show();

    return QApplication::exec();
}