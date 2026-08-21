#include <QApplication>
#include <QWidget>


int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget widget;
    widget.setWindowTitle("Test");
    widget.show();
    return QApplication::exec();
}