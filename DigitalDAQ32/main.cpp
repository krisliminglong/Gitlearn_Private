#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 注册 std::vector<int> 类型
    qRegisterMetaType<std::vector<int>>("std::vector<int>");
    qRegisterMetaType<QVector<QVector<QVector<double>>>>("QVector<QVector<QVector<double>>>");
    qRegisterMetaType<QVector<QVector<double>>>("QVector<QVector<double>>");
    qRegisterMetaType<std::vector<std::vector<uint64_t>>>("std::vector<std::vector<uint64_t>>");
    MainWindow w;
    w.show();
    return a.exec();
}
