#ifndef MONITORINGWINDOW_H
#define MONITORINGWINDOW_H

#include <QMainWindow>
#include<qcustomplot.h>
namespace Ui {
class MonitoringWindow;
}

class MonitoringWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MonitoringWindow(QWidget *parent = nullptr);
    ~MonitoringWindow();

private:
    Ui::MonitoringWindow *ui;

    QCustomPlot* customPlot;
    QCustomPlot* SpeedcustomPlot;
    QVector<QCheckBox*> bladeCheckBoxes;
    QVector<QCheckBox*> ChannelCheckBoxes;

    int numBlades;

    void WIndowsIntial();
    void WIndowsStyle();

    //-----师妹的测试函数测试函数------------------
    void updatePlotWithRandomSpeedData(QCustomPlot *SpeedcustomPlot,
                                       int channelIndex,
                                       const QVector<double> &cycleData);

public slots:
   void updateDataUI(const QVector<QVector<QVector<double>>>& vibrationData,
                     const QVector<QVector<double>>& speedData,
                     const QVector<QVector<double>>& cycleData);
   void SetParameter(int bladeCount);//初始化界面
   void ClearDraw();


};

#endif // MONITORINGWINDOW_H
