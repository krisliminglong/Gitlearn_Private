#include "parameter_identification.h"
#include "ui_parameter_identification.h"
#include "xlsxdocument.h"

parameter_identification::parameter_identification(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::parameter_identification),
    HaveBaisPlot(new QCustomPlot(this)),
    NoBaisPlot(new QCustomPlot(this)),
    samplingRate(0.0),
    bladeCount(0),
    radius(0.0),
    sensor1Angle(0.0),
    sensor2Angle(0.0),
    sensor3Angle(0.0)
{
    ui->setupUi(this);
    this->setWindowTitle("振动参数识别"); // 设置窗口的名字

    // Create a QTabWidget，这个是用来管理多个Widget
    QTabWidget *tabWidget = new QTabWidget(this);
    FirstWidgetInitial(tabWidget);//第一个控件初始化函数

    SecondWidgetInitial(tabWidget);//第二个控件初始化函数

    ThirdWidgetInitial(tabWidget);
    // Set the QTabWidget as the central widget
    setCentralWidget(tabWidget);

    riseFilePaths.resize(3);
    fallFilePaths.resize(3);
}

parameter_identification::~parameter_identification()
{
    delete ui;
}

//读取二进制文件
std::vector<uint64_t> parameter_identification::readBinFile(const QString &filePath)
{
    std::vector<uint64_t> data;
    std::ifstream file(filePath.toStdString(), std::ios::binary);
    if (file.is_open())
    {
        uint64_t value;
        while (file.read(reinterpret_cast<char*>(&value), sizeof(value)))
        {
            data.push_back(value);
        }
        file.close();
    }
    return data;
}

//通道1的数据
void parameter_identification::selectSensor1File()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择传感器1的bin文件"), "", tr("Bin Files (*.bin)"));
    if (!filePath.isEmpty())
    {
        sensor1PathEdit->setText(filePath);
        sensor1FilePath = filePath;
    }
}

//通道2的数据
void parameter_identification::selectSensor2File()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择传感器2的bin文件"), "", tr("Bin Files (*.bin)"));
    if (!filePath.isEmpty())
    {
        sensor2PathEdit->setText(filePath);
        sensor2FilePath = filePath;
    }
}

//通道3的数据
void parameter_identification::selectSensor3File()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择传感器3的bin文件"), "", tr("Bin Files (*.bin)"));
    if (!filePath.isEmpty())
    {
        sensor3PathEdit->setText(filePath);
        sensor3FilePath = filePath;
    }
}

//设置参数并读取bin文件中的数据
void parameter_identification::setParameters()
{
    if (samplingRateEdit->text().isEmpty()|| bladeCountEdit->text().isEmpty()||radiusEdit->text().isEmpty()||
        sensor1PathEdit->text().isEmpty() || sensor2PathEdit->text().isEmpty()||sensor3PathEdit->text().isEmpty())
    {
        QMessageBox::warning(this, tr("参数设置错误"), tr("所有参数均为必填项"));
        return;
    }
    if (samplingRateEdit == nullptr || bladeCountEdit == nullptr || radiusEdit == nullptr ||
        sensor1PathEdit == nullptr || sensor2PathEdit == nullptr || sensor3PathEdit == nullptr)
   {
       qDebug() << "UI elements not initialized";
       return;
   }
    samplingRate = samplingRateEdit->text().toDouble();
    bladeCount = bladeCountEdit->text().toInt();
    radius = radiusEdit->text().toDouble();

    bladeComboBox->clear();
    for (int i = 1; i <= bladeCount; ++i)//从第二个开始初始化，因为第一个叶片已经存在
    {  //往复选框中添加叶片号
       bladeComboBox->addItem(tr("叶片%1").arg(i));
    }

    // 定义一个读取文件的函数
    auto readFiles = [this]()
    {
        sensor1Data = readBinFile(sensor1FilePath);
        sensor2Data = readBinFile(sensor2FilePath);
        sensor3Data = readBinFile(sensor3FilePath);

        if (sensor1Data.empty() || sensor2Data.empty() || sensor3Data.empty())
        {
            emit fileReadError(tr("数组中没有数据"));
            return;
        }
        else
        {
            emit fileReadCompleted(tr("参数设置与文件读取完成"));
        }
    };

    // 使用std::thread启动读取文件的任务
    std::thread readThread(readFiles);
    readThread.detach(); // 分离线程以便后台运行


    qDebug() << "采样率:" << samplingRate;
    qDebug() << "叶片数:" << bladeCount;
    qDebug() << "半径:" << radius;
    qDebug() << "传感器1文件路径:" << sensor1FilePath;
    qDebug() << "传感器2文件路径:" << sensor2FilePath;
    qDebug() << "传感器3文件路径:" << sensor3FilePath;
}

//调用calculation中的函数计算所有通道的所有叶片的振动
void parameter_identification::calculateVibrationDisplacement()
{
    if (sensor1Data.empty() || sensor2Data.empty() || sensor3Data.empty()
        ||samplingRateEdit->text().isEmpty()
        ||bladeCountEdit->text().isEmpty()
        ||radiusEdit->text().isEmpty())
    {
        emit DataNotEnough(tr("计算振动的参数不足"));
        return;
    }
    //使用智能指针，自动释放内存，防止内存泄漏
    auto CalVibFunction1 = std::make_shared<DataCalculation>();
    auto CalVibFunction2 = std::make_shared<DataCalculation>();
    auto CalVibFunction3 = std::make_shared<DataCalculation>();
    //对类进行初始化
    CalVibFunction1.get()->SetParameter(bladeCount,radius,samplingRate);
    CalVibFunction2.get()->SetParameter(bladeCount,radius,samplingRate);
    CalVibFunction3.get()->SetParameter(bladeCount,radius,samplingRate);
    /*这是一个叫lambda 表达式，接收DataCalculation、std::vector<uint64_t>这2个参数，
      并调用 CalVibFunction类中的函数进行计算*/
    auto calculate = [this](std::shared_ptr<DataCalculation> CalVibFunction,
                            std::vector<uint64_t> data, int sensorIndex)
    {
        int sampling = static_cast<int>(samplingRate);
        CalVibFunction->VibParaIdentify(data, bladeCount, sampling, radius);
        emit vibCalCompleted(tr("传感器 %1 的振动位移计算完毕").arg(sensorIndex + 1));
     };


    //这个了是将3个类的信号与槽连接，把数据传入进来
    connect(CalVibFunction1.get(), &DataCalculation::SendVibData,this,
            [this](const QVector<QVector<double>> &vibration1,
                   const QVector<QVector<double>> &Speed_Frenquence,
                   const QVector<QVector<double>> &Roll_Cycle_Number,
                   const QVector<QVector<double>> &Remove_Constant_Bias_vib,
                   const QVector<QVector<double>> &Remove_Bias_origin_vib)
       {
           sensor1Vibration = vibration1;
           sensor1Speed = Speed_Frenquence;
           sensor1RollCycle = Roll_Cycle_Number;
           sensor1RemoveBias = Remove_Constant_Bias_vib;
           sensor1_Original_Bias=Remove_Bias_origin_vib;

           sensor1VibMax.resize(sensor1Vibration.size());
           sensor1VibMin.resize(sensor1Vibration.size());

           for (int i = 0; i < sensor1Vibration.size(); ++i)
           {
               sensor1VibMax[i] = *std::max_element(sensor1_Original_Bias[i].begin()+1, sensor1_Original_Bias[i].end());
               sensor1VibMin[i] = *std::min_element(sensor1_Original_Bias[i].begin()+1, sensor1_Original_Bias[i].end());
           }
       });

   connect(CalVibFunction2.get(), &DataCalculation::SendVibData,this,
           [this](const QVector<QVector<double>> &vibration1,
                  const QVector<QVector<double>> &Speed_Frenquence,
                  const QVector<QVector<double>> &Roll_Cycle_Number,
                  const QVector<QVector<double>> &Remove_Constant_Bias_vib,
                  const QVector<QVector<double>> &Remove_Bias_origin_vib)
       {
           sensor2Vibration = vibration1;
           sensor2Speed = Speed_Frenquence;
           sensor2RollCycle = Roll_Cycle_Number;
           sensor2RemoveBias = Remove_Constant_Bias_vib;
           sensor2_Original_Bias=Remove_Bias_origin_vib;

           sensor2VibMax.resize(sensor2_Original_Bias.size());
           sensor2VibMin.resize(sensor2_Original_Bias.size());

           for (int i = 0; i < sensor2_Original_Bias.size(); ++i)
           {
               sensor2VibMax[i] = *std::max_element(sensor2_Original_Bias[i].begin()+1, sensor2_Original_Bias[i].end());
               sensor2VibMin[i] = *std::min_element(sensor2_Original_Bias[i].begin()+1, sensor2_Original_Bias[i].end());
           }
       });

   connect(CalVibFunction3.get(), &DataCalculation::SendVibData,this,
           [this](const QVector<QVector<double>> &vibration1,
                  const QVector<QVector<double>> &Speed_Frenquence,
                  const QVector<QVector<double>> &Roll_Cycle_Number,
                  const QVector<QVector<double>> &Remove_Constant_Bias_vib,
                  const QVector<QVector<double>> &Remove_Bias_origin_vib)
       {
           sensor3Vibration = vibration1;
           sensor3Speed = Speed_Frenquence;
           sensor3RollCycle = Roll_Cycle_Number;
           sensor3RemoveBias = Remove_Constant_Bias_vib;
           sensor3_Original_Bias=Remove_Bias_origin_vib;

           sensor3VibMax.resize(sensor3_Original_Bias.size());
           sensor3VibMin.resize(sensor3_Original_Bias.size());

           for (int i = 0; i < sensor3_Original_Bias.size(); ++i)
           {
               sensor3VibMax[i] = *std::max_element(sensor3_Original_Bias[i].begin()+1, sensor3_Original_Bias[i].end());
               sensor3VibMin[i] = *std::min_element(sensor3_Original_Bias[i].begin()+1, sensor3_Original_Bias[i].end());
           }
       });

    auto trimData = [](std::vector<uint64_t> &data, size_t front, int back)
    {
      if (data.size() > front + back)
      {
          data.erase(data.begin(), data.begin() + front);
          data.erase(data.end() - back, data.end());
      }
     };

      std::vector<uint64_t> trimmedSensor1Data = sensor1Data;
      std::vector<uint64_t> trimmedSensor2Data = sensor2Data;
      std::vector<uint64_t> trimmedSensor3Data = sensor3Data;

      trimData(trimmedSensor1Data, 2400, 1400);
      trimData(trimmedSensor2Data, 2400, 1400);
      trimData(trimmedSensor3Data, 2400, 1400);

      std::thread sensor1Thread(calculate, CalVibFunction1, std::move(trimmedSensor1Data),0);
      std::thread sensor2Thread(calculate, CalVibFunction2, std::move(trimmedSensor2Data),1);
      std::thread sensor3Thread(calculate, CalVibFunction3, std::move(trimmedSensor3Data),2);

      sensor1Thread.detach();
      sensor2Thread.detach();
      sensor3Thread.detach();
}

//绘图函数-单叶片or全叶片
void parameter_identification::BladeVidDataDraw()
{
    int sensorIndex = sensorComboBox->currentIndex();
    int bladeIndex = bladeComboBox->currentIndex();

    if (allBladesRadioButton->isChecked())//如果全叶片勾选，调用全叶片绘制函数
    {
        plotAllBlades(sensorIndex);
    }
    else if (singleBladeRadioButton->isChecked())//如果单叶片勾选，调用单叶片绘制函数
    {
        plotSingleBlade(sensorIndex, bladeIndex);
    }
}

//参数设置界面的图像
void parameter_identification::clearPlots()
{
    HaveBaisPlot->clearGraphs();
    HaveBaisPlot->replot();

    NoBaisPlot->clearGraphs();
    NoBaisPlot->replot();
}

//皮尔相关性系数
Eigen::MatrixXd parameter_identification::PiErXiShu(const Eigen::VectorXd &x, const Eigen::VectorXd &y)
{
    Eigen::VectorXd x_mean = x.array() - x.mean();
    Eigen::VectorXd y_mean = y.array() - y.mean();

    double numerator = (x_mean.array() * y_mean.array()).sum();
    double denominator = std::sqrt((x_mean.array().square().sum()) * (y_mean.array().square().sum()));

    Eigen::MatrixXd result(2, 2);
    result(0, 0) = 1.0;
    result(1, 1) = 1.0;
    result(0, 1) = numerator / denominator;
    result(1, 0) = numerator / denominator;

    return result;
}

void parameter_identification::exportSingleBlade(int sensorIndex, int bladeIndex)
{
    QVector<QVector<double>>* vibrationData = nullptr;
    QVector<QVector<double>>* speedData = nullptr;

    switch (sensorIndex)
    {
        case 0:
            vibrationData = &sensor1Vibration;
            speedData = &sensor1Speed;
            break;
        case 1:
            vibrationData = &sensor2Vibration;
            speedData = &sensor2Speed;
            break;
        case 2:
            vibrationData = &sensor3Vibration;
            speedData = &sensor3Speed;
            break;
        default:
            return;
    }

    if (!vibrationData || vibrationData->isEmpty() || bladeIndex >= vibrationData->size())
    {
        QMessageBox::warning(this, tr("导出错误"), tr("无效的叶片数据"));
        return;
    }

    QString dirPath = QFileDialog::getExistingDirectory(this, tr("选择保存目录"), QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dirPath.isEmpty()) {
        return; // 如果用户取消了选择，直接返回
    }

    QString fileName = QString("%1/Blade%2_Vibration.bin").arg(dirPath).arg(bladeIndex + 1);
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(this, tr("导出错误"), tr("无法打开文件进行写入"));
        return;
    }
    file.write(reinterpret_cast<const char*>(vibrationData->at(bladeIndex).data()), vibrationData->at(bladeIndex).size() * sizeof(double));
    file.close();

    fileName = QString("%1/Blade%2_Speed.bin").arg(dirPath).arg(bladeIndex + 1);
    file.setFileName(fileName);
    if (!file.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(this, tr("导出错误"), tr("无法打开文件进行写入"));
        return;
    }
    file.write(reinterpret_cast<const char*>(speedData->at(bladeIndex).data()), speedData->at(bladeIndex).size() * sizeof(double));
    file.close();

    QMessageBox::information(this, tr("导出成功"), tr("单叶片数据导出成功"));
}


void parameter_identification::exportAllBlades(int sensorIndex)
{
    QVector<QVector<double>>* vibrationData = nullptr;
    QVector<QVector<double>>* speedData = nullptr;

    switch (sensorIndex)
    {
       case 0:
           vibrationData = &sensor1Vibration;
           speedData = &sensor1Speed;
           break;
       case 1:
           vibrationData = &sensor2Vibration;
           speedData = &sensor2Speed;
           break;
       case 2:
           vibrationData = &sensor3Vibration;
           speedData = &sensor3Speed;
           break;
       default:
           return;
    }

    if (!vibrationData || vibrationData->isEmpty())
    {
        QMessageBox::warning(this, tr("导出错误"), tr("无效的叶片数据"));
        return;
    }

    QString dirPath = QFileDialog::getExistingDirectory(this, tr("选择保存目录"), QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dirPath.isEmpty()) {
        return; // 如果用户取消了选择，直接返回
    }

    QDir dir(dirPath + "/AllBlades");
    if (!dir.exists())
    {
        dir.mkpath(".");
    }

    for (int i = 0; i < vibrationData->size(); ++i)
    {
        QString fileName = QString("%1/Blade%2_Vibration.bin").arg(dir.path()).arg(i + 1);
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly))
        {
            QMessageBox::warning(this, tr("导出错误"), tr("无法打开文件进行写入: %1").arg(fileName));
            continue;
        }
        file.write(reinterpret_cast<const char*>(vibrationData->at(i).data()), vibrationData->at(i).size() * sizeof(double));
        file.close();
    }

    QString fileName = QString("%1/AllBlades_Speed.bin").arg(dir.path());
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(this, tr("导出错误"), tr("无法打开文件进行写入"));
        return;
    }

    // 假设所有叶片的速度数据大小一致
    int dataSize = speedData->at(0).size();
    QVector<double> averageSpeeds(dataSize, 0.0);

    for (int i = 0; i < dataSize; ++i)
    {
        for (int j = 0; j < speedData->size(); ++j)
        {
            averageSpeeds[i] += speedData->at(j).at(i);
        }
        averageSpeeds[i] /= speedData->size();
        averageSpeeds[i] /= 60.0; // 转换为频率
    }

    file.write(reinterpret_cast<const char*>(averageSpeeds.data()), averageSpeeds.size() * sizeof(double));
    file.close();

    QMessageBox::information(this, tr("导出成功"), tr("全叶片数据导出成功"));
}


void parameter_identification::exportData()
{
    int sensorIndex = sensorComboBox->currentIndex();
    int bladeIndex = bladeComboBox->currentIndex();

    if (allBladesRadioButton->isChecked()) // 如果全叶片勾选
    {
       exportAllBlades(sensorIndex);
    }
    else if (singleBladeRadioButton->isChecked()) // 如果单叶片勾选
    {
       exportSingleBlade(sensorIndex, bladeIndex);
    }
}


//计算安装的角度
void parameter_identification::CalSensorAngle()
{
    if (sensor1Data.empty() || sensor2Data.empty() || sensor3Data.empty()
        ||samplingRateEdit->text().isEmpty() || bladeCountEdit->text().isEmpty()
        ||radiusEdit->text().isEmpty() ||sensor1Vibration.empty()||
          sensor2Vibration.empty()||sensor3Vibration.empty())
    {
        emit CalAngle(tr("请先计算叶片的振动幅值"));
        return;
    }
    int nprobe = 3;
    int blade_num = bladeCount;
    int m = std::floor(blade_num / 2.0);
    double Radius = radius;
    int sampling = static_cast<int>(samplingRate);

    // 振动数据，第一层是传感器，第二层是叶片，第三层是数据点
    QVector<QVector<QVector<double>>> vibration3 = {sensor1Vibration, sensor2Vibration, sensor3Vibration};

    // 转速数据，第一层是传感器，第二层是叶片，第三层是转速
    QVector<QVector<QVector<double>>> speed3 = {sensor1Speed, sensor2Speed, sensor3Speed};

    // 修剪后的传感器数据
    auto trimData = [](std::vector<uint64_t> &data, size_t front, size_t back)
    {
        if (data.size() > front + back)
        {
            data.erase(data.begin(), data.begin() + front);
            data.erase(data.end() - back, data.end());
        }
    };

    std::vector<uint64_t> trimmedSensor1Data = sensor1Data;
    std::vector<uint64_t> trimmedSensor2Data = sensor2Data;
    std::vector<uint64_t> trimmedSensor3Data = sensor3Data;

    //static int a=2400;
    trimData(trimmedSensor1Data, 2400, 1400);
    trimData(trimmedSensor2Data, 2400, 1400);
    trimData(trimmedSensor3Data, 2403, 1400);
    //a++;
    //qDebug()<<"移动的值为"<<a;
    std::vector<std::vector<uint64_t>> trimmedData = {trimmedSensor1Data, trimmedSensor2Data, trimmedSensor3Data};

    if (nprobe == 3)
    {
        std::vector<std::vector<double>> degreeblade2(blade_num, std::vector<double>(nprobe, 0.0));//存放3各传感器的振动均值

        int nrl = 20; // 以20圈为基准计算一次，去计算各叶片20圈的振动位移平均值

        for (int iprobe = 0; iprobe < nprobe; ++iprobe)
        {
            for (int i = 0; i < blade_num; ++i)
            {
                double sum = 0.0;
                for (int j = 1; j <=nrl; ++j) // 从第2圈开始到第21圈
                {
                    sum += vibration3[iprobe][i][j];
                }
                degreeblade2[i][iprobe] = sum / nrl;
            }
        }


//        Eigen::MatrixXd degreeblade_mid(2 * blade_num, nprobe);
//        Eigen::Map<Eigen::MatrixXd> degreeblade_map(degreeblade2[0].data(), blade_num, nprobe);
//        degreeblade_mid << degreeblade_map, degreeblade_map;
//        Eigen::MatrixXd CrossCorrelation3(blade_num, nprobe - 1);

        //从第二个传感器开始计算与第一个传感器的振动位移相关系数
//        for (int iprobe = 1; iprobe < nprobe; ++iprobe)
//        {
//            for (int iblade = 0; iblade < blade_num; ++iblade)
//            {
//                auto CrossCorrelation2 = PiErXiShu(degreeblade_map.col(0), degreeblade_mid.block(iblade, iprobe, blade_num, 1).col(0));
//                CrossCorrelation3(iblade, iprobe - 1) = CrossCorrelation2(0, 1);
//            }
//        }

//        std::vector<int> leadx(nprobe - 1);//每个传感器中与第一个传感器第一个叶片对齐的索引，从0开始
//        for (int iprobe = 0; iprobe < nprobe - 1; ++iprobe) {
//            leadx[iprobe] = std::distance(CrossCorrelation3.col(iprobe).data(),
//                                          std::max_element(CrossCorrelation3.col(iprobe).data(),
//                                                           CrossCorrelation3.col(iprobe).data() + blade_num));
//        }
//        qDebug()<<leadx;


//        // 循环移位，确保叶片对齐
//        for (int iprobe = 1; iprobe < nprobe; ++iprobe)
//        {
//            int shift = leadx[iprobe - 1];
//            std::vector<double> temp(blade_num);
//            for (int i = 0; i < blade_num; ++i) {
//                temp[i] = degreeblade2[i][iprobe];
//            }
//            std::rotate(temp.begin(), temp.begin() + shift, temp.end());
//            for (int i = 0; i < blade_num; ++i) {
//                degreeblade2[i][iprobe] = temp[i];
//            }
//        }

        QVector<QVector<double>> degree1to2_tr(nrl, QVector<double>(nprobe - 1));//存放第二、第三传感器的传感器夹角，后面求均值
        QVector<double> degree1to2_set(nprobe - 1);//存放最终的角度夹角
        std::vector<int> leadx(nprobe - 1);
        leadx={0,4};
        if (nprobe > 1)
        {
            for (int i = 1; i <=nrl; ++i)
            {
                int begin1 = i * blade_num;
                int num2 = begin1 + m - 1;
                for (int iprobe = 0; iprobe < nprobe - 1; ++iprobe)
                {
                    // 使用对齐的叶片索引进行计算
                    int alignedIndex = leadx[iprobe];
                    double timeDiff = (trimmedData[iprobe + 1][num2] - trimmedData[0][num2]) / static_cast<double>(sampling);
                    degree1to2_tr[i-1][iprobe] = (2 * M_PI * speed3[iprobe + 1][0][i] / 60.0 * Radius *timeDiff
                                                  - degreeblade2[m-1+alignedIndex][iprobe + 1] + degreeblade2[m-1][0]) / Radius;
                }
            }

            for (int iprobe = 0; iprobe < nprobe - 1; ++iprobe)
            {
                double sum = 0.0;
                for (int i = 0; i <nrl; ++i)
                {
                    sum += degree1to2_tr[i][iprobe];
                }
                degree1to2_set[iprobe] = sum / nrl ;
            }

            sensor1Angle = 0.0; // 第一个传感器角度固定为0°
            sensor2Angle = degree1to2_set[0] * 180 / M_PI; // 第二个传感器角度
            sensor3Angle = degree1to2_set[1] * 180 / M_PI; // 第三个传感器角度

            // 更新文本框
            angleEdit1->setText("0°");
            angleEdit2->setText(QString::number(sensor2Angle, 'f', 4) + "°");
            angleEdit3->setText(QString::number(sensor3Angle, 'f', 4) + "°");
        }
    }
}

//LM中导入数据的槽函数，可以把3个传感器的频率值和对应的振动值合成一个大数组
void parameter_identification::initializeData()
{
   if(sensor1Speed.empty()||sensor2Speed.empty()||sensor3Speed.empty()||bladeCount<=0)
   {
       QMessageBox::warning(this, tr("数据错误"), tr("无有效数据"));
       return;
   }
   b->clear();
   c->clear();

   // 添加传感器选择
   b->addItem("传感器1");
   b->addItem("传感器2");
   b->addItem("传感器3");

   // 添加叶片号选择
   for (int i = 1; i <= bladeCount; ++i)
   {
       c->addItem(tr("叶片%1").arg(i));
   }

   // 默认选择第一个传感器和第一片叶片
   b->setCurrentIndex(0);
   c->setCurrentIndex(0);

   // 绘制初始图像，只绘制第一个传感器的第一个叶片
   a->clearGraphs();

   a->addGraph();
   a->graph(0)->setData(sensor1RollCycle[0],sensor1_Original_Bias[0]);
   a->yAxis->setRange(sensor1VibMin[0]-100,sensor1VibMax[0]+100);
   a->xAxis->setRange(0,sensor1RollCycle[0].size());
   a->replot();

   int maxDataCount = std::min({sensor1Speed[0].size(), sensor2Speed[0].size(), sensor3Speed[0].size()});
   d->setValidator(new QIntValidator(1, maxDataCount, this));
   e->setValidator(new QIntValidator(1, maxDataCount, this));

   Fre_Xdata={sensor1Speed,sensor2Speed,sensor3Speed};
   CycleNum_Xdata={sensor1RollCycle,sensor2RollCycle,sensor3RollCycle};
   NoBiasVib_Ydata={sensor1_Original_Bias,sensor2_Original_Bias,sensor3_Original_Bias};

}

//LM中设置共振参数的按钮的槽函数，显示固定范围内的振动
void parameter_identification::SetVibRang()
{
   if(Fre_Xdata.empty()||NoBiasVib_Ydata.empty())
   {
       QMessageBox::warning(this, tr("数据错误"), tr("请先导入数据"));
       return;
   }
  // 获取当前选择的传感器和叶片号
  int sensorIndex = b->currentIndex();
  int bladeIndex = c->currentIndex();

  // 获取起始圈数和终止圈数
  bool ok1, ok2;//用来查看文本转整数是否成功
  int startCycle = d->text().toInt(&ok1);
  int endCycle = e->text().toInt(&ok2);

  if (!ok1 || !ok2 || startCycle >= endCycle)
  {
      QMessageBox::warning(this, "输入错误", "起始圈数必须小于终止圈数");
      return;
  }

  // 获取对应的 CycleNum_Xdata 和 NoBiasVib_Ydata
  QVector<double> xData = CycleNum_Xdata[sensorIndex][bladeIndex].mid(startCycle, endCycle - startCycle);
  QVector<double> yData = NoBiasVib_Ydata[sensorIndex][bladeIndex].mid(startCycle, endCycle - startCycle);

  // 清除并绘制图像
  a->clearGraphs();
  a->addGraph();
  a->graph(0)->setData(xData, yData);
  a->yAxis->setRange(sensor1VibMin[bladeIndex]-100,sensor1VibMax[bladeIndex]+100);
  a->xAxis->setRange(startCycle,endCycle);
  a->replot();
}

//LM中清空图像的槽函数
void parameter_identification::LMPlotClear()
{
   a->clearGraphs();
   a->replot();
}

//LM中复位函数，点击后显示全部振动位移
void parameter_identification::ReSetVibPlot()
{
    if(Fre_Xdata.empty()||NoBiasVib_Ydata.empty())
    {
        QMessageBox::warning(this, tr("数据错误"), tr("请先导入数据"));
        return;
    }
   // 获取当前选择的传感器和叶片号
   int sensorIndex = b->currentIndex();
   int bladeIndex = c->currentIndex();

   // 获取对应的 CycleNum_Xdata 和 NoBiasVib_Ydata
   QVector<double> xData = CycleNum_Xdata[sensorIndex][bladeIndex];
   QVector<double> yData = NoBiasVib_Ydata[sensorIndex][bladeIndex];
   // 清除并绘制图像
   a->clearGraphs();
   a->addGraph();
   a->graph(0)->setData(xData, yData);
   a->yAxis->setRange(sensor1VibMin[bladeIndex]-100,sensor1VibMax[bladeIndex]+100);
   a->xAxis->setRange(0,CycleNum_Xdata[sensorIndex][bladeIndex].size());
   a->replot();
}

//LM拟合函数，点击后开始lM拟合，获取未知参数
void parameter_identification::on_LMfitButton_clicked()
{
   if(Fre_Xdata.empty()||NoBiasVib_Ydata.empty())
   {
      QMessageBox::warning(this, tr("数据错误"), tr("请先导入数据"));
      return;
   }
   int sensorIndex = b->currentIndex(); // 获取传感器编号
   int bladeIndex = c->currentIndex();  // 获取叶片编号
   int downlimit = d->text().toInt();   // 获取起始圈数
   int uplimit = e->text().toInt();     // 获取终止圈数

   if(downlimit>=uplimit)
   {
      QMessageBox::warning(this, tr("数据错误"), tr("起始圈数需要小于终止圈数"));
      return;
   }
   QVector<double> speed;
   QVector<double> bias;

   qDebug()<<"起始圈数"<<downlimit<<"  终止圈数"<<uplimit<<"  传感器"<<sensorIndex<<"  叶片号"<<bladeIndex;

   speed=Fre_Xdata[sensorIndex][bladeIndex];
   bias=NoBiasVib_Ydata[sensorIndex][bladeIndex];
   // 使用std::thread来启动LMfit.fitProbe
   std::thread t(&parameter_identification::performLMfit, this, downlimit, uplimit, speed, bias, 500);
   t.detach(); // 分离线程

}

//清空表格的内容，相当于重置
void parameter_identification::clearTableButton()
{
    // 清空表格内容，但保留表头
    tableWidget->setRowCount(0);
    tableWidget->setRowCount(3); // 预置3行空行
}

//LM中放入线程中执行的函数，这里面有个invokeMethod，可以研究一下
void parameter_identification::performLMfit(int downlimit, int uplimit, QVector<double> speed, QVector<double> bias, int Q_value)
{
   auto result = LMfit.fitProbe(downlimit, uplimit, speed, bias, Q_value);

   auto est_params = std::get<0>(result);
   auto x = std::get<1>(result);
   auto y = std::get<2>(result);
   auto y_fit = std::get<3>(result);

   // 使用返回的参数进行进一步处理或绘图
   QMetaObject::invokeMethod(this, [this, x = std::move(x), y = std::move(y),
                             y_fit = std::move(y_fit),est_params]()mutable
   {
       a->clearGraphs();
       a->addGraph();
       a->graph(0)->setData(x, y);
       a->addGraph();
       a->graph(1)->setData(x, y_fit);

       a->yAxis->setRange(*std::min_element(y.begin(), y.end()) - 100, *std::max_element(y.begin(), y.end()) + 100);
       a->graph(0)->setName("原始数据");
       a->graph(1)->setName("拟合数据");
       a->graph(1)->setPen(QPen(Qt::red));
       a->legend->setVisible(true);

       a->rescaleAxes(true); // 确保只调整 x 轴
       a->replot();
       qDebug() << "拟合完成并绘制图像";

       // 获取当前选择的传感器和叶片号
       int sensorIndex = b->currentIndex();
       int bladeIndex = c->currentIndex();
       // 设置表格数据
       double angle = 0.0;
       if (sensorIndex == 0) angle = sensor1Angle;
       else if (sensorIndex == 1) angle = sensor2Angle;
       else if (sensorIndex == 2) angle = sensor3Angle;

       double A0 = est_params(0, 0);
       double Fn = est_params(1, 0);
       double Q = est_params(2, 0);
       double Phase = est_params(3, 0);
       double Db = est_params(4, 0);
       double A = A0 * Q;

       // 查找第一个空行
       int newRow = -1;
       for (int i = 0; i < tableWidget->rowCount(); ++i)
       {
           if (tableWidget->item(i, 0) == nullptr)
           {
               newRow = i;
               break;
           }
       }

       // 如果没有空行，则添加新行
       if (newRow == -1)
       {
           newRow = tableWidget->rowCount();
           tableWidget->insertRow(newRow);
       }
       tableWidget->setItem(newRow, 0, new QTableWidgetItem(QString::number(sensorIndex + 1)));
       tableWidget->setItem(newRow, 1, new QTableWidgetItem(QString::number(angle)));
       tableWidget->setItem(newRow, 2, new QTableWidgetItem(QString::number(bladeIndex + 1)));
       tableWidget->setItem(newRow, 3, new QTableWidgetItem(QString::number(A)));
       tableWidget->setItem(newRow, 4, new QTableWidgetItem(QString::number(A0)));
       tableWidget->setItem(newRow, 5, new QTableWidgetItem(QString::number(Fn)));
       tableWidget->setItem(newRow, 6, new QTableWidgetItem(QString::number(Q)));
       tableWidget->setItem(newRow, 7, new QTableWidgetItem(QString::number(Phase)));
       tableWidget->setItem(newRow, 8, new QTableWidgetItem(QString::number(Db)));
   }, Qt::QueuedConnection);
}


//单叶片的绘图
void parameter_identification::plotSingleBlade(int sensorIndex, int bladeIndex)
{
   QVector<QVector<double>>* vibrationData = nullptr;
   QVector<QVector<double>>* removeBiasData = nullptr;
   QVector<QVector<double>>* speedData = nullptr;
   QVector<QVector<double>>* rollCycleData = nullptr;
   QVector<QVector<double>>* OrigBiasData = nullptr;

   qDebug()<<"索引值为"<<sensorIndex<<"   叶片号是"<<bladeIndex;
   switch (sensorIndex)
    {
       case 0:
           vibrationData = &sensor1Vibration;
           removeBiasData = &sensor1RemoveBias;
           speedData = &sensor1Speed;
           rollCycleData = &sensor1RollCycle;
           OrigBiasData =&sensor1_Original_Bias;
           break;
       case 1:
           vibrationData = &sensor2Vibration;
           removeBiasData = &sensor2RemoveBias;
           speedData = &sensor2Speed;
           rollCycleData = &sensor2RollCycle;
           OrigBiasData =&sensor2_Original_Bias;
           break;
       case 2:
           vibrationData = &sensor3Vibration;
           removeBiasData = &sensor3RemoveBias;
           speedData = &sensor3Speed;
           rollCycleData = &sensor3RollCycle;
           OrigBiasData =&sensor3_Original_Bias;
           break;
       default:
           return;
    }

    if (!vibrationData || vibrationData->isEmpty() || bladeIndex >= vibrationData->size())
    {
       QMessageBox::warning(this, tr("绘图错误"), tr("无效的叶片数据"));
       return;
    }

    if (bladeIndex >= (*vibrationData).size() || bladeIndex >= (*removeBiasData).size() ||
        bladeIndex >= (*speedData).size() || bladeIndex >= (*rollCycleData).size() ||
        bladeIndex >= (*OrigBiasData).size())
     {
       QMessageBox::warning(this, tr("绘图错误"), tr("无效的叶片索引"));
       return;
     }

    // 移除每个叶片的第一个数据
    if ((*vibrationData)[bladeIndex].size() > 1)
    {
        (*vibrationData)[bladeIndex].removeFirst();
        (*removeBiasData)[bladeIndex].removeFirst();
        (*speedData)[bladeIndex].removeFirst();
        (*rollCycleData)[bladeIndex].removeFirst();
        (*OrigBiasData)[bladeIndex].removeFirst();
    }
    else
    {
       QMessageBox::warning(this, tr("绘图错误"), tr("叶片数据点不足"));
       return;
    }

    if(sensorIndex==0)
    {
      double vibmax =  sensor1VibMax[bladeIndex] - 100;
      double vibmin =  sensor1VibMin[bladeIndex] + 100;
      NoBaisPlot->yAxis->setRange(vibmin, vibmax);
    }
    if(sensorIndex==1)
    {
      double vibmax =  sensor2VibMax[bladeIndex] - 100;
      double vibmin =  sensor2VibMin[bladeIndex] + 100;
      NoBaisPlot->yAxis->setRange(vibmin, vibmax);
    }
    if(sensorIndex==2)
    {
      double vibmax =  sensor3VibMax[bladeIndex] - 100;
      double vibmin =  sensor3VibMin[bladeIndex] + 100;
      NoBaisPlot->yAxis->setRange(vibmin, vibmax);
    }

    QPen speedPen;
    speedPen.setColor(Qt::red); // 设置转速曲线的颜色

   // 绘制原始振动和圈数的图像（左y轴）和频率与圈数的图像（右y轴）
    HaveBaisPlot->clearGraphs();
    HaveBaisPlot->addGraph(HaveBaisPlot->xAxis, HaveBaisPlot->yAxis);

    HaveBaisPlot->graph(0)->setData((*rollCycleData)[bladeIndex], (*vibrationData)[bladeIndex]);

    HaveBaisPlot->addGraph(HaveBaisPlot->xAxis, HaveBaisPlot->yAxis2);
    HaveBaisPlot->graph(1)->setPen(speedPen);
    HaveBaisPlot->graph(1)->setData((*rollCycleData)[bladeIndex], (*speedData)[bladeIndex]);

    HaveBaisPlot->rescaleAxes();
    HaveBaisPlot->replot();


    // 绘制去恒偏的振动
    NoBaisPlot->clearGraphs();
    NoBaisPlot->addGraph();
    NoBaisPlot->graph(0)->setData((*rollCycleData)[bladeIndex], (*OrigBiasData)[bladeIndex]);

    NoBaisPlot->addGraph(NoBaisPlot->xAxis, NoBaisPlot->yAxis2);
    NoBaisPlot->graph(1)->setData((*rollCycleData)[bladeIndex], (*speedData)[bladeIndex]);
    NoBaisPlot->graph(1)->setPen(speedPen);


    // Set y-axis range to ±200 um
    NoBaisPlot->yAxis->setRange(-200, 200);
    NoBaisPlot->rescaleAxes();
    NoBaisPlot->replot();
}

//全叶片的绘图
void parameter_identification::plotAllBlades(int sensorIndex)
{
    const QVector<QVector<double>>* vibrationData = nullptr;
    const QVector<QVector<double>>* removeBiasData = nullptr;
    const QVector<QVector<double>>* speedData = nullptr;
    const QVector<QVector<double>>* rollCycleData = nullptr;

    switch (sensorIndex)
    {
        case 0:
            vibrationData = &sensor1Vibration;
            removeBiasData = &sensor1RemoveBias;
            speedData = &sensor1Speed;
            rollCycleData = &sensor1RollCycle;
            break;
        case 1:
            vibrationData = &sensor2Vibration;
            removeBiasData = &sensor2RemoveBias;
            speedData = &sensor2Speed;
            rollCycleData = &sensor2RollCycle;
            break;
        case 2:
            vibrationData = &sensor3Vibration;
            removeBiasData = &sensor3RemoveBias;
            speedData = &sensor3Speed;
            rollCycleData = &sensor3RollCycle;
            break;
        default:
            return;
    }

    if (!vibrationData || vibrationData->isEmpty())
    {
        QMessageBox::warning(this, tr("绘图错误"), tr("无效的叶片数据"));
        return;
    }

    // 绘制所有叶片的原始振动和去恒偏振动
    HaveBaisPlot->clearGraphs();
    NoBaisPlot->clearGraphs();

    int colorStep = 360 / (vibrationData->size() + 1); // 计算颜色步进值，确保相邻颜色差异较大

    for (int i = 0; i < vibrationData->size(); ++i)
    {
        QPen pen;
        pen.setColor(QColor::fromHsv((i * colorStep) % 360, 255, 255)); // 设置颜色，使用颜色步进值

        HaveBaisPlot->addGraph(HaveBaisPlot->xAxis, HaveBaisPlot->yAxis);
        HaveBaisPlot->graph(i)->setData((*rollCycleData)[i], (*vibrationData)[i]);
        HaveBaisPlot->graph(i)->setPen(pen); // 应用颜色

        NoBaisPlot->addGraph();
        NoBaisPlot->graph(i)->setData((*rollCycleData)[i], (*removeBiasData)[i]);
        NoBaisPlot->graph(i)->setPen(pen); // 应用颜色
    }

    // 绘制第一个叶片的转速-圈数曲线,绘制一个叶片的就行了
    int bladeIndex = 0; // 选择第一个叶片
    HaveBaisPlot->addGraph(HaveBaisPlot->xAxis, HaveBaisPlot->yAxis2);
    NoBaisPlot->addGraph(NoBaisPlot->xAxis, NoBaisPlot->yAxis2);
    HaveBaisPlot->graph(vibrationData->size())->setData((*rollCycleData)[bladeIndex], (*speedData)[bladeIndex]);
    NoBaisPlot->graph(vibrationData->size())->setData((*rollCycleData)[bladeIndex], (*speedData)[bladeIndex]);

    QPen speedPen;
    speedPen.setColor(Qt::red); // 设置转速曲线的颜色
    HaveBaisPlot->graph(vibrationData->size())->setPen(speedPen);
    NoBaisPlot->graph(vibrationData->size())->setPen(speedPen);


    HaveBaisPlot->rescaleAxes();
    HaveBaisPlot->replot();

    NoBaisPlot->rescaleAxes();
    NoBaisPlot->replot();
}

//第一个窗口的函数
void parameter_identification::FirstWidgetInitial(QTabWidget *tabWidget)
{
    // Create the first tab widget and set the layout
    QWidget *firstTab = new QWidget;
    tabWidget->addTab(firstTab, tr("参数设置与振动位移"));

    QHBoxLayout *mainLayout = new QHBoxLayout(firstTab);

    // Create a layout for the plots (80% of the main layout)
    QVBoxLayout *plotLayout = new QVBoxLayout;
    plotLayout->addWidget(HaveBaisPlot);
    plotLayout->addWidget(NoBaisPlot);

    // Set up the first custom plot (HaveBaisPlot) with dual Y axes
    HaveBaisPlot->plotLayout()->insertRow(0);
    QCPTextElement *title1 = new QCPTextElement(HaveBaisPlot, "原始振动绘制", QFont("sans", 12, QFont::Bold));
    HaveBaisPlot->plotLayout()->addElement(0, 0, title1);
    HaveBaisPlot->addGraph(HaveBaisPlot->xAxis, HaveBaisPlot->yAxis);
    HaveBaisPlot->addGraph(HaveBaisPlot->xAxis, HaveBaisPlot->yAxis2);
    HaveBaisPlot->xAxis->setLabel(tr("圈数"));
    HaveBaisPlot->yAxis->setLabel(tr("原始振动幅值（um）"));
    HaveBaisPlot->yAxis2->setVisible(true);
    HaveBaisPlot->yAxis2->setLabel(tr("转速"));
    HaveBaisPlot->setInteractions(QCP::iRangeZoom | QCP::iRangeDrag);

    // Set up the second custom plot (NoBaisPlot) with dual Y axes
    NoBaisPlot->plotLayout()->insertRow(0);
    QCPTextElement *title2 = new QCPTextElement(NoBaisPlot, "去除恒偏振动绘制", QFont("sans", 12, QFont::Bold));
    NoBaisPlot->plotLayout()->addElement(0, 0, title2);
    NoBaisPlot->addGraph(NoBaisPlot->xAxis, NoBaisPlot->yAxis);
    NoBaisPlot->xAxis->setLabel(tr("圈数"));
    NoBaisPlot->yAxis->setLabel(tr("去恒偏振动幅值（um）"));
    NoBaisPlot->addGraph(NoBaisPlot->xAxis, NoBaisPlot->yAxis2);
    NoBaisPlot->yAxis2->setVisible(true);
    NoBaisPlot->yAxis2->setLabel(tr("转速"));
    NoBaisPlot->setInteractions(QCP::iRangeZoom | QCP::iRangeDrag);
    // Add plot layout to the main layout (80% of the width)
    mainLayout->addLayout(plotLayout, 4);

    // Create a widget A and add it to the main layout
    QWidget *widgetA = new QWidget;
    QVBoxLayout *layoutA = new QVBoxLayout(widgetA);

    // Create parameter setting B
    QGroupBox *groupBoxB = new QGroupBox(tr("参数设置"));
    QVBoxLayout *layoutB = new QVBoxLayout;

    QLabel *samplingRateLabel = new QLabel(tr("采样率:"));
    samplingRateEdit = new QLineEdit;

    QLabel *bladeCountLabel = new QLabel(tr("叶片数:"));
    bladeCountEdit = new QLineEdit;

    QLabel *radiusLabel = new QLabel(tr("半径(um):"));
    radiusEdit = new QLineEdit;

    QHBoxLayout *sensor1Layout = new QHBoxLayout;
    QLabel *sensor1Label = new QLabel(tr("传感器1:"));
    sensor1PathEdit = new QLineEdit;
    sensor1PathEdit->setReadOnly(true);
    QPushButton *sensor1Button = new QPushButton(tr("选择bin文件"));
    sensor1Layout->addWidget(sensor1Label);
    sensor1Layout->addWidget(sensor1PathEdit);
    sensor1Layout->addWidget(sensor1Button);

    QHBoxLayout *sensor2Layout = new QHBoxLayout;
    QLabel *sensor2Label = new QLabel(tr("传感器2:"));
    sensor2PathEdit = new QLineEdit;
    sensor2PathEdit->setReadOnly(true);
    QPushButton *sensor2Button = new QPushButton(tr("选择bin文件"));
    sensor2Layout->addWidget(sensor2Label);
    sensor2Layout->addWidget(sensor2PathEdit);
    sensor2Layout->addWidget(sensor2Button);

    QHBoxLayout *sensor3Layout = new QHBoxLayout;
    QLabel *sensor3Label = new QLabel(tr("传感器3:"));
    sensor3PathEdit = new QLineEdit;
    sensor3PathEdit->setReadOnly(true);
    QPushButton *sensor3Button = new QPushButton(tr("选择bin文件"));
    sensor3Layout->addWidget(sensor3Label);
    sensor3Layout->addWidget(sensor3PathEdit);
    sensor3Layout->addWidget(sensor3Button);

    QPushButton *parameterSetButton = new QPushButton(tr("参数设置/导入数据"));

    // Angle display for three sensors
    QGridLayout *angleLayout = new QGridLayout;
    angleLayout->addWidget(new QLabel(tr("传感器1角度 °")), 0, 0);
    angleLayout->addWidget(new QLabel(tr("传感器2角度 °")), 0, 1);
    angleLayout->addWidget(new QLabel(tr("传感器3角度 °")), 0, 2);
    angleEdit1 = new QLineEdit;
    angleEdit2 = new QLineEdit;
    angleEdit3 = new QLineEdit;
    angleLayout->addWidget(angleEdit1, 1, 0);
    angleLayout->addWidget(angleEdit2, 1, 1);
    angleLayout->addWidget(angleEdit3, 1, 2);

    QPushButton *GetAngleButton = new QPushButton(tr("获取传感器安装角度"));

    // Adding widgets to layoutB
    layoutB->addWidget(samplingRateLabel);
    layoutB->addWidget(samplingRateEdit);
    layoutB->addWidget(bladeCountLabel);
    layoutB->addWidget(bladeCountEdit);
    layoutB->addWidget(radiusLabel);
    layoutB->addWidget(radiusEdit);
    layoutB->addLayout(sensor1Layout);
    layoutB->addLayout(sensor2Layout);
    layoutB->addLayout(sensor3Layout);
    layoutB->addWidget(parameterSetButton);
    layoutB->addLayout(angleLayout);
    layoutB->addWidget(GetAngleButton);

    groupBoxB->setLayout(layoutB);

    // Create view options C
    QGroupBox *groupBoxC = new QGroupBox(tr("查看选项"));
    QVBoxLayout *layoutC = new QVBoxLayout;

    QHBoxLayout *sensorSelectionLayout = new QHBoxLayout;
    QLabel *sensorComboBoxLabel = new QLabel(tr("选择传感器:"));
    sensorComboBox = new QComboBox;
    sensorComboBox->addItem(tr("传感器1"));
    sensorComboBox->addItem(tr("传感器2"));
    sensorComboBox->addItem(tr("传感器3"));
    sensorSelectionLayout->addWidget(sensorComboBoxLabel);
    sensorSelectionLayout->addWidget(sensorComboBox);

    QHBoxLayout *bladeSelectionLayout = new QHBoxLayout;
    QLabel *bladeComboBoxLabel = new QLabel(tr("选择叶片:"));
    bladeComboBox = new QComboBox;
    for (int i = 1; i <= 1; ++i)//简单初始化
    {  //往复选框中添加叶片号
       bladeComboBox->addItem(tr("叶片%1").arg(i));
    }
    bladeSelectionLayout->addWidget(bladeComboBoxLabel);
    bladeSelectionLayout->addWidget(bladeComboBox);

    QHBoxLayout *bladeOptionLayout = new QHBoxLayout;
    singleBladeRadioButton = new QRadioButton(tr("单叶片"));
    allBladesRadioButton = new QRadioButton(tr("全叶片"));
    QButtonGroup *bladeGroup = new QButtonGroup;
    bladeGroup->addButton(singleBladeRadioButton);
    bladeGroup->addButton(allBladesRadioButton);
    bladeOptionLayout->addWidget(singleBladeRadioButton);
    bladeOptionLayout->addWidget(allBladesRadioButton);

    QPushButton *CaculateButton=new QPushButton(tr("计算振动位移"));
    QPushButton *plotButton = new QPushButton(tr("绘图"));
    QPushButton *clearPlotButton = new QPushButton(tr("清空绘图"));
    QPushButton *DaoShuJuButton = new QPushButton(tr("导出转速与位移数据"));

    // Adding widgets to layoutC
    layoutC->addLayout(sensorSelectionLayout);
    layoutC->addLayout(bladeSelectionLayout);
    layoutC->addLayout(bladeOptionLayout);
    layoutC->addWidget(CaculateButton);
    layoutC->addWidget(plotButton);
    layoutC->addWidget(clearPlotButton);
    layoutC->addWidget(DaoShuJuButton);

    groupBoxC->setLayout(layoutC);

    // Adding groups B and C to layoutA (20% of the width)
    layoutA->addWidget(groupBoxB);
    layoutA->addWidget(groupBoxC);

    // Add widgetA to the main layout
    mainLayout->addWidget(widgetA, 1);

    firstTab->setLayout(mainLayout);

//------------------------------------信号与槽的连接--------------------------------------

    connect(sensor1Button, &QPushButton::clicked, this, &parameter_identification::selectSensor1File);
    connect(sensor2Button, &QPushButton::clicked, this, &parameter_identification::selectSensor2File);
    connect(sensor3Button, &QPushButton::clicked, this, &parameter_identification::selectSensor3File);
    connect(parameterSetButton, &QPushButton::clicked, this, &parameter_identification::setParameters);
    connect(CaculateButton, &QPushButton::clicked, this, &parameter_identification::calculateVibrationDisplacement);
    connect(plotButton, &QPushButton::clicked, this, &parameter_identification::BladeVidDataDraw);
    connect(clearPlotButton, &QPushButton::clicked, this, &parameter_identification::clearPlots);
    connect(GetAngleButton, &QPushButton::clicked, this, &parameter_identification::CalSensorAngle);
    connect(DaoShuJuButton, &QPushButton::clicked, this, &parameter_identification::exportData);


    connect(this, &parameter_identification::fileReadError, this, [](const QString &errorMessage)
    {
        QMessageBox::warning(nullptr, QObject::tr("文件读取错误"), errorMessage);
    });
    connect(this, &parameter_identification::DataNotEnough, this, [](const QString &message)
    {
        QMessageBox::warning(nullptr, QObject::tr("计算振动失败"), message);
    });
    connect(this, &parameter_identification::CalAngle, this, [](const QString &message)
    {
        QMessageBox::warning(nullptr, QObject::tr("角度计算失败"), message);
    });
    connect(this, &parameter_identification::fileReadCompleted, this, [](const QString &message)
    {
        QMessageBox::information(nullptr, QObject::tr("bin文件读取完成"), message);
    });

    connect(this, &parameter_identification::vibCalCompleted, this, [](const QString &message) {
        QMessageBox::information(nullptr, tr("振动计算完成"), message);
    });
}

//第二窗口
void parameter_identification::SecondWidgetInitial(QTabWidget *tabWidget)
{
    QWidget *secondTab = new QWidget;
    tabWidget->addTab(secondTab, tr("LM拟合"));

//=========================创建A部分=========================================
    a = new QCustomPlot(secondTab);
    a->setInteractions(QCP::iRangeZoom | QCP::iRangeDrag);
    QLabel *bLabel = new QLabel("传感器选择:", secondTab);
    b = new QComboBox(secondTab);
    b->setFixedSize(100, 25);

    QLabel *cLabel = new QLabel("叶片号选择:", secondTab);
    c = new QComboBox(secondTab);
    c->setFixedSize(100, 25);

    QLabel *dLabel = new QLabel("起始圈数:", secondTab);
    d = new QLineEdit(secondTab);
    d->setFixedSize(100, 25);
    d->setText("1");

    QLabel *eLabel = new QLabel("终止圈数:", secondTab);
    e = new QLineEdit(secondTab);
    e->setFixedSize(100, 25);
    e->setText("1");

    QLabel *fLabel = new QLabel("导入所有的振动数据:", secondTab);
    QPushButton *f = new QPushButton("导入数据", secondTab);
    f->setFixedSize(100, 25);

    QLabel *gLabel = new QLabel("设置共振区间的振动数据:", secondTab);
    QPushButton *g = new QPushButton("设置共振范围", secondTab);
    g->setFixedSize(100, 25);

    QPushButton *h = new QPushButton("清空图像", secondTab);
    h->setFixedSize(100, 25);

    QPushButton *o = new QPushButton("重置", secondTab);
    o->setFixedSize(100, 25);

    // 使用QGroupBox组织A部分
    QGroupBox *groupBoxA = new QGroupBox("振动位移拟合", secondTab);
    QVBoxLayout *layoutGroupBoxA = new QVBoxLayout(groupBoxA);
    layoutGroupBoxA->addWidget(a);

    QVBoxLayout *j = new QVBoxLayout();
    j->addWidget(bLabel);
    j->addWidget(b);
    j->addWidget(cLabel);
    j->addWidget(c);

    QVBoxLayout *k = new QVBoxLayout();
    k->addWidget(dLabel);
    k->addWidget(d);
    k->addWidget(eLabel);
    k->addWidget(e);

    QVBoxLayout *l = new QVBoxLayout();
    l->addWidget(fLabel);
    l->addWidget(f);
    l->addWidget(gLabel);
    l->addWidget(g);

    QVBoxLayout *i = new QVBoxLayout();
    i->addWidget(h);
    i->addWidget(o);

    QHBoxLayout *m = new QHBoxLayout();
    m->addLayout(j);
    m->addLayout(k);
    m->addLayout(l);
    m->addLayout(i);

    layoutGroupBoxA->addLayout(m);


//====================B共振参数显示部分===================================
    // 使用QGroupBox组织B部分
    QGroupBox *groupBoxB = new QGroupBox("共振参数显示", secondTab);
    QVBoxLayout *layoutGroupBoxB = new QVBoxLayout(groupBoxB);
    // 在这里添加B部分的控件和布局
    // 表格控件
    tableWidget = new QTableWidget(3, 9, secondTab);
    tableWidget->setHorizontalHeaderLabels(QStringList() << "Probe No." << "Probe Angle" << "Blade No." << "A/µm" << "A0" << "Fn/Hz" << "Q" << "Phase" << "Db");
    tableWidget->horizontalHeader()->setStretchLastSection(false);
    // 设置样式，确保所有行之间有网格线，包括标题行和第一行之间
    tableWidget->setStyleSheet("QTableWidget::item { border: 1px solid #d6d9dc; }"
                               "QTableWidget::item:selected { border: 1px solid #d6d9dc; }"
                               "QHeaderView::section { border: 1px solid #d6d9dc; background-color: #f0f0f0; }");
    // 参数输入区域
    QLabel *a0Label = new QLabel("A0:", secondTab);
    QLineEdit *a0LineEdit = new QLineEdit(secondTab);
    a0LineEdit->setFixedSize(100, 30);;
    a0LineEdit->setText("0.000");

    QLabel *fnLabel = new QLabel("Fn:", secondTab);
    QLineEdit *fnLineEdit = new QLineEdit(secondTab);
    fnLineEdit->setFixedSize(100, 30);
    fnLineEdit->setText("100.000");

    QLabel *qLabel = new QLabel("Q:", secondTab);
    QLineEdit *qLineEdit = new QLineEdit(secondTab);
    qLineEdit->setFixedSize(100, 30);
    qLineEdit->setText("500.000");

    QLabel *phaseLabel = new QLabel("Phase:", secondTab);
    QLineEdit *phaseLineEdit = new QLineEdit(secondTab);
    phaseLineEdit->setFixedSize(100, 30);
    phaseLineEdit->setText("0.000");

    QLabel *dbLabel = new QLabel("Db:", secondTab);
    QLineEdit *dbLineEdit = new QLineEdit(secondTab);
    dbLineEdit->setFixedSize(100, 30);
    dbLineEdit->setText("0.000");

    QLabel *NLabel = new QLabel(secondTab);
    QPushButton *fitOneButton = new QPushButton("LM拟合", secondTab);
    QPushButton *ClearExcelButton = new QPushButton("清空表格", secondTab);
    fitOneButton->setFixedSize(100, 32);
    ClearExcelButton->setFixedSize(100, 32);

    QVBoxLayout *paramLayout1 = new QVBoxLayout();
    paramLayout1->addWidget(a0Label);
    paramLayout1->addWidget(a0LineEdit);
    paramLayout1->addWidget(fnLabel);
    paramLayout1->addWidget(fnLineEdit);

    QVBoxLayout *paramLayout2 = new QVBoxLayout();
    paramLayout2->addWidget(qLabel);
    paramLayout2->addWidget(qLineEdit);
    paramLayout2->addWidget(phaseLabel);
    paramLayout2->addWidget(phaseLineEdit);

    QVBoxLayout *paramLayout3 = new QVBoxLayout();
    paramLayout3->addWidget(dbLabel);
    paramLayout3->addWidget(dbLineEdit);
    paramLayout3->addWidget(NLabel);
    paramLayout3->addWidget(NLabel);
    paramLayout3->addWidget(NLabel);

    QVBoxLayout *paramLayout4 = new QVBoxLayout();
    paramLayout4->addWidget(NLabel);
    paramLayout4->addWidget(fitOneButton);
    paramLayout4->addWidget(NLabel);
    paramLayout4->addWidget(ClearExcelButton);

    QHBoxLayout *paramLayout = new QHBoxLayout();
    paramLayout->addLayout(paramLayout1);
    paramLayout->addLayout(paramLayout2);
    paramLayout->addLayout(paramLayout3);
    paramLayout->addLayout(paramLayout4);

    layoutGroupBoxB->addWidget(tableWidget);
    layoutGroupBoxB->addLayout(paramLayout);


//============================C部分==============================
    // 使用QGroupBox组织C部分
    QGroupBox *groupBoxC = new QGroupBox("倍频拟合显示", secondTab);
    QVBoxLayout *layoutGroupBoxC = new QVBoxLayout(groupBoxC);

    // 创建绘图控件
    QCustomPlot *plotWidget = new QCustomPlot(groupBoxC);

    // 创建拟合范围设置控件
    QGroupBox *fitRangeGroupBox = new QGroupBox("拟合范围设置", groupBoxC);
    QVBoxLayout *fitRangeLayout = new QVBoxLayout(fitRangeGroupBox);

    QLabel *eo1Label = new QLabel("EO1:", fitRangeGroupBox);
    QLineEdit *eo1LineEdit = new QLineEdit(fitRangeGroupBox);
    eo1LineEdit->setText("1");

    QLabel *eo2Label = new QLabel("EO2:", fitRangeGroupBox);
    QLineEdit *eo2LineEdit = new QLineEdit(fitRangeGroupBox);
    eo2LineEdit->setText("60");

    fitRangeLayout->addWidget(eo1Label);
    fitRangeLayout->addWidget(eo1LineEdit);
    fitRangeLayout->addWidget(eo2Label);
    fitRangeLayout->addWidget(eo2LineEdit);

    // 创建变化速度控件
    QGroupBox *speedChangeGroupBox = new QGroupBox("变化速度", groupBoxC);
    QVBoxLayout *speedChangeLayout = new QVBoxLayout(speedChangeGroupBox);

    QLabel *intervalLengthLabel = new QLabel("间隔长度:", speedChangeGroupBox);
    QDoubleSpinBox *intervalLengthSpinBox = new QDoubleSpinBox(speedChangeGroupBox);
    intervalLengthSpinBox->setRange(0.1, 100); // 根据需要调整范围
    intervalLengthSpinBox->setSingleStep(0.1);
    intervalLengthSpinBox->setValue(0.60);

    speedChangeLayout->addWidget(intervalLengthLabel);
    speedChangeLayout->addWidget(intervalLengthSpinBox);

    // 创建计算按钮
    QPushButton *calculateButton = new QPushButton("计算", groupBoxC);

    // 将所有控件垂直排列
    layoutGroupBoxC->addWidget(plotWidget,6);
    layoutGroupBoxC->addWidget(fitRangeGroupBox,2);
    layoutGroupBoxC->addWidget(speedChangeGroupBox,2);
    layoutGroupBoxC->addWidget(calculateButton);


//===================D部分=============================================
    // 使用QGroupBox组织D部分
    QGroupBox *groupBoxD = new QGroupBox("功能待定", secondTab);
    QVBoxLayout *layoutGroupBoxD = new QVBoxLayout(groupBoxD);
    // 在这里添加D部分的控件和布局


//=========2个控件=======================================
    // 创建AB部分
    QWidget *widgetAB = new QWidget(secondTab);
    QVBoxLayout *layoutAB = new QVBoxLayout(widgetAB);
    layoutAB->addWidget(groupBoxA, 6);  // A部分占AB的70%
    layoutAB->addWidget(groupBoxB, 4);  // B部分占AB的30%
    // 创建CD部分
    QWidget *widgetCD = new QWidget(secondTab);
    QVBoxLayout *layoutCD = new QVBoxLayout(widgetCD);
    layoutCD->addWidget(groupBoxC, 7);  // C部分占CD的70%
    layoutCD->addWidget(groupBoxD, 3);  // D部分占CD的30%

    // 主布局
    QHBoxLayout *mainLayout = new QHBoxLayout(secondTab);
    mainLayout->addWidget(widgetAB, 70);  // AB部分占60%
    mainLayout->addWidget(widgetCD, 30);  // CD部分占40%

    secondTab->setLayout(mainLayout);


//-------信号与槽的连接--------------------------------------
    connect(fitOneButton, &QPushButton::clicked, this, &parameter_identification::on_LMfitButton_clicked);
    connect(f, &QPushButton::clicked, this, &parameter_identification::initializeData);
    connect(g, &QPushButton::clicked, this, &parameter_identification::SetVibRang);
    connect(h, &QPushButton::clicked, this, &parameter_identification::LMPlotClear);
    connect(o, &QPushButton::clicked, this, &parameter_identification::ReSetVibPlot);
    connect(ClearExcelButton, &QPushButton::clicked, this, &parameter_identification::clearTableButton);
}

//第三窗口
void parameter_identification::ThirdWidgetInitial(QTabWidget *tabWidget)
{
    QWidget *ThirdTab = new QWidget;
    tabWidget->addTab(ThirdTab, tr("数据降噪"));
    QVBoxLayout *mainLayout = new QVBoxLayout(ThirdTab);

    for (int i = 1; i <= 3; ++i)
    {
        QVBoxLayout *channelLayout = new QVBoxLayout;
        QLabel *channelLabel = new QLabel(tr("通道%1：").arg(i));
        channelLabel->setFixedHeight(30);
        QTextEdit *risePathText = new QTextEdit(this);
        risePathText->setReadOnly(true);
        risePathText->setFixedHeight(30);
        QTextEdit *fallPathText = new QTextEdit(this);
        fallPathText->setReadOnly(true);
        fallPathText->setFixedHeight(30);
        QPushButton *chooseRiseButton = new QPushButton(tr("选择传感器%1上升沿").arg(i), this);
        QPushButton *chooseFallButton = new QPushButton(tr("选择传感器%1下降沿").arg(i), this);
        QPushButton *exportBinButton = new QPushButton(tr("导出传感器%1降噪后的bin文件").arg(i), this);

        QHBoxLayout *channelRiseLayout = new QHBoxLayout;
        QHBoxLayout *channelFallLayout = new QHBoxLayout;
        QHBoxLayout *channelButtonLayout = new QHBoxLayout;

        channelRiseLayout->addWidget(chooseRiseButton);
        channelRiseLayout->addWidget(risePathText);
        channelFallLayout->addWidget(chooseFallButton);
        channelFallLayout->addWidget(fallPathText);
        channelButtonLayout->addWidget(exportBinButton);

        channelLayout->addWidget(channelLabel);
        channelLayout->addLayout(channelRiseLayout);
        channelLayout->addLayout(channelFallLayout);
        channelLayout->addLayout(channelButtonLayout);

        mainLayout->addLayout(channelLayout);

        // 保存指针
        risePathTexts.push_back(risePathText);
        fallPathTexts.push_back(fallPathText);
        chooseRiseButtons.push_back(chooseRiseButton);
        chooseFallButtons.push_back(chooseFallButton);
        exportBinButtons.push_back(exportBinButton);

        // 连接信号和槽
        connect(chooseRiseButton, &QPushButton::clicked, this, [=]() { chooseRiseFile(i); });
        connect(chooseFallButton, &QPushButton::clicked, this, [=]() { chooseFallFile(i); });
        connect(exportBinButton, &QPushButton::clicked, this, [=]() { exportBinFile(i); });
    }

    // 选择导出路径按钮及显示路径的文本框
    QHBoxLayout *outputLayout = new QHBoxLayout;
    QPushButton *chooseOutputButton = new QPushButton(tr("选择导出路径"), this);
    outputPathText = new QTextEdit(this);
    outputPathText->setFixedHeight(30);
    outputLayout->addWidget(chooseOutputButton);
    outputLayout->addWidget(outputPathText);
    mainLayout->addLayout(outputLayout);

    QPushButton *exportExcelButton = new QPushButton(tr("导出三通道的数据为Excel进行倍频识别"), this);

    mainLayout->addWidget(exportExcelButton);

    ThirdTab->setLayout(mainLayout);

    connect(chooseOutputButton, &QPushButton::clicked, this, &parameter_identification::chooseOutputDirectory);
    connect(exportExcelButton, &QPushButton::clicked, this, &parameter_identification::exportToExcel);
}

QString parameter_identification::generateOutputFileName(const QString &inputFilePath, const QString &suffix)
{
    QFileInfo fileInfo(inputFilePath);
    QString baseName = fileInfo.completeBaseName(); // 获取不带后缀的文件名
    return baseName + "_" + suffix;
}

void parameter_identification::chooseRiseFile(int channel)
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择上升沿文件"), "", tr("Binary Files (*.bin)"));
    if (!filePath.isEmpty())
    {
        riseFilePaths[channel - 1] = filePath;
        risePathTexts[channel - 1]->setText(filePath);
    }
}

void parameter_identification::chooseFallFile(int channel)
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择下降沿文件"), "", tr("Binary Files (*.bin)"));
    if (!filePath.isEmpty())
    {
        fallFilePaths[channel - 1] = filePath;
        fallPathTexts[channel - 1]->setText(filePath);
    }
}

void parameter_identification::chooseOutputDirectory()
{
    outputDirectory = QFileDialog::getExistingDirectory(this, tr("选择导出路径"), "");
    if (!outputDirectory.isEmpty())
    {
        outputPathText->setText(outputDirectory);
    }
}

void parameter_identification::exportBinFile(int channel)
{
    if (outputDirectory.isEmpty())
    {
        QMessageBox::warning(this, tr("警告"), tr("请先选择导出路径。"));
        return;
    }

    QString riseFilePath = riseFilePaths[channel - 1];
    QString fallFilePath = fallFilePaths[channel - 1];
    if (riseFilePath.isEmpty() || fallFilePath.isEmpty())
    {
        QMessageBox::warning(this, tr("警告"), tr("请先选择上升沿和下降沿文件。"));
        return;
    }

    QString outputRiseFileName = generateOutputFileName(riseFilePath, "_DeleteNoiseVersion.bin");
    QString outputFallFileName = generateOutputFileName(fallFilePath, "_DeleteNoiseVersion.bin");

    qDebug() << "Starting export thread for channel " << channel;
    qDebug() << "Rise file path: " << riseFilePath;
    qDebug() << "Fall file path: " << fallFilePath;
    qDebug() << "Output rise file name: " << outputRiseFileName;
    qDebug() << "Output fall file name: " << outputFallFileName;

    // 使用 lambda 函数来创建新线程
    std::thread exportThread([this, riseFilePath, fallFilePath, outputRiseFileName, outputFallFileName]()
    {
        try
        {
            this->exportBinFileThread(riseFilePath, fallFilePath, outputRiseFileName, outputFallFileName);
        }
        catch (const std::exception& e)
        {
            qDebug() << "Exception in export thread: " << e.what();
            QMessageBox::critical(this, tr("错误"), tr("导出过程中发生错误: ") + QString::fromStdString(e.what()));
        }
    });

    exportThread.detach();
}

void parameter_identification::exportBinFileThread(const QString& riseFilePath, const QString& fallFilePath,
                                                   const QString& outputRiseFileName, const QString& outputFallFileName)
{
    try
    {
        if (riseFilePath.isEmpty() || fallFilePath.isEmpty())
        {
            throw std::runtime_error("Rise or fall file path is empty.");
        }

        // 读取 bin 文件中的上升和下降时间
        std::vector<uint64_t> riseTimes = readBinFile(riseFilePath);
        std::vector<uint64_t> fallTimes = readBinFile(fallFilePath);
        if (riseTimes.empty() || fallTimes.empty())
        {
            emit fileReadError(tr("数组中没有数据"));
            return;
        }

        size_t window_size = 100; // 每组100个数据
        size_t num_segments = riseTimes.size() / window_size;

        std::vector<uint64_t> cleanRiseTimes;
        std::vector<uint64_t> cleanFallTimes;

        DataCalculation* Calculate = new DataCalculation;

        // 第一次滤波
        for (size_t i = 0; i < num_segments; ++i)
        {
            std::vector<uint64_t> riseSegment(riseTimes.begin() + i * window_size, riseTimes.begin() + (i + 1) * window_size);
            std::vector<uint64_t> fallSegment(fallTimes.begin() + i * window_size, fallTimes.begin() + (i + 1) * window_size);

            auto [cleanRiseSegment, cleanFallSegment] = Calculate->DeleteNoiseData(riseSegment, fallSegment);

            cleanRiseTimes.insert(cleanRiseTimes.end(), cleanRiseSegment.begin(), cleanRiseSegment.end());
            cleanFallTimes.insert(cleanFallTimes.end(), cleanFallSegment.begin(), cleanFallSegment.end());
        }

        // 第二次滤波
        std::vector<uint64_t> FinalRiseTimes;
        std::vector<uint64_t> FinalFallTimes;
        Calculate->remove_large_gap_pulses(cleanRiseTimes, FinalRiseTimes, 20, 0.8, 2, 2.5);
        Calculate->remove_large_gap_pulses(cleanFallTimes, FinalFallTimes, 20, 0.8, 2, 2.5);

        // 写入 FinalRiseTimes 到 bin 文件
        std::ofstream outputRiseFile(outputDirectory.toStdString() + "/" + outputRiseFileName.toStdString(), std::ios::binary);
        if (outputRiseFile.is_open())
        {
            outputRiseFile.write(reinterpret_cast<const char*>(FinalRiseTimes.data()), FinalRiseTimes.size() * sizeof(uint64_t));
            outputRiseFile.close();
        }
        else
        {
            throw std::runtime_error("Cannot open rise file for writing.");
        }

        // 写入 FinalFallTimes 到 bin 文件
        std::ofstream outputFallFile(outputDirectory.toStdString() + "/" + outputFallFileName.toStdString(), std::ios::binary);
        if (outputFallFile.is_open())
        {
            outputFallFile.write(reinterpret_cast<const char*>(FinalFallTimes.data()), FinalFallTimes.size() * sizeof(uint64_t));
            outputFallFile.close();
        }
        else
        {
            throw std::runtime_error("Cannot open fall file for writing.");
        }
    }
    catch (const std::exception& e)
    {
        qDebug() << "Exception in exportBinFileThread: " << e.what();
        QMessageBox::critical(nullptr, tr("错误"), tr("导出过程中发生错误: ") + QString::fromStdString(e.what()));
    }
}

void parameter_identification::exportBinFileThread()
{
    if (outputDirectory.isEmpty())
    {
        emit fileReadError(tr("导出路径为空"));
        return;
    }
    try
        {
            std::vector<std::vector<uint64_t>> allFinalRiseTimes(3);
            std::vector<std::vector<uint64_t>> allFinalFallTimes(3);

            for (int channel = 0; channel < 3; ++channel)
            {
                QString riseFilePath = riseFilePaths[channel];
                QString fallFilePath = fallFilePaths[channel];

                if (riseFilePath.isEmpty() || fallFilePath.isEmpty())
                {
                    throw std::runtime_error("Rise or fall file path is empty for channel " + std::to_string(channel + 1));
                }

                // 读取 bin 文件中的上升和下降时间
                std::vector<uint64_t> riseTimes = readBinFile(riseFilePath);
                std::vector<uint64_t> fallTimes = readBinFile(fallFilePath);
                if (riseTimes.empty() || fallTimes.empty())
                {
                    emit fileReadError(tr("数组中没有数据"));
                    return;
                }

                size_t window_size = 100; // 每组100个数据
                size_t num_segments = riseTimes.size() / window_size;

                std::vector<uint64_t> cleanRiseTimes;
                std::vector<uint64_t> cleanFallTimes;

                DataCalculation* Calculate = new DataCalculation;

                // 第一次滤波
                for (size_t i = 0; i < num_segments; ++i)
                {
                    std::vector<uint64_t> riseSegment(riseTimes.begin() + i * window_size, riseTimes.begin() + (i + 1) * window_size);
                    std::vector<uint64_t> fallSegment(fallTimes.begin() + i * window_size, fallTimes.begin() + (i + 1) * window_size);

                    auto [cleanRiseSegment, cleanFallSegment] = Calculate->DeleteNoiseData(riseSegment, fallSegment);

                    cleanRiseTimes.insert(cleanRiseTimes.end(), cleanRiseSegment.begin(), cleanRiseSegment.end());
                    cleanFallTimes.insert(cleanFallTimes.end(), cleanFallSegment.begin(), cleanFallSegment.end());
                }

                // 第二次滤波
                Calculate->remove_large_gap_pulses(cleanRiseTimes, allFinalRiseTimes[channel], 20, 0.8, 2, 2.5);
                Calculate->remove_large_gap_pulses(cleanFallTimes, allFinalFallTimes[channel], 20, 0.8, 2, 2.5);
            }

            emit fileReadError(tr("导出开始，请勿执行任何界面操作"));

            // 创建上升沿 Excel 文件
            QString riseExcelFileName = outputDirectory + "/RiseTimes.xlsx";
            QXlsx::Document riseXlsx;

            size_t maxRiseSize = std::max({ allFinalRiseTimes[0].size(), allFinalRiseTimes[1].size(), allFinalRiseTimes[2].size() });

            for (size_t i = 0; i < maxRiseSize; ++i)
            {
                if (i < allFinalRiseTimes[0].size())
                    riseXlsx.write(i + 1, 1, static_cast<double>(allFinalRiseTimes[0][i]));
                if (i < allFinalRiseTimes[1].size())
                    riseXlsx.write(i + 1, 2, static_cast<double>(allFinalRiseTimes[1][i]));
                if (i < allFinalRiseTimes[2].size())
                    riseXlsx.write(i + 1, 3, static_cast<double>(allFinalRiseTimes[2][i]));
            }

            if (!riseXlsx.saveAs(riseExcelFileName))
            {
                throw std::runtime_error("Failed to save rise times to Excel file.");
            }

            // 创建下降沿 Excel 文件
            QString fallExcelFileName = outputDirectory + "/FallTimes.xlsx";
            QXlsx::Document fallXlsx;

            size_t maxFallSize = std::max({ allFinalFallTimes[0].size(), allFinalFallTimes[1].size(), allFinalFallTimes[2].size() });

            for (size_t i = 0; i < maxFallSize; ++i)
            {
                if (i < allFinalFallTimes[0].size())
                    fallXlsx.write(i + 1, 1, static_cast<double>(allFinalFallTimes[0][i]));
                if (i < allFinalFallTimes[1].size())
                    fallXlsx.write(i + 1, 2, static_cast<double>(allFinalFallTimes[1][i]));
                if (i < allFinalFallTimes[2].size())
                    fallXlsx.write(i + 1, 3, static_cast<double>(allFinalFallTimes[2][i]));
            }

            if (!fallXlsx.saveAs(fallExcelFileName))
            {
                throw std::runtime_error("Failed to save fall times to Excel file.");
            }

             emit fileReadError(tr("导出完成"));
        }
    catch (const std::exception& e)
        {
            qDebug() << "Exception in exportBinFileThread: " << e.what();
            emit fileReadError(tr("导出过程出现错误"));
        }
}

void parameter_identification::exportToExcel()
{

    // 将导出操作放到线程中执行
    std::thread exportThread([this]()
    {
        this->exportBinFileThread();
    });

    exportThread.detach();
}

