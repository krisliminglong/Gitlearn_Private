#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qRegisterMetaType<std::vector<int>>("std::vector<int>");
    qRegisterMetaType<QVector<QVector<QVector<double>>>>("QVector<QVector<QVector<double>>>");
    qRegisterMetaType<QVector<QVector<double>>>("QVector<QVector<double>>");
    qRegisterMetaType<std::vector<std::vector<uint64_t>>>("std::vector<std::vector<uint64_t>>");
    MainWindow w;
    w.show();
    return a.exec();
    //远端修改V4.0

    //本地修改V5.0
    //远端修改V5.0

    //远端修改V6.0




    //本地修改V6.0
}
