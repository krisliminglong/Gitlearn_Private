#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
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
#include<stdio.h>

#include<QThread>
#include<QTimer>
#include "qcustomplot.h"
#include "datathread.h"
#include "calculation.h"
#include "mydial.h"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , select(0)//区判断开启那些通道
    ,channelPlotStates(32, false)
    ,parametersSet(false)
    ,monitoringWindowParameterSet(false)
    ,bladeCount(0)
{
    ui->setupUi(this);
    this->setWindowTitle("叶尖定时分析（BTT）系统"); // 设置窗口的名字

//------------------绘图界面------------------------------------------------------------------
    // 初始化QTabWidget
    tabWidget = new QTabWidget(this);//用来管理主界面和监控界面
    setCentralWidget(tabWidget);

    // 初始化主界面
    QWidget *mainView = new QWidget(this);//父类是this，也就是最大的最底层的这个界面
    MainWindow_Init(mainView);//窗口初始化

    // 初始化完成后将主界面和监测界面添加到QTabWidget中
    tabWidget->addTab(mainView, tr("主界面"));

//--------------数据处理部分------------------------------------------------------------------
    datathread=new QThread;//创建了一个数据采集的线程
    working=new DataThread;//创建了一个数据采集的实列对象
    working->moveToThread(datathread);//把数据采集的实列对象移动到线程里

    datacalculate=new QThread;
    DataCollectAndCalculate=new DataCalculation;//32个叶片，1阶线性拟合是监测用，2阶是准确计算用
    DataCollectAndCalculate->moveToThread(datacalculate);

    connect(datathread, &QThread::started, working, &DataThread::dataprocced);//数据采集线程开始后，就启动数据采集函数

    //数据采集和处理线程的互联
    connect(working,&DataThread::EmitAllChannelsData,DataCollectAndCalculate,&DataCalculation::ProcessAllChannelsData);//无噪音处理
    connect(working,&DataThread::EmitAllChannelsRiseAndFallData,DataCollectAndCalculate,&DataCalculation::ProcessAllRiseAndFallData);//有噪音处理

    //启动线程
    connect(startButton,&QPushButton::clicked,this,[&]()
    {
        if (dataPath.isEmpty())  // 如果用户没有选择路径
        {
            QMessageBox::information(this, tr("提示"), tr("请选择数据保存的路径"));
            return;
        }
        if (!channelACheckbox->isChecked()&&!channelBCheckbox->isChecked()) {//如果用户没有激活通道
            QMessageBox::warning(this, "提示", "请选择激活的通道");
            return;
        }
        if (!parametersSet) {//如果没有设置参数
            QMessageBox::warning(this, "提示", "请设置所有必要参数");
            return;
          }
         datathread->start();//这个是跳出采集循环后就手动停止
         datacalculate->start();//线程没有没有关闭哦,所有我在主线程stop函数中，进行了手动停止
    });

    //设置采样率，把开始和停止按钮绑定信号
    connect(sampleRateComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(SetSample(int)));
    connect(stopButton, SIGNAL(clicked()), this, SLOT(Stop()));
    connect(startButton, SIGNAL(clicked()), this, SLOT(Begin()));
    connect(SetParamButton, SIGNAL(clicked()), this, SLOT(SetParam()));
    //通道选择的关联
    connect(channelACheckbox, &QCheckBox::stateChanged, this, &MainWindow::updateChannels);
    connect(channelBCheckbox, &QCheckBox::stateChanged, this, &MainWindow::updateChannels);

    //数据保存路径
    connect(saveButton,SIGNAL(clicked()),this,SLOT(datasaveway()));
    connect(this,&MainWindow::datasavepath,working,&DataThread::reveivepath);

    //主界面的绘图
    connect(DataCollectAndCalculate, &DataCalculation::dataReady, this, &MainWindow::updateUI);

    //监测板卡是否存在
    QTimer *timer = new QTimer(this);//设置定时器，每一秒检测一下设备
    connect(timer, SIGNAL(timeout()), this, SLOT(isDevicePresent()));
    timer->start(1500); // 每1000毫秒（1秒）检查一次设备

//===============================绘图的子窗口===================================================================================================================================================
    monitoringWindow = new MonitoringWindow(this);//单叶片监测的子界面
    wavedisplaywindow = new WaveDisplay(this);//数据回放和数据导出和回放
    parameter_identify = new parameter_identification(this);//参数识别界面

    tabWidget->addTab(monitoringWindow, tr("监测界面"));//把主界面个监控界面放一块

    QMenuBar *menuBar = this->menuBar();//菜单栏
    QMenu *viewMenu = menuBar->addMenu(tr("其他功能选项"));

    //菜单栏的样式
    menuBar->setStyleSheet("QMenuBar { background-color: rgb(173, 216, 230); color: black; font-size: 10pt; border: 1px solid black;}"
                           "QMenu { background-color: rgb(0, 255, 36); color: black; font-size: 8pt; border: 1px solid #ADD8E6;}");

    QAction *actionShowMonitoring = new QAction(tr("数据回放/导出Excel"), this);
    QAction *VibParameterIdentify = new QAction(tr("振动的参数识别"), this);


    viewMenu->addAction(actionShowMonitoring);
    viewMenu->addAction(VibParameterIdentify);

    // 连接动作的触发信号到相应的槽
    connect(actionShowMonitoring, &QAction::triggered, this, &MainWindow::showMonitoringView);//打开数据导入导出窗口
    connect(VibParameterIdentify, &QAction::triggered, this, &MainWindow::showParamView);//打开打开参数识别窗口

    //子窗口的绘图
    connect(DataCollectAndCalculate, &DataCalculation::dataReady,monitoringWindow, &MonitoringWindow::updateDataUI);

}

MainWindow::~MainWindow()
{
    delete ui;
}


// 槽函数，处理通道复选框状态变化事件
void MainWindow::channelCheckboxStateChanged(QCheckBox *checkbox, int state)
{
    QString checkboxText = checkbox->text();
    int channelIndex = channelCheckboxes.indexOf(checkbox);//获取勾选的复选框的索引
    if (channelIndex != -1)
    {
        channelPlotStates[channelIndex] = (state == Qt::Checked);
    }
}
//设置采样率
void MainWindow::SetSample(int index)
{
    Q_UNUSED(index);
    int sample = sampleRateComboBox->currentData().toInt();
    working->setSample(sample);//传给线程
}

void MainWindow::Stop()
{
    working->stop();//除了在采集数据的线程跳出循环外，还负责置零数据索引值
    datacalculate->quit();//手动停止线程
}

void MainWindow::Begin()
{
    DataCollectAndCalculate->SetIndexZero();
    working->begin();
}

void MainWindow::updateChannels()
{
    uint32_t channels = 0;
    if (channelACheckbox->isChecked()) {
        channels = 0x000000000000FFFF;
        select=10086;
    }
    if (channelBCheckbox->isChecked()) {
        channels = 0x00000000FFFF0000;
        select=10010;
    }
    if (channelACheckbox->isChecked() && channelBCheckbox->isChecked()) {
        channels = 0x00000000FFFFFFFF;
        select=10000;
    }
    working->setChannels(channels,select); // 假设 DataThread 类中有一个设置 channel
}

void MainWindow::datasaveway()
{
    // 弹出对话框让用户选择一个文件夹
   dataPath = QFileDialog::getExistingDirectory(this, tr("选择数据保存路径"), "/home",
              QFileDialog::ShowDirsOnly|QFileDialog::DontResolveSymlinks);
   savePathLabel->setText("当前文件保存路径：" + dataPath); // 设置标签的文本为 "当前文件保存路径：" + 选择的路径
   working->reveivepath(dataPath);
}


void MainWindow::updateUI(const QVector<QVector<QVector<double> > > &vibrationData,
                          const QVector<QVector<double> > &speedData,
                          const QVector<QVector<double> > &cycleData)
{
    plot2->clearPlottables();

    // 假设plot2已经正确配置了双Y轴
    for (int channel = 0; channel < vibrationData.size(); ++channel) {
        // 检查是否选中该通道,通道中是否有数据
        if (!channelPlotStates[channel]||speedData[channel].isEmpty()) continue;
        // 对每个通道的每个叶片进行遍历
        for (int leaf = 0; leaf < vibrationData[channel].size(); ++leaf) {
            // 检查尺寸是否匹配,因为发送过来的数据，里面会含有空数组，因为在计算时，是2m+1，这个有可能比叶片数要多1
            if (cycleData[channel].size() != vibrationData[channel][leaf].size())
            {
                continue;
            }

            // 创建振动位移图表
            QCPGraph* vibrationGraph = plot2->addGraph();
            vibrationGraph->setData(cycleData[channel], vibrationData[channel][leaf]);
            vibrationGraph->setPen(QPen(Qt::GlobalColor(Qt::blue + leaf % 14))); // 分配不同的颜色
        }
        for(int t=0;t<speedData[channel].size();++t)
        {
          dial->setValue(speedData[channel][t]);
        }
    }
    plot2->rescaleAxes();
    plot2->replot();


    //------------------仿真测试代码----------------------------
    for (int channel = 0; channel < vibrationData.size(); ++channel)
    {
        if (!channelPlotStates[channel]||speedData[channel].isEmpty()) continue;
        setupBarChart(plot1,bladeCount);
        setupSpectrumAnalysis(plot3,speedData[channel][0]/60);//转速的中心频率
    }

}

void MainWindow::showMonitoringView()
{
    wavedisplaywindow->show();
}

void MainWindow::showParamView()
{
    parameter_identify->show();
}

void MainWindow::SetParam()
{
    bladeCount = bladeCountLineEdit->text().toInt();
    double bladeRadius = bladeRadiusLineEdit->text().toDouble();
    int sampleRate = sampleRateComboBox->currentData().toInt();
    if(bladeCountLineEdit->text().isEmpty() || bladeRadiusLineEdit->text().isEmpty()||bladeCount==0||bladeRadius==0)
    {
        parametersSet=false;
        QMessageBox::information(this, tr("提示"), tr("请输入有效参数"));
    }
    else
    {
        parametersSet = true; // 标记参数已被设置
        DataCollectAndCalculate->SetParameter(bladeCount,bladeRadius,sampleRate);
        // 仅当monitoringWindowParameterSet为false时调用SetParameter
        if (!monitoringWindowParameterSet) {
            monitoringWindow->SetParameter(bladeCount);//创建绘图复选框
            monitoringWindowParameterSet = true; // 标记为已调用
        }
        QMessageBox::information(this, tr("提示"), tr("参数设置成功"));
    }
}

bool MainWindow::isDevicePresent()
{
    int32 lCardType;
    drv_handle hDrv=spcm_hOpen("/dev/spcm0");
    spcm_dwGetParam_i32 (hDrv, SPC_PCITYP, &lCardType);
    if (hDrv) {
        CardsTatus->setText("采集卡状态：未采集数据状态");
        spcm_vClose (hDrv);  //防止资源泄露，所以在检测后需要关闭
        return true;
    } else {
        CardsTatus->setText("采集卡状态：数据采集状态或被采集卡被其他应用占用");
        return false;
    }

}

void MainWindow::MainWindow_Init(QWidget *mainView)
{
    // 设置背景渐变
    QPalette palette;
    QLinearGradient gradient(0, 0, width(), height());
    gradient.setColorAt(0, QColor(255, 255, 255)); // 开始颜色（白色）
    gradient.setColorAt(1, QColor(150, 220, 150)); // 结束颜色（浅绿色）
    palette.setBrush(QPalette::Window, QBrush(gradient));
    setPalette(palette);//这个是设置最大的那个界面的背景颜色
    mainView->setAutoFillBackground(true);
    mainView->setPalette(palette);//这个是设置控件的背景颜色
    //主布局
    QVBoxLayout *mainLayout2 = new QVBoxLayout(mainView); // 主垂直布局，父类是mainView

    QHBoxLayout *layout2 = new QHBoxLayout; // 第二行布局，包含3个绘图工具
    QVBoxLayout *layout3 = new QVBoxLayout; // 第三行布局，包含通道勾选框和通道号

    //绘图框
    plot2 = new QCustomPlot(this);
    plot2->setInteractions(QCP::iRangeZoom | QCP::iRangeDrag);
    plot2->yAxis->setLabel("振动幅值(um)"); // 左侧Y轴标签
    plot2->xAxis->setLabel("圈数"); // X轴标签
    QCPTextElement *title2 = new QCPTextElement(plot2);
    title2->setText("叶尖定时分析界面");
    title2->setFont(QFont("sans", 10, QFont::Bold)); // 设置字体为粗体，大小为12
    // 将标题添加到图表的顶部
    plot2->plotLayout()->insertRow(0); // 在最上面插入一个新行
    plot2->plotLayout()->addElement(0, 0, title2); // 在新行中添加标题元素

    layout2->addWidget(plot2);

    QVBoxLayout *sublayout = new QVBoxLayout;
    plot1 = new QCustomPlot(this);
    plot1->setInteractions(QCP::iRangeZoom | QCP::iRangeDrag);
    plot1->xAxis->setLabel("叶片号");
    plot1->yAxis->setLabel("振动相对误差值%");
    QCPTextElement *title1 = new QCPTextElement(plot1);
    title1->setText("转速稳定性分析");
    title1->setFont(QFont("sans", 10, QFont::Bold)); // 设置字体为粗体，大小为12
    // 将标题添加到图表的顶部
    plot1->plotLayout()->insertRow(0); // 在最上面插入一个新行
    plot1->plotLayout()->addElement(0, 0, title1); // 在新行中添加标题元素
    sublayout->addWidget(plot1);

    QHBoxLayout *sublayout1 = new QHBoxLayout;
    plot3=new QCustomPlot(this);
    plot3->setInteractions(QCP::iRangeZoom | QCP::iRangeDrag);
    plot3->xAxis->setLabel("频率（Hz）");
    plot3->yAxis->setLabel("幅值");
    QCPTextElement *title = new QCPTextElement(plot3);
    title->setText("叶片平均振动频率");
    title->setFont(QFont("sans", 10, QFont::Bold)); // 设置字体为粗体，大小为12
    // 将标题添加到图表的顶部
    plot3->plotLayout()->insertRow(0); // 在最上面插入一个新行
    plot3->plotLayout()->addElement(0, 0, title); // 在新行中添加标题元素

    //速度仪表盘
    dial = new MyDial(this);
    sublayout1->addWidget(plot3);
    sublayout1->addWidget(dial);
    sublayout->addLayout(sublayout1);

    layout2->addLayout(sublayout);

    layout2->addSpacing(20); // 添加一些间距

    //第三行布局
    for (int row = 0; row < 4; ++row)
    {
        QHBoxLayout *rowLayout = new QHBoxLayout; // 每行布局
        rowLayout->setSpacing(1); // 设置行内控件之间的间距
        for (int channel = 1 + row * 8; channel <= (row + 1) * 8; ++channel) {
            QCheckBox *channelCheckbox = new QCheckBox("通道 "+QString::number(channel), this);
            channelCheckboxes.append(channelCheckbox); // 将复选框对象添加到数组中
            //将每个复选框都与状态函数关联
            connect(channelCheckbox, &QCheckBox::stateChanged, [=](int state) {
                           channelCheckboxStateChanged(channelCheckbox, state);
                       });
            rowLayout->addWidget(channelCheckbox);
        }
        layout3->addLayout(rowLayout);
    }

    // 第四行布局，包含按钮和复选框
    QHBoxLayout *layout4 = new QHBoxLayout;
    layout4->setSpacing(8);
    QVBoxLayout *checkBoxLayout = new QVBoxLayout; // 垂直布局用于复选框
    channelACheckbox = new QCheckBox("激活的通道A系列：D0-D15", this);
    channelBCheckbox = new QCheckBox("激活的通道B系列：D16-D31", this);
    channelACheckbox->setStyleSheet("border: 1px solid black;"); // 添加内部边框
    channelBCheckbox->setStyleSheet("border: 1px solid black;"); // 添加内部边框
    checkBoxLayout->addWidget(channelACheckbox);
    checkBoxLayout->addWidget(channelBCheckbox);

    startButton = new QPushButton("开始采集", this);
    saveButton = new QPushButton("数据保存", this);
    stopButton = new QPushButton("停止采集", this);

    sampleRateComboBox = new QComboBox(this);
    sampleRateComboBox->addItem("200kHZ",200*1000);
    sampleRateComboBox->addItem("500kHZ",500*1000);
    sampleRateComboBox->addItem("1MHZ",1000*1000);
    sampleRateComboBox->addItem("2MHZ",2*1000*1000);
    sampleRateComboBox->addItem("5MHZ",5*1000*1000);
    sampleRateComboBox->addItem("10MHZ",10*1000*1000);
    sampleRateComboBox->addItem("20MHZ",20*1000*1000);
    sampleRateComboBox->addItem("40MHZ",40*1000*1000);
    sampleRateComboBox->addItem("50MHZ",50*1000*1000);
    sampleRateComboBox->addItem("100MHZ",100*1000*1000);
    sampleRateComboBox->addItem("125MHZ",125*1000*1000);


    // 设置按钮的样式
    QString buttonStyle = "QPushButton { background-color: #2ecc71; color: white; border: none; padding: 8px 16px; }"
                          "QPushButton:hover { background-color: #27ae60; }";
    startButton->setStyleSheet(buttonStyle);
    saveButton->setStyleSheet(buttonStyle);
    stopButton->setStyleSheet(buttonStyle);

    QLabel *sampleRateLabel = new QLabel("采样率：", this); // 提示文本
    sampleRateLabel->setFixedSize(sampleRateLabel->fontMetrics().boundingRect(sampleRateLabel->text()).size());
    sampleRateComboBox->setStyleSheet(buttonStyle);
    layout4->addLayout(checkBoxLayout); // 添加复选框布局
    layout4->addWidget(startButton);
    layout4->addWidget(saveButton);
    layout4->addWidget(stopButton);
    layout4->addWidget(sampleRateLabel);
    layout4->addWidget(sampleRateComboBox);

    // 第五行布局，显示数据保存路径
    QHBoxLayout *savePathLayout = new QHBoxLayout;
    savePathLabel = new QLabel("数据保存路径：", this);
    savePathLabel->setFixedHeight(20);
    savePathLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    savePathLabel->setLineWidth(2);
    savePathLabel->setStyleSheet("border-color: #2ecc71;"); // 绿色边框
    savePathLayout->addWidget(savePathLabel);

    // 第六行布局，显示板卡的状态
    QHBoxLayout *Cardstatus = new QHBoxLayout;
    CardsTatus = new QLabel("采集卡的状态：", this);
    CardsTatus ->setFixedHeight(20);
    CardsTatus ->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    CardsTatus ->setLineWidth(2);
    savePathLabel->setStyleSheet("border-color: #2ecc71;"); // 绿色边框
    Cardstatus->addWidget(CardsTatus);

    //第七行，半径和叶片数目
    QHBoxLayout *bladeLayout = new QHBoxLayout;
    QLabel *bladeCountLabel = new QLabel("叶片数：", this);
    QIntValidator *intValidator = new QIntValidator(1, 1000, this); // 假设叶片数在1到100之间
    bladeCountLineEdit = new QLineEdit(this);
    bladeCountLineEdit->setValidator(intValidator);//只接收整数输入
    bladeCountLineEdit->setFixedHeight(20);

    QLabel *bladeRadiusLabel = new QLabel("叶片半径（um）：", this);
    QDoubleValidator *doubleValidator = new QDoubleValidator(0.00, 100000000.0, 2, this); // 假设半径范围和精度
    bladeRadiusLineEdit = new QLineEdit(this);
    bladeRadiusLineEdit->setValidator(doubleValidator);//只接收小数
    bladeRadiusLineEdit->setFixedHeight(20);

    SetParamButton = new QPushButton("设置参数", this);
    SetParamButton->setStyleSheet(buttonStyle);
    // 添加到布局中
    bladeLayout->addWidget(bladeCountLabel);
    bladeLayout->addWidget(bladeCountLineEdit);
    bladeLayout->addWidget(bladeRadiusLabel);
    bladeLayout->addWidget(bladeRadiusLineEdit);
    bladeLayout->addWidget(SetParamButton);


    //把所有的控件和布局添加一个主布局中
    mainLayout2->addLayout(layout2); // 添加绘图工具行布局
    mainLayout2->addLayout(layout3); // 添加通道勾选框和通道号布局
    mainLayout2->addLayout(layout4); // 添加按钮和复选框
    mainLayout2->addLayout(savePathLayout);// 添加数据保存路径
    mainLayout2->addLayout(Cardstatus);//板卡的状态
    mainLayout2->addLayout(bladeLayout);//板卡的状态

    //那主布局添加到主界面
    mainView->setLayout(mainLayout2);//全部添加到主界面
}






//--------------师妹的仿真函数---------------------------
void MainWindow::setupBarChart(QCustomPlot *customPlot,int bladeCount)
{
   customPlot->clearPlottables();
   QVector<double> tickValues;
   QVector<double> values;
   QVector<QString> labels;

   // 生成32个叶片的数据
   for (int i = 1; i <= bladeCount; ++i)
   {
       tickValues << i;
       labels << QString("叶片 %1").arg(i);
       // 在0.5至0.8之间生成随机值
       double value = 0.2 + static_cast<double>(rand()) / RAND_MAX * (0.8 - 0.5);
       values << value;
   }

   // 创建柱状图
   QCPBars *bars = new QCPBars(customPlot->xAxis, customPlot->yAxis);
   bars->setWidth(0.45);  // 设置柱状图的宽度
   bars->setData(tickValues, values);
   bars->setPen(QPen(Qt::blue));
   bars->setBrush(QColor(0, 0, 255, 50));

   // 创建自定义刻度标签
   QSharedPointer<QCPAxisTickerText> textTicker(new QCPAxisTickerText);
   for (int i = 0; i < tickValues.size(); ++i) {
       textTicker->addTick(tickValues[i], labels[i]);
   }
   customPlot->xAxis->setTicker(textTicker);
   customPlot->xAxis->setTickLabelRotation(60);  // 标签旋转，便于阅读

   // 设置坐标轴范围
   customPlot->xAxis->setRange(0, 33);  // 留出空间以显示最后一个标签
   customPlot->yAxis->setRange(0.2, 0.8);  // 叶尖间隙范围

   // 网格设置
   customPlot->xAxis->grid()->setVisible(true);
   customPlot->yAxis->grid()->setSubGridVisible(true);

   // 重绘图表
   customPlot->replot();
}


void MainWindow::setupSpectrumAnalysis(QCustomPlot *customPlot, double rollspeed)
{
    QVector<double> frequency(1024), amplitude(1024);

    int mainFrequency = static_cast<int>(rollspeed) + (rand() % 5 - 2);  // 主频率在 rollspeed - 2 到 rollspeed + 2 之间
    double peakAmplitude = 1.0 + ((rand() % 100) / 200.0);  // 主峰的随机幅值在 1.0 到 1.5 之间
    int peakWidth = 10;  // 峰宽为 10Hz
    double noiseLevel = 0.05;  // 噪声水平

    // 填充频率和振幅数据
    for (int i = 0; i < frequency.size(); ++i)
    {
        frequency[i] = mainFrequency - 512 + i;  // 频率从 mainFrequency - 512 到 mainFrequency + 511
        if (frequency[i] >= mainFrequency - peakWidth && frequency[i] <= mainFrequency + peakWidth) {
            // 创建不规则的三角峰
            double distance = abs(frequency[i] - mainFrequency);
            double peakNoise = (rand() % 100 - 50) / 1000.0; // 强化峰值附近的随机性
            double nonSymmetry = (rand() % 100) / 100.0; // 引入非对称性
            if (frequency[i] < mainFrequency) {
                // 峰左侧
                amplitude[i] = (peakAmplitude - distance / peakWidth * peakAmplitude) + peakNoise * nonSymmetry;
            } else {
                // 峰右侧
                amplitude[i] = (peakAmplitude - distance / peakWidth * peakAmplitude) + peakNoise / nonSymmetry;
            }
        } else {
            amplitude[i] = ((rand() % 100) / 1000.0) * noiseLevel;  // 其他频率添加随机噪声
        }
    }

    // 创建图形并设置数据
    customPlot->addGraph();
    customPlot->graph(0)->setData(frequency, amplitude);
    customPlot->graph(0)->setLineStyle(QCPGraph::lsLine);
    customPlot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 5));

    // 设置坐标轴
    customPlot->xAxis->setLabel("Frequency (Hz)");
    customPlot->yAxis->setLabel("Amplitude");
    customPlot->xAxis->setRange(mainFrequency - 50, mainFrequency + 50);  // 设置 x 轴范围
    customPlot->yAxis->setRange(0, 3);  // 设置 y 轴范围

    // 重绘图表
    customPlot->replot();
}

//-------------------------师妹的仿真函数到这里结束--------

