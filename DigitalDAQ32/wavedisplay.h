#ifndef WAVEDISPLAY_H
#define WAVEDISPLAY_H

#include <QMainWindow>
#include <qcustomplot.h>
namespace Ui {
class WaveDisplay;
}

class WaveDisplay : public QMainWindow
{
    Q_OBJECT

public:
    explicit WaveDisplay(QWidget *parent = nullptr);
    ~WaveDisplay();

private:
    Ui::WaveDisplay *ui;
    QCustomPlot *customPlot;

    QLineEdit *LineEditRisingEdge;
    QPushButton *ButtonLoadRisingEdge;
    QLineEdit *LineEditFallingEdge;
    QPushButton *ButtonLoadFallingEdge;
    QPushButton *StartDataPlot;

    QLineEdit *BinFilePath;
    QLineEdit *ExcelFilePath;
    QPushButton *ButtonChoosePath;
    QPushButton *ButtonExcel;
    QPushButton *ExcelOut;
    QPushButton *ClearDraw;

    std::atomic<bool> allDataSent;//看看是不是绘制完成了，或者数据完了，或者数据到达极限了，就停止了

    void initWidgets();

    void readTimestamps(const QString &RisefilePath,const QString &FallfilePath);//数据逐个数据全绘制

    void readTimestamps1(const QString &RisefilePath,const QString &FallfilePath);//数据只绘制上升沿和下降沿以及前一个状态

    void ExportDataToExcel();//读取bin文件导出Excel
private slots:
    //————————数据回放——————————————
    void onButtonLoadRisingEdgeClicked();
    void onButtonLoadFallingEdgeClicked();
    void onStartDataPlotClicked();

    //——————数据导出————————————
    void onButtonChoosePathClicked(); //选择二进制文件
    void onButtonExcelClicked(); //选择导出Excel的路径
    void ButtonExcelOut();

    void updateYAxisRange();
    void plotData(const QVector<double>& risingEdges, const QVector<double>& fallingEdges);
    void ClearDrawPicture();
signals:
    void sendWarning(const QString &title, const QString &message);
    void dataReady(const QVector<double>& x, const QVector<double>& y);
};

#endif // WAVEDISPLAY_H
