#include "wavedisplay.h"
#include "ui_wavedisplay.h"
#include "xlsxdocument.h"
WaveDisplay::WaveDisplay(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::WaveDisplay),
    allDataSent (false) // 标志所有数据是否已发送完毕
{
    ui->setupUi(this);
    this->setWindowTitle("数据回放/数据导出");

    //------界面初始化-----------------------------------
    initWidgets();//初始化窗口

    // ---------数据回放---------------------------------
    connect(ButtonLoadRisingEdge, &QPushButton::clicked, this, &WaveDisplay::onButtonLoadRisingEdgeClicked);
    connect(ButtonLoadFallingEdge, &QPushButton::clicked, this, &WaveDisplay::onButtonLoadFallingEdgeClicked);
    connect(StartDataPlot, &QPushButton::clicked, this, &WaveDisplay::onStartDataPlotClicked);
    connect(ClearDraw,&QPushButton::clicked,this,&WaveDisplay::ClearDrawPicture);

    // ----------数据导出为Excel--------------------------
    connect(ButtonChoosePath, &QPushButton::clicked, this, &WaveDisplay::onButtonChoosePathClicked);
    connect(ButtonExcel, &QPushButton::clicked, this, &WaveDisplay::onButtonExcelClicked);
    connect(ExcelOut,&QPushButton::clicked,this,&WaveDisplay::ButtonExcelOut);

    //传递一些提示信息
    connect(this, &WaveDisplay::sendWarning, this, [](const QString &title, const QString &message) {
        QMessageBox::warning(nullptr, title, message, QMessageBox::Ok);
    });//传递一些信息到主线程

    connect(this,&WaveDisplay::dataReady,this,&WaveDisplay::plotData);

    // 保持Y轴范围在0到1，不管X轴如何变化
    connect(customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(updateYAxisRange()));
}

WaveDisplay::~WaveDisplay()
{
    delete ui;
}

void WaveDisplay::initWidgets()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 创建并配置布局
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // 创建QCustomPlot控件
    customPlot = new QCustomPlot();
    customPlot->setMinimumSize(600, 400);  // 设置最小尺寸
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    // 创建文件路径输入和按钮
    LineEditRisingEdge = new QLineEdit();
    ButtonLoadRisingEdge = new QPushButton("选择上升沿bin文件");
    LineEditFallingEdge = new QLineEdit();
    ButtonLoadFallingEdge = new QPushButton("选择下降沿bin文件");

    // 创建布局
    QHBoxLayout *risingLayout = new QHBoxLayout();
    risingLayout->addWidget(new QLabel("上升沿bin文件路径:"));
    risingLayout->addWidget(LineEditRisingEdge);
    risingLayout->addWidget(ButtonLoadRisingEdge);

    QHBoxLayout *fallingLayout = new QHBoxLayout();
    fallingLayout->addWidget(new QLabel("下降沿bin文件路径:"));
    fallingLayout->addWidget(LineEditFallingEdge);
    fallingLayout->addWidget(ButtonLoadFallingEdge);

    QVBoxLayout *kongjian1=new QVBoxLayout();
    kongjian1->addLayout(risingLayout);
    kongjian1->addLayout(fallingLayout);
    StartDataPlot = new QPushButton("点击生成");
    ClearDraw = new QPushButton("清空图像");

    QHBoxLayout *kongjian2=new QHBoxLayout();
    kongjian2->addLayout(kongjian1);
    QVBoxLayout *kongjian5=new QVBoxLayout();
    kongjian5->addWidget(StartDataPlot);
    kongjian5->addWidget( ClearDraw);
    kongjian2->addLayout(kongjian5);

    BinFilePath=new QLineEdit();
    ButtonChoosePath=new QPushButton("选择导出的文件");
    ExcelFilePath=new QLineEdit();
    ButtonExcel=new QPushButton("选择导出的路径");

    QHBoxLayout *BinFile = new QHBoxLayout();
    BinFile->addWidget(new QLabel("需要导出的bin文件:"));
    BinFile->addWidget(BinFilePath);
    BinFile->addWidget(ButtonChoosePath);

    QHBoxLayout *ExcelFile = new QHBoxLayout();
    ExcelFile->addWidget(new QLabel("导出Excel的路径:"));
    ExcelFile->addWidget(ExcelFilePath);
    ExcelFile->addWidget(ButtonExcel);

    QVBoxLayout *kongjian3=new QVBoxLayout();
    kongjian3->addLayout(BinFile);
    kongjian3->addLayout(ExcelFile);

    QHBoxLayout *kongjian4=new QHBoxLayout();
    ExcelOut = new QPushButton("点击导出");
    kongjian4->addLayout(kongjian3);
    kongjian4->addWidget(ExcelOut);


    // 将控件添加到主布局
    mainLayout->addWidget(customPlot);

    QLabel *separatorLabel = new QLabel("———————————————数据回放功能———————————————");
    separatorLabel->setAlignment(Qt::AlignCenter); // 居中对齐
    separatorLabel->setFixedHeight(20);
    mainLayout->addWidget(separatorLabel);

    mainLayout->addLayout(kongjian2);

    QLabel *separatorLabel1 = new QLabel("————————————导出数据为Excel功能——————————");
    separatorLabel1->setAlignment(Qt::AlignCenter); // 居中对齐
    separatorLabel1->setFixedHeight(20);
    mainLayout->addWidget(separatorLabel1);

    mainLayout->addLayout(kongjian4);


    // 设置样式
    QString styleSheet = "QWidget { color: white; background-color: black; }"
                         "QPushButton { "
                         "    background-color: lightgreen; "
                         "    color: white; "
                         "    border: 2px solid white; "
                         "    border-radius: 10px; "
                         "    padding: 5px; "
                         "}"
                         "QPushButton:hover { "
                         "    background-color: lime; "
                         "    border: 2px solid yellow; "
                         "}"
                         "QLineEdit { background-color: gray; }";

    centralWidget->setStyleSheet(styleSheet);
}



//——————————————数据回放的函数——————————————————————

void WaveDisplay::readTimestamps(const QString &RisefilePath, const QString &FallfilePath)
{
    QFile fileRise(RisefilePath);
    QFile fileFall(FallfilePath);
    if (!fileRise.open(QIODevice::ReadOnly) || !fileFall.open(QIODevice::ReadOnly)) {
        emit sendWarning("警告", "无法打开BIN文件！");
        return;
    }
   QDataStream inRise(&fileRise), inFall(&fileFall);
   inRise.setByteOrder(QDataStream::LittleEndian);
   inFall.setByteOrder(QDataStream::LittleEndian);

   QVector<quint64> risingEdges, fallingEdges;
   quint64 riseTimestamp, fallTimestamp;
   const int blockSize = 50000;//一次最多5000000个数据
   quint64 currentTime = 0;//记录时间点
   bool currentState = false;//起始状态定为0
   QVector<double> x, y;
   while (!inRise.atEnd() || !inFall.atEnd())
   {
      risingEdges.clear();
      fallingEdges.clear();
      for (int i = 0; i < blockSize && !inRise.atEnd(); i++) {
          inRise >> riseTimestamp;
          risingEdges.append(riseTimestamp);
      }
      for (int i = 0; i < blockSize && !inFall.atEnd(); i++) {
          inFall >> fallTimestamp;
          fallingEdges.append(fallTimestamp);
      }
      quint64 xMax = qMax(!risingEdges.isEmpty() ? risingEdges.last() : 0,
                          !fallingEdges.isEmpty() ? fallingEdges.last() : 0);
      int riseIndex = 0, fallIndex = 0; // 上升沿和下降沿索引
      while (currentTime <= xMax)
      {
          // 检查是否到达上升沿
          if (riseIndex < risingEdges.size() && currentTime == risingEdges[riseIndex]) {
              currentState = true; // 状态变为高电平
              riseIndex++;
          }

          // 检查是否到达下降沿
          if (fallIndex < fallingEdges.size() && currentTime == fallingEdges[fallIndex]) {
              currentState = false; // 状态变为低电平
              fallIndex++;
          }

          // 添加当前状态的值
          x.push_back(static_cast<double>(currentTime));
          y.push_back(currentState ? 1.0 : 0.0);

          // 下一个时间单位
          currentTime++;
      }
   }

   QElapsedTimer elapsedTimer;
   elapsedTimer.start(); // 启动计时器

   while(!allDataSent)
   {
     try {
           if (elapsedTimer.elapsed() >= 1000) // 每隔500ms发送一次数据
           {
               int pointsToSend = qMin(10000, x.size()); // 取缓冲区中的数据点数量和50000的最小值
               if (pointsToSend > 0) {
                   QVector<double> xToSend, yToSend;
                   for (int i = 0; i < pointsToSend; ++i) {
                       xToSend.append(x[i]);
                       yToSend.append(y[i]);
                   }
                   emit dataReady(xToSend, yToSend); // 发送数据
                   x.erase(x.begin(), x.begin() + pointsToSend); // 清空已发送的数据
                   y.erase(y.begin(), y.begin() + pointsToSend);
               }
               elapsedTimer.restart(); // 重新计时
           }
           if (x.empty() || y.empty())
           {
               allDataSent.store(true);
               break;
           }
       }
   catch (const std::exception& e)
    {
       qDebug() << "Exception caught:" << e.what();
       break; // Or continue, depending on how you want to handle the exception
    }

   }
   fileRise.close();
   fileFall.close();
   qDebug()<<"循环结束";
}

void WaveDisplay::readTimestamps1(const QString &RisefilePath, const QString &FallfilePath)
{
    QFile fileRise(RisefilePath);
    QFile fileFall(FallfilePath);
    if (!fileRise.open(QIODevice::ReadOnly) || !fileFall.open(QIODevice::ReadOnly)) {
        emit sendWarning("警告", "无法打开BIN文件！");
        return;
    }
    QDataStream inRise(&fileRise), inFall(&fileFall);
    inRise.setByteOrder(QDataStream::LittleEndian);
    inFall.setByteOrder(QDataStream::LittleEndian);

    quint64 riseTimestamp, fallTimestamp;
    QVector<double> timePoints, stateValues;
    const int maxPoints = 4000000; // Qcustomplot绘制的最大点数为 400 万

    // 读取所有上升沿和下降沿时间戳
    while (!inRise.atEnd() || !inFall.atEnd())
    {
        if (!inRise.atEnd()) {
            inRise >> riseTimestamp;
            // 添加上升沿之前的状态点
            timePoints.push_back(static_cast<double>(riseTimestamp) - 0.1);
            stateValues.push_back(0.0);
            // 添加上升沿的点
            timePoints.push_back(static_cast<double>(riseTimestamp));
            stateValues.push_back(1.0);
        }
        if (!inFall.atEnd()) {
            inFall >> fallTimestamp;
            // 添加下降沿之前的状态点
            timePoints.push_back(static_cast<double>(fallTimestamp) - 0.1);
            stateValues.push_back(1.0);
            // 添加下降沿的点
            timePoints.push_back(static_cast<double>(fallTimestamp));
            stateValues.push_back(0.0);
        }
        if(timePoints.size()>4000000)
        {
            qDebug()<<"已经超过4000000";
            break;
        }
    }

    fileRise.close();
    fileFall.close();

    allDataSent.store(false);//每次调用都需要重置

    if(timePoints.size() >= maxPoints)
    {
        QElapsedTimer elapsedTimer;
        elapsedTimer.start(); // 启动计时器
        while(!allDataSent)
        {
            if (elapsedTimer.elapsed() >= 500) // 每隔500ms发送一次数据
            {
                int pointsToSend = qMin(100000, timePoints.size()); // 取缓冲区中的数据点数量和100000的最小值
                if (pointsToSend > 0)
                {
                    QVector<double> xToSend, yToSend;
                    for (int i = 0; i < pointsToSend; ++i) {
                        xToSend.append(timePoints[i]);
                        yToSend.append(stateValues[i]);
                    }
                    emit dataReady(xToSend, yToSend); // 发送数据
                    stateValues.erase(stateValues.begin(), stateValues.begin() + pointsToSend); // 清空已发送的数据
                    timePoints.erase(timePoints.begin(), timePoints.begin() + pointsToSend);
                }
                elapsedTimer.restart(); // 重新计时
            }
            if (timePoints.empty() || stateValues.empty())
            {
                allDataSent.store(true);
                break;
            }
        }
    }
    else
    {
        emit dataReady(timePoints, stateValues);  // 发送处理好的数据
    }
}

void WaveDisplay::plotData(const QVector<double>& risingEdges, const QVector<double>& fallingEdges) {

   if(allDataSent)
   {
       return;//防呆处理
   }
    //不重复创建
   if (!customPlot->graph(0))
   {
    customPlot->addGraph();
    customPlot->xAxis->setLabel("Time");
    customPlot->yAxis->setLabel("State");
    customPlot->yAxis->setRange(-1.1, 1.1);
   }
   //这些需要处理一些错误异常，因为有时候addData是把数据复制到一块内存中，如果数据过多，会抛出bad_alloc
   try{
        customPlot->graph(0)->addData(risingEdges, fallingEdges);
      }
   catch (const std::exception& e)
      {
         allDataSent.store(true);  //数据到达绘图软件极限
         emit sendWarning("警告", "绘图数据已达最大值");
      }
    customPlot->graph(0)->rescaleAxes(true);
    customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void WaveDisplay::ClearDrawPicture()
{
    if(customPlot->graph(0))
    {
      customPlot->graph(0)->data()->clear();
      customPlot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void WaveDisplay::updateYAxisRange()
{
    customPlot->yAxis->setRange(-1, 2); // 设置Y轴始终保持在0到1的范围
    customPlot->replot(QCustomPlot::rpQueuedReplot); // 立即重新绘制图形
}

void WaveDisplay::onButtonLoadRisingEdgeClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择上升沿文件"), "", tr("Binary Files (*.bin)"));
    if (!filePath.isEmpty())
    {
        LineEditRisingEdge->setText(filePath);
    }
}

void WaveDisplay::onButtonLoadFallingEdgeClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择下降沿文件"), "", tr("Binary Files (*.bin)"));
    if (!filePath.isEmpty())
    {
        LineEditFallingEdge->setText(filePath);
    }
}

void WaveDisplay::onStartDataPlotClicked()
{
    if (LineEditRisingEdge->text().isEmpty() || LineEditFallingEdge->text().isEmpty())
    {
        QMessageBox::warning(this, "警告", "文件不全，请选择完整的文件路径！", QMessageBox::Ok);
        return;
    }

    // 使用 std::thread 异步读取时间戳数据
    std::thread readThread(&WaveDisplay::readTimestamps1, this, LineEditRisingEdge->text(), LineEditFallingEdge->text());
    readThread.detach(); // 让线程自由运行
}
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————



//——————————————数据导出功能的函数————————————————————————————————————————————————————————————————————————————————
void WaveDisplay::onButtonChoosePathClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择导出的文件"), "", tr("Binary Files (*.bin)"));
    if (!filePath.isEmpty())
    {
        BinFilePath->setText(filePath);
    }
}

void WaveDisplay::onButtonExcelClicked()
{
    QString folderPath = QFileDialog::getExistingDirectory(this, "选择导出的文件夹", QDir::homePath());
    if (!folderPath.isEmpty())
    {
        ExcelFilePath->setText(folderPath);
    }
}

void WaveDisplay::ExportDataToExcel()
{
    // 获取基路径
    QString baseDir = ExcelFilePath->text();

    // 获取当前时间并格式化为字符串
    QString currentTime = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString folderName = "ExcelExport_" + currentTime;

    // 在基路径下创建新文件夹
    QDir dir(baseDir);
    if (!dir.exists(folderName)) {
        dir.mkpath(folderName); // 创建文件夹
    }

    QFile binFile(BinFilePath->text());
    if (!binFile.open(QIODevice::ReadOnly)) {
        emit sendWarning("警告", "无法打开BIN文件！");
        return;
    }

    QDataStream in(&binFile);
    in.setByteOrder(QDataStream::LittleEndian);

    int fileIndex = 1; // 文件序号
    const int batchSize = 1000000; // 每个文件包含的数据量最多1000000行
    QVector<quint64> batchTimestamps;

    while (!in.atEnd()) {
        batchTimestamps.clear();
        // 构建每个Excel文件的完整路径
        QString excelFilePath = QDir(dir.absoluteFilePath(folderName)).absoluteFilePath(QString("exported_data_%1.xlsx").arg(fileIndex));

        QXlsx::Document xlsx;
        int row = 1;
        int column = 1;

        for (int i = 0; i < batchSize && !in.atEnd(); ++i) {
            quint64 timestamp;
            in >> timestamp;
            batchTimestamps.append(timestamp);
        }

        for (quint64 ts : batchTimestamps) {
            xlsx.write(row++, column, static_cast<double>(ts));
            if (row > 1000000) {
                row = 1;
                ++column;
            }
        }

        if (!xlsx.saveAs(excelFilePath)) {//在判断时，已经在执行保存的动作了
            emit sendWarning("错误", "无法保存Excel文件！");
            return;
        }

        fileIndex++; // 准备创建下一个文件
    }
    emit sendWarning("完成", "数据导出完成");
}

void WaveDisplay::ButtonExcelOut()
{
    std::thread exportThread(&WaveDisplay::ExportDataToExcel, this);
    exportThread.detach();  // 确保线程可以自由运行，不需要等待主线程
}


