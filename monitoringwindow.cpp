#include "monitoringwindow.h"
#include "ui_monitoringwindow.h"

MonitoringWindow::MonitoringWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MonitoringWindow),
    numBlades(0)
{
    ui->setupUi(this);
    this->setWindowTitle("监测界面"); // 设置窗口的名字
    this->setMinimumSize(600,400);

}

MonitoringWindow::~MonitoringWindow()
{
    delete ui;
}

void MonitoringWindow::updateDataUI(const QVector<QVector<QVector<double> > > &vibrationData, const QVector<QVector<double> > &speedData, const QVector<QVector<double> > &cycleData)
{
    // 遍历每个通道的数据
     for (int channelIndex = 0; channelIndex < speedData.size(); ++channelIndex)
     {
            // 检查数据有效性
            if (cycleData[channelIndex].isEmpty() || speedData[channelIndex].isEmpty()) {
                continue; // 如果当前通道没有数据，则跳过
            }
            // 在更新数据之前，检查对应的复选框是否勾选
            if (!ChannelCheckBoxes[channelIndex]->isChecked())
            {
                SpeedcustomPlot->graph(channelIndex)->data()->clear(); // 清空数据
                SpeedcustomPlot->graph(channelIndex)->setVisible(false); // 如果未勾选，则隐藏图形
                continue;
            }
            else
            {
                SpeedcustomPlot->graph(channelIndex)->setVisible(true); // 如果勾选，则显示图形
            }
            // ------转速曲线画图-------------------------
            SpeedcustomPlot->graph(channelIndex)->addData(cycleData[channelIndex], speedData[channelIndex]);   // 更新数据

            for (int BladelIndex = 0; BladelIndex < vibrationData[channelIndex].size(); ++BladelIndex)
                 {
                        // 确保通道有数据
                        if (cycleData[channelIndex].isEmpty() || vibrationData[channelIndex][BladelIndex].isEmpty())
                        {
                            continue; // 如果当前通道没有数据，则跳过
                        }
                        // 在更新数据之前，检查对应的复选框是否勾选
                        if (!bladeCheckBoxes[BladelIndex]->isChecked())
                        {
                           customPlot->graph(BladelIndex)->data()->clear(); // 清空数据
                           customPlot->graph(BladelIndex)->setVisible(false); // 如果未勾选，则隐藏图形
                           continue;
                        }
                        else
                        {
                           customPlot->graph(BladelIndex)->setVisible(true); // 如果勾选，则显示图形
                        }

                        // 准备数据
                        QVector<double> bladeKeyData = cycleData[channelIndex]; // 圈数数据
                        QVector<double> bladeValueData = vibrationData[channelIndex][BladelIndex]; // 振动位移数据
                        customPlot->graph(BladelIndex)->addData(bladeKeyData, bladeValueData); // 更新数据

                  }

      }
        // 重绘图表以显示更新后的数据
        SpeedcustomPlot->rescaleAxes(true);
        SpeedcustomPlot->replot(QCustomPlot::rpQueuedReplot);
        customPlot->rescaleAxes(true);
        customPlot->replot(QCustomPlot::rpQueuedReplot);

}

void MonitoringWindow::SetParameter(int bladeCount)
{
    numBlades=bladeCount;
    WIndowsIntial();//界面初始化
    WIndowsStyle();
}

void MonitoringWindow::ClearDraw()
{
    for (int i = 0; i < customPlot->graphCount(); ++i)
    {
        customPlot->graph(i)->data()->clear();
    }

    // 同样的操作对于SpeedcustomPlot
    for (int i = 0; i < SpeedcustomPlot->graphCount(); ++i)
    {
        SpeedcustomPlot->graph(i)->data()->clear();
    }
    // 之后，您可能想要重绘图表以更新界面
    customPlot->replot();
    SpeedcustomPlot->replot();
}


void MonitoringWindow::WIndowsIntial()
{
 //————————————圈数-叶片转速曲线————————————————————————————————————————————————————————
    customPlot=new QCustomPlot(this);
    customPlot->xAxis->setLabel("圈数");
    customPlot->yAxis->setLabel("振动位移(um)");
    customPlot->axisRect()->setupFullAxesBox();    //四边安上坐标轴
    customPlot->setInteractions(QCP::iRangeZoom | QCP::iRangeDrag);//使得坐标轴可以缩放
    for (int i = 0; i <  numBlades; ++i) {
        customPlot->addGraph();//为每个叶片都创建图层
        QPen pen;
        pen.setColor(QColor(rand() % 256, rand() % 256, rand() % 256));
        customPlot->graph(i)->setPen(pen); // 将笔应用到当前图层上
    }
    customPlot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

//————————————圈数-转速曲线——————————————————————————————————————————————————————————
    SpeedcustomPlot=new QCustomPlot(this);
    SpeedcustomPlot->xAxis->setLabel("圈数");
    SpeedcustomPlot->yAxis->setLabel("转速");
    SpeedcustomPlot->axisRect()->setupFullAxesBox();    //四边安上坐标轴
    SpeedcustomPlot->setInteractions(QCP::iRangeZoom | QCP::iRangeDrag);
    SpeedcustomPlot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);//自由拉伸

    // 创建用于存放通道复选框的网格布局
    QGridLayout *checkboxGridLayout = new QGridLayout();
    for (int i = 0; i < 16; ++i) {
            SpeedcustomPlot->addGraph();
            QColor color = QColor(rand() % 255, rand() % 255, rand() % 255);
            SpeedcustomPlot->graph(i)->setPen(QPen(color));
            QCheckBox *checkBox = new QCheckBox(QString("通道 %1").arg(i + 1), this);
            ChannelCheckBoxes.append(checkBox);
            checkBox->setChecked(false); // 默认不勾选
            int row = i / 8; // 计算行号
            int col = i % 8; // 计算列号
            checkboxGridLayout->addWidget(checkBox, row, col);
        }


    QHBoxLayout *mainLayout = new QHBoxLayout;//第一级布局是水平布局
    this->centralWidget()->setLayout(mainLayout);

    QVBoxLayout *DrawLayout = new QVBoxLayout;
    DrawLayout->addWidget(customPlot);
    DrawLayout->addWidget(SpeedcustomPlot);
    DrawLayout->addLayout(checkboxGridLayout);
    mainLayout->addLayout(DrawLayout);//第二级

    // 创建一个垂直布局来存储所有叶片复选框列的复选框
    QVBoxLayout *buttonAndCheckBox=new  QVBoxLayout();
    QHBoxLayout *checkboxColumnsLayout = new QHBoxLayout();//这个和绘图工具都是第二级
    QPushButton *clearDraw=new QPushButton("清空图像", this);
    clearDraw->setMaximumHeight(30); // 设置按钮的最大高度，数值根据需要调整

    buttonAndCheckBox->addLayout(checkboxColumnsLayout);
    buttonAndCheckBox->addWidget(clearDraw);
    mainLayout->addLayout(buttonAndCheckBox); // 将复选框列的布局添加到主布局

    const int bladesPerColumn = 16;  // 根据叶片数量创建多个垂直布局，每个布局包含至多10个复选框
    int numColumns = (numBlades + bladesPerColumn - 1) / bladesPerColumn; // 计算需要的列数
    for (int columnIndex = 0; columnIndex < numColumns; ++columnIndex)
    {   // 创建每列的垂直布局，并在每个布局中添加最多16个复选框
        QVBoxLayout *columnLayout = new QVBoxLayout;//这个是第三级
        int startBladeIndex = columnIndex * bladesPerColumn;  // 计算当前列应该添加的复选框数量
        int endBladeIndex = std::min(startBladeIndex + bladesPerColumn, numBlades);
        for (int i = startBladeIndex; i < endBladeIndex; ++i) {
            QCheckBox *checkBox = new QCheckBox(tr("叶片 %1").arg(i + 1), this);
            columnLayout->addWidget(checkBox);
            bladeCheckBoxes.append(checkBox);
        }
        // 如果最后一列不满16个复选框，则补充添加空白的标签或透明的复选框以对齐
        if (columnIndex == numColumns - 1 && (endBladeIndex % bladesPerColumn) != 0) {
            int missingBoxes = bladesPerColumn - (endBladeIndex % bladesPerColumn);
            for (int i = 0; i < missingBoxes; ++i) {
                QLabel *emptyLabel = new QLabel(this);
                emptyLabel->setFixedHeight(20); // 设置与复选框相同的高度
                columnLayout->addWidget(emptyLabel);
            }
        }
        checkboxColumnsLayout->addLayout(columnLayout); // 将列布局添加到复选框列的水平布局
    }

    connect(clearDraw, SIGNAL(clicked()), this, SLOT(ClearDraw()));//清空图像
}

void MonitoringWindow::WIndowsStyle()
{
    QString windowStyle = "background-color: #323232; color: #d6d6d6;";
    this->setStyleSheet(windowStyle);

    // 设置图表风格
    QString graphStyle = "QCustomPlot {"
                         "background-color: #212121;"
                         "border: 1px solid #444;"
                         "}";
    customPlot->setStyleSheet(graphStyle);
    SpeedcustomPlot->setStyleSheet(graphStyle);

}














//---------师妹的测试函数测试函数------------------
void MonitoringWindow::updatePlotWithRandomSpeedData(QCustomPlot *SpeedcustomPlot, int channelIndex, const QVector<double> &cycleData)
{
   QVector<double> randomSpeedData;  // 新的速度数据数组
   randomSpeedData.reserve(cycleData.size());  // 预分配足够的空间

   // 生成在3000附近波动的速度数据
   for (int i = 0; i < cycleData.size(); ++i) {
       int randomOffset = rand() % 11 - 5;  // 生成 -5 到 +5 之间的随机数
       double speedValue = 3000 + randomOffset;  // 在3000±5范围内生成速度值
       randomSpeedData.push_back(speedValue);  // 添加到速度数据数组
   }

   // 添加更新数据到图表
   SpeedcustomPlot->graph(channelIndex)->addData(cycleData, randomSpeedData);
   SpeedcustomPlot->rescaleAxes(true);
   SpeedcustomPlot->replot();  // 重新绘制图表以显示更新的数据
}
