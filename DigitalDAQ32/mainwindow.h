#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "qcustomplot.h"
#include "datathread.h"
//----driver includes----
#include "../c_header/dlltyp.h"
#include "../c_header/regs.h"
#include "../c_header/spcerr.h"
#include "../c_header/spcm_drv.h"
#include "../c_header/spectrum.h"
// ----- include of common example librarys -----
#include "../common/spcm_lib_card.h"
#include "../common/spcm_lib_data.h"
#include "../common/spcm_lib_thread.h"
#include "../common/ostools/spcm_ostools.h"

#include<ringbuffer.h>
#include "calculation.h"
#include "monitoringwindow.h"
#include "mydial.h"
#include "wavedisplay.h"
#include "parameter_identification.h"
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    int bladeCount;

    MyDial *dial;
    QCustomPlot *plot1;
    QCustomPlot *plot2;
    QCustomPlot *plot3;

    QVector<QCheckBox*> channelCheckboxes;//用来存储复选框的状态的容器

    QThread* datathread;//采集线程
    DataThread* working;

    QThread* datacalculate;//计算线程
    DataCalculation* DataCollectAndCalculate;

    QCheckBox *channelACheckbox;//通道0至15的通道复选框
    QCheckBox *channelBCheckbox;//通道16至31的通道复选框

    QPushButton *startButton;
    QPushButton *saveButton;
    QPushButton *stopButton;
    QPushButton *SetParamButton;

    QComboBox *sampleRateComboBox;//采样率设置

    QString dataPath;  // 创建一个QString变量来保存用户选择的路径
    QLabel *savePathLabel;//数据保存路径
    QLabel *CardsTatus;//采集卡的状态
    QLineEdit *bladeCountLineEdit;//叶片的数目
    QLineEdit *bladeRadiusLineEdit;//叶片半径
    QVector<bool> channelPlotStates;//存储复选框的状态
    uint32_t select;//传递选择参数   
    bool parametersSet;//用来跟踪参数是否被正确的设置并传入计算线程

    QTabWidget *tabWidget;

    //第一个窗口初始化
    void MainWindow_Init(QWidget *mainView);

    //第二窗口
    MonitoringWindow *monitoringWindow;
    bool monitoringWindowParameterSet;// 添加这个成员变量,是为了让绘图界面只初始化一次

    //第三窗口
    WaveDisplay *wavedisplaywindow;

    //第四窗口
    parameter_identification *parameter_identify;

private slots:
    bool isDevicePresent();//监测板卡的存在
    void channelCheckboxStateChanged(QCheckBox *checkbox, int state);//定义一个槽函数，把复选框和信号值作为参数
    void SetSample(int index);//设置采样率
    void Stop();//设置开始
    void Begin();//设置开始
    void updateChannels();//传输激活通道的数量
    void datasaveway();//发送数据路径
    void updateUI(const QVector<QVector<QVector<double>>>& vibrationData,
                  const QVector<QVector<double>>& speedData,
                  const QVector<QVector<double>>& cycleData);

    void showMonitoringView();
    void showParamView();
    void SetParam();//这个函数是数据计算类初始化使用的

 signals:
    void datasavepath(const QString& path);//传参信号


public:
 //------------------师妹的绘图函数---------------------
    void setupBarChart(QCustomPlot *customPlot,int bladeCount);//绘制柱状图
    void setupSpectrumAnalysis(QCustomPlot *customPlot,double rollspeed);//绘制频谱分析的图


};

#endif // MAINWINDOW_H
