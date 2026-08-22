#include "calculation.h"

DataCalculation::DataCalculation(QObject *parent)
                : QObject(parent),
                  num_blade(0),//初始化
                  FirstFitOrder(1),//采用1阶拟合,用于监测
                  SeconedFitOrder(2),//二阶拟合，用于参数识别
                  SampleRate(200000),//采样率初始化为200k
                  Radius(0),//半径初始化为0
                  cycleCounter(16, 0) //16通道全部置于，由于只做了16通道，这里只设置16通道的数据
{
    elapsedTimer.start(); // 启动计时器
}

std::pair<Eigen::MatrixXd, Eigen::MatrixXd> DataCalculation::createMatrix(int num_blade, int order)
{
    //mat_C, mat_B这2个矩阵，在SetParameter这个函数中，由主线程调用一次，所以只会计算一次，不会多次计算
    using namespace Eigen;
    int m = std::floor(num_blade / 2.0);//floor是不大于num_blade / 2.0最大整数
    int framelen = 2 * m + 1;

    MatrixXd mat_X = MatrixXd::Zero(framelen, order + 1);
    mat_X.col(0) = VectorXd::Ones(framelen);
    for (int i = -m; i <= m; ++i) 
    {
        for (int j = 1; j <= order; ++j)
        {
           mat_X(i + m, j) = std::pow(i, j); // Fill columns based on order
        }
    }

    MatrixXd mat_C = (mat_X.transpose() * mat_X).inverse() * mat_X.transpose();//论文中计算转速的J矩阵
    MatrixXd mat_B = mat_X * mat_C;//计算理论时刻的矩阵，这里只用了一阶拟合
    qDebug()<<"创造的mat_B.cols()="<<mat_B.cols();
    //使用 qDebug 打印矩阵
    //qDebug() <<"阶次为"<<order<<"   "<< matrixToQString(mat_X);

    return {mat_C, mat_B};
}

std::tuple<double, Eigen::VectorXd, Eigen::VectorXd> DataCalculation::fit(
                                            double sampling, double radius,
                                            const Eigen::MatrixXd &mat_C,
                                            const Eigen::MatrixXd &mat_B,
                                            const std::vector<uint64_t> &dataConst, int num_blade)
{
      // 创建一个副本以便修改
       std::vector<uint64_t> data = dataConst;
       /*解决奇数个叶片问题，就是2m+1个叶片，m是向下取整，如果叶片是奇数，2m+1后就刚刚好是叶片数，但是传进来的数据是num_blade++1
       这时候移除最后一个叶片，才能保证数据是2m+1*/
       if (data.size() % 2 == 0)
       {
           data.pop_back();
       }

       std::vector<double> temp(data.size());//把data变为double，因为Eigen只接收double类型的
       std::transform(data.begin(), data.end(), temp.begin(),
                      [](uint64_t val) -> double { return static_cast<double>(val); });
       /*Eigen::Map是把数据映射成相邻或者矩阵，而不需要负责数据，需要2个参数，第一个temp.data()是数组的起始地址的指针
         第二个是数据的长度，也就是需要映射多少数据*/
       Eigen::VectorXd dataVec = Eigen::Map<Eigen::VectorXd>(temp.data(), temp.size());//映射为一个列向量
       Eigen::VectorXd dividedData = dataVec / sampling;//得到时间

       // 计算转频，单位是Hz，f=1/(叶片数*a1)
       double fittime = 1 / (num_blade * mat_C.row(1).dot(dividedData));

       // 拟合后的数据作为理论到达时间
       Eigen::VectorXd t1 = mat_B * dividedData;//这里只是简单的计算一下

       // 计算振动
       Eigen::VectorXd vib = (dividedData - t1) * 2 * M_PI * radius * fittime;

       return {fittime, t1, vib};
}


void DataCalculation::ProcessAllChannelsData(const std::vector<std::vector<uint64_t>>& allData)
{
    for (size_t channel = 0; channel < allData.size(); ++channel)
    {
        const auto& newData = allData[channel];//&是引用可以避免拷贝
        auto& accumulator = ChannelDataAccumulator[channel];
        accumulator.insert(accumulator.end(), newData.begin(), newData.end());

        // 确保数据长度是叶片数加1的整数倍,因为2m+1后，要么是叶片数，要么比叶片数多1
        int PartCount=accumulator.size() / (num_blade + 1);
        int processDataLength = (num_blade + 1) * PartCount;

        if (processDataLength > 0)
        {
            std::vector<uint64_t> dataToProcess(accumulator.begin(), accumulator.begin() + processDataLength);
            for(int i=0;i<PartCount;i++)
            {
                //每num_blade + 1个数据处理一次,但是取数据的时候，需要从从叶片数的起始点开始，才能保证虽然每次都取了num_blade + 1个数据
                //但是计算出来的叶片位移才是对应的，因为监测时，计算出的位移是2m+1个叶片的位移。
                std::vector<uint64_t> batchDataToProcess(dataToProcess.begin() + i * num_blade,
                                                         dataToProcess.begin() + (i + 1) * num_blade + 1);
                // 调用fit函数处理数据
                auto [RollSpeed, TheoryTime, vib] = fit(SampleRate, Radius, mat_C, mat_B,batchDataToProcess, num_blade);

                // 将Eigen::VectorXd转换为std::vector以便存储
                std::vector<double> vibVector(vib.data(), vib.data() + vib.size());

                // 振动直接添加到对应的叶片
                for (int leafIndex = 0; leafIndex < num_blade; ++leafIndex) {
                    vibrationData[channel][leafIndex].append(vibVector[leafIndex]);
                }
                cycleData[channel].append(cycleCounter[channel]);
                speedData[channel].append(RollSpeed*60);
                cycleCounter[channel] += 1; // 更新圈数
            }
            // 从累积器中移除已处理的数据
            accumulator.erase(accumulator.begin(), accumulator.begin() + PartCount * num_blade);
        }

    }
    bool hasNewData = false;
    // 如果speedData中也没有新数据，则检查cycleData
    if (!hasNewData)
    {
       for (const auto& channelData : cycleData)
       {
            if (!channelData.isEmpty())
            {
                hasNewData = true;
                break;
            }
        }
    }
    if (hasNewData &&elapsedTimer.elapsed()>= 1000)
     {
        elapsedTimer.restart();
        emit dataReady(vibrationData, speedData, cycleData);//可能会有数组为空的情况，我们在绘图的时候在进行处理
        //发送完后及时清空
        for (auto& channelData : vibrationData)
             {
                 for (auto& leafData : channelData)
                 {
                     leafData.clear();
                 }
             }
             for (auto& channelData : speedData)
             {
                 channelData.clear();
             }
             for (auto& channelData : cycleData)
             {
                 channelData.clear();
             }
    }
}

void DataCalculation::ProcessAllRiseAndFallData(const std::vector<std::vector<uint64_t> > &RiseData,
                                                const std::vector<std::vector<uint64_t> > &FallData)
{
    const size_t groupSize = 100;//每100个数据为一个单元去噪
    const size_t unstableDataThreshold =300; // 前300个数据不稳定，需移除后再降噪
    size_t window_size = 20;//小范围内的窗口，叶片之间的间隔是差别不大的
    double threshold_min = 0.8;//大于窗口均值的0.8倍就认为是争取的脉冲
    double threshold_max = 2;//但是不能大于2倍
    double interval_threshold_factor = 2.5;//窗口内可能有较大的间隔，未来避免极值的影响，选取中位数作为参考，超过中位数2.5倍，就认为是缺项，就得移除避免影响均值的计算

    for (size_t channel = 0; channel < RiseData.size(); ++channel)
    {
        riseDataBuffer[channel].insert(riseDataBuffer[channel].end(), RiseData[channel].begin(), RiseData[channel].end());
        fallDataBuffer[channel].insert(fallDataBuffer[channel].end(), FallData[channel].begin(), FallData[channel].end());

        // 检查并移除前300个不稳定数据
        if (!InstableDataTrace[channel] && riseDataBuffer[channel].size() >= unstableDataThreshold && fallDataBuffer[channel].size() >= unstableDataThreshold)
        {
            qDebug()<<"数据不稳定";
            riseDataBuffer[channel].erase(riseDataBuffer[channel].begin(), riseDataBuffer[channel].begin() + unstableDataThreshold);
            fallDataBuffer[channel].erase(fallDataBuffer[channel].begin(), fallDataBuffer[channel].begin() + unstableDataThreshold);
            InstableDataTrace[channel] = true;
        }

        // 如果没有移除前300个不稳定数据，则跳过去噪处理
        if (!InstableDataTrace[channel])
        {
            continue;
        }

        //利用叶片厚度进行第一次降噪
        while (riseDataBuffer[channel].size() >= groupSize && fallDataBuffer[channel].size() >= groupSize)
        {
           std::vector<uint64_t> riseGroup(riseDataBuffer[channel].begin(), riseDataBuffer[channel].begin() + groupSize);
           std::vector<uint64_t> fallGroup(fallDataBuffer[channel].begin(), fallDataBuffer[channel].begin() + groupSize);

           auto [cleanRiseTimes, cleanFallTimes] = DeleteNoiseData(riseGroup, fallGroup);

           //用下降沿进行计算
           DeleteNoiseFallData[channel].insert(DeleteNoiseFallData[channel].end(), cleanFallTimes.begin(), cleanFallTimes.end());

           //第一次降噪后及时删除原始数据
           riseDataBuffer[channel].erase(riseDataBuffer[channel].begin(), riseDataBuffer[channel].begin() + groupSize);
           fallDataBuffer[channel].erase(fallDataBuffer[channel].begin(), fallDataBuffer[channel].begin() + groupSize);
        }

        //利用叶片间隔第二次降噪，20圈为一次，窄脉冲用0.8-2倍的均值作为判断依据，2.5倍中位数就认为是缺项，就进行补项
        remove_large_gap_pulses(DeleteNoiseFallData[channel],FallData_Finall[channel],window_size,threshold_min,threshold_max,interval_threshold_factor);

        //用下降沿来计算
        int PartCount = FallData_Finall[channel].size() / (num_blade + 1);
        int processDataLength = (num_blade + 1) * PartCount;

        if (processDataLength > 0)
        {
           std::vector<uint64_t> dataToProcess(FallData_Finall[channel].begin(), FallData_Finall[channel].begin() + processDataLength);
           for (int i = 0; i < PartCount; i++)
           {
               std::vector<uint64_t> batchDataToProcess(dataToProcess.begin() + i * num_blade,
                                                        dataToProcess.begin() + (i + 1) * num_blade + 1);

               auto [RollSpeed, TheoryTime, vib] = fit(SampleRate, Radius, mat_C, mat_B, batchDataToProcess, num_blade);

               std::vector<double> vibVector(vib.data(), vib.data() + vib.size());

               for (int leafIndex = 0; leafIndex < num_blade; ++leafIndex)
               {
                   vibrationData[channel][leafIndex].push_back(vibVector[leafIndex]);
               }
               cycleData[channel].push_back(cycleCounter[channel]);
               speedData[channel].push_back(RollSpeed * 60);
               cycleCounter[channel] += 1; // 更新圈数
           }
           //无论用那个沿去计算，计算完成后都要及时清除
           FallData_Finall[channel].erase(FallData_Finall[channel].begin(), FallData_Finall[channel].begin() + PartCount * num_blade);

//           //移除振动位移中的大毛刺
//           filterVibrationData(vibrationData[channel]);
        }
    }

    bool hasNewData = false;
    // 如果speedData中也没有新数据，则检查cycleData
    if (!hasNewData)
    {
       for (const auto& channelData : cycleData)
       {
            if (!channelData.isEmpty())
            {
                hasNewData = true;
                break;
            }
        }
    }
    if (hasNewData &&elapsedTimer.elapsed()>= 1000)
     {
        elapsedTimer.restart();
        emit dataReady(vibrationData, speedData, cycleData);//可能会有数组为空的情况，我们在绘图的时候在进行处理
        //发送完后及时清空
        for (auto& channelData : vibrationData)
             {
                 for (auto& leafData : channelData)
                 {
                     leafData.clear();
                 }
             }
             for (auto& channelData : speedData)
             {
                 channelData.clear();
             }
             for (auto& channelData : cycleData)
             {
                 channelData.clear();
             }
    }
}


//SG滤波准确计算

void DataCalculation::VibParaIdentify(std::vector<uint64_t> data_useful, int blade_num, int sampling, double Radius)
{
    qDebug()<<"我是calculation类中SG滤波准确计算位移的函数";
    int min_length = data_useful.size();

    // 将数据转换为Eigen向量
    Eigen::VectorXd data_vector(min_length);
    for (int i = 0; i < min_length; ++i)
    {
        data_vector(i) = static_cast<double>(data_useful[i]);
    }

    int m = floor(blade_num / 2);
    int framelen = 2 * m + 1;
    int ndata_num = blade_num + 2 * m;

    // 计算第一圈前的第一次滤波数据
    Eigen::VectorXd sg_mid_x =  Eigen::VectorXd::Zero(ndata_num);
    Eigen::VectorXd sg_mid_v =  Eigen::VectorXd::Zero(ndata_num);
    Eigen::VectorXd probe_sgfit =  Eigen::VectorXd::Zero(min_length);

    // 第一圈的前2m+1个数据先处理一下，获取第一圈的理论时间
    if (mat_B.cols() != framelen || data_vector.rows() < framelen)
    {
        qDebug()<<"mat_B.cols()="<<mat_B.cols();
        qDebug()<<"framelen="<<framelen;
        qDebug()<<"calculation中1号点位出问题了";
        return;
    }

    //第一圈的前2m+1个数据先处理一下，获取第一圈的理论时间
    probe_sgfit.head(framelen) = mat_B * (data_vector.head(framelen));

    //最后一圈不要了，就是减一，第一圈不准，最后一圈不要
    QVector<QVector<double>> vibration1(blade_num, QVector<double>(min_length / blade_num - 1));
    QVector<QVector<double>> Speed_Frenquence(blade_num, QVector<double>(min_length / blade_num - 1));
    QVector<QVector<double>> Roll_Cycle_Number(blade_num, QVector<double>(min_length / blade_num - 1));


    for (int iblade = 0; iblade < blade_num; ++iblade)
    {
        if (mat_C.row(1).cols() != framelen)
        {
            qDebug()<<"calculation中2号点位出问题了";
            return;
        }
        double speed = 1.0 / (blade_num * (mat_C.row(1).dot(probe_sgfit.head(framelen) / sampling)));
        double detay = (data_vector(iblade) - probe_sgfit(iblade))/ sampling;
        vibration1[iblade][0] = 2 * M_PI * Radius * detay * speed;
        Speed_Frenquence[iblade][0]=speed;
        Roll_Cycle_Number[iblade][0]=0;
    }


    //这里int((min_length / blade_num) - 1)是为了留下余量，最后一圈就不要了
    for (int irev = 1; irev <int((min_length / blade_num) - 1); ++irev)
    {
        int ibegin = irev * blade_num - 2 * m;
        int iend = (irev + 1) * blade_num + 2 * m;
        Eigen::VectorXd ndataR_1st = data_vector.segment(ibegin, iend - ibegin);
        if (ndataR_1st.size() != (iend - ibegin))
        {
            qDebug()<<"calculation中3号点位出问题了";
            return;
        }
        //第一次滤波，这个就相当于把第二圈叶片放中间，4m+blade_num是这样排列的---2m---blade_num---2m----
        //经过2m+1次滤波后，变成----m---blade_num---m---
        for (int isg = 0; isg < ndata_num; ++isg)
        {
            Eigen::VectorXd ndata = ndataR_1st.segment(isg, 2 * m + 1);
            if (mat_B.row(m).cols() != ndata.rows()||Theory_Time_matrix.row(m).cols() != ndata.rows())
            {
               qDebug()<<"4号点位出问题了";
               return;
            }
            sg_mid_v(isg) = mat_B.row(m) * ndata;// 用于计算转速的
            sg_mid_x(isg) = Theory_Time_matrix.row(m) * ndata;//用于拟合真正到达时间的
        }

        int irevnBsg = irev * blade_num;

        //第二次滤波
        for (int iblade = 0; iblade < blade_num; ++iblade)
        {
            Eigen::VectorXd ndata_v = sg_mid_v.segment(iblade, 2 * m + 1);
            Eigen::VectorXd ndata_x = sg_mid_x.segment(iblade, 2 * m + 1);

            if (mat_C.row(1).cols() != ndata_v.rows()||Theory_Time_matrix.row(m).cols() != ndata_x.rows())
            {
                qDebug()<<"5号点位出问题了";
                return;
            }

            double speed_fre = 1.0 / (blade_num * (mat_C.row(1).dot(ndata_v / sampling))); // 计算速度频率，Hz
            probe_sgfit(irevnBsg + iblade) = Theory_Time_matrix.row(m) * ndata_x;//真正的理论时间

            // 计算振动、添加频率、圈数
            vibration1[iblade][irev] = ((data_vector(irevnBsg + iblade) - probe_sgfit(irevnBsg + iblade)) / sampling) * 2 * M_PI * Radius * speed_fre;
            Speed_Frenquence[iblade][irev]=speed_fre*60;
            Roll_Cycle_Number[iblade][irev]=irev;
        }
    }

    QVector<double> mean_vibration(blade_num); //用平均值当作恒偏量


    for (int iblade = 0; iblade < blade_num; ++iblade)
    {
        // 从第二圈开始算,因为在计算时第一圈的数据不准确，最后一圈的数据没有使用，
        //但是我们没有把最末尾的数据添加到vib里，所以除第一圈外，vib里面全部的数据都是准确的
        double sum = std::accumulate(vibration1[iblade].begin() + 1, vibration1[iblade].end(), 0.0);
        mean_vibration[iblade] = sum / (vibration1[iblade].size() - 1); // 计算平均值，第一圈没有算，所以-1
    }

    //去除恒偏量后的个叶片的振动位移再加上q等间距拉开
    QVector<QVector<double>> Remove_Constant_Bias_vib(blade_num, QVector<double>(min_length / blade_num - 1));
    //去除恒偏量后的个叶片的振动位移，不加q
    QVector<QVector<double>> Remove_Bias_origin_vib(blade_num, QVector<double>(min_length / blade_num - 1));
    double q = 0;
    for (int iblade = 0; iblade < blade_num; ++iblade)
    {
        for (int irev = 0; irev < int((min_length / blade_num) - 1); ++irev)
        {
            Remove_Constant_Bias_vib[iblade][irev] = vibration1[iblade][irev] - mean_vibration[iblade] + q;
            Remove_Bias_origin_vib[iblade][irev]=vibration1[iblade][irev] - mean_vibration[iblade];
        }
        q += 200;  // 每个叶片增加100的偏量
    }

    // 发出信号
    emit SendVibData(vibration1, Speed_Frenquence, Roll_Cycle_Number,
                     Remove_Constant_Bias_vib,Remove_Bias_origin_vib);
    //return std::make_tuple(vibration1, Speed_Frenquence, Roll_Cycle_Number, Remove_Constant_Bias_vib);

}

void DataCalculation::SetParameter(int num_blade1, double Radius1, int SampleRate1)
{
    num_blade=num_blade1;
    Radius=Radius1;
    SampleRate=SampleRate1;

    std::tie(mat_C, mat_B) = createMatrix(num_blade, FirstFitOrder);//只用于一阶拟合，监测使用，但是计算的位移不准确
    std::tie(SeconedFit_Speed_matrix,Theory_Time_matrix)=createMatrix(num_blade, SeconedFitOrder);


    ChannelDataAccumulator.resize(16); // 假设有16个通道的数据

    InstableDataTrace.resize(16,false);//用于追踪前2000个数据是否移除，并初始化为false
    riseDataBuffer.resize(16);
    fallDataBuffer.resize(16);
    DeleteNoiseRiseData.resize(16);
    DeleteNoiseFallData.resize(16);
    FallData_Finall.resize(16);

    // 确保存储结构有足够的空间
    vibrationData.resize(16);//多层嵌套
    for (int channel = 0; channel < vibrationData.size(); ++channel)
    {
        vibrationData[channel].resize(num_blade); // 为每个叶片编号创建空间，这也是导致最后一个是空数组的原因，key与value不匹配
        for (int leaf = 0; leaf < num_blade; ++leaf)
        {
            vibrationData[channel][leaf].reserve(10000); // 假设初始时每个叶片至少能存100个数据点
        }
    }

    speedData.resize(16);
    for (int channel = 0; channel < speedData.size(); ++channel)
    {
        speedData[channel].reserve(10000); // 假设初始时每个通道至少能存100个转速数据点
    }

    cycleData.resize(16);
    for (int channel = 0; channel < cycleData.size(); ++channel)
    {
        cycleData[channel].reserve(10000); // 假设初始时每个通道至少能存100个圈数数据点
    }
}

void DataCalculation::SetIndexZero()
{
    // 遍历每个通道的每个叶片，将振动数据清空但保留已分配的空间
    for (int channel = 0; channel < vibrationData.size(); ++channel)
    {
       for (int leaf = 0; leaf < vibrationData[channel].size(); ++leaf)
       {
           vibrationData[channel][leaf].clear();
       }
    }

    // 对于速度数据和周期数据，由于它们是二维的，直接清空每个通道的数据
    for (int channel = 0; channel < speedData.size(); ++channel)
    {
       speedData[channel].clear();
    }

    for (int channel = 0; channel < cycleData.size(); ++channel)
    {
       cycleData[channel].clear();
    }

    for (size_t channel = 0; channel < ChannelDataAccumulator.size(); ++channel)
    {
       ChannelDataAccumulator[channel].clear();
       riseDataBuffer[channel].clear();
       fallDataBuffer[channel].clear();
       DeleteNoiseRiseData[channel].clear();
       DeleteNoiseFallData[channel].clear();
       FallData_Finall[channel].clear();
    }
    InstableDataTrace.clear();//清除前2000个数据的标志位，清零后重置
    InstableDataTrace.resize(riseDataBuffer.size(), false); // 重置并初始化为 false
    // 重置圈数计数器
    cycleCounter.fill(0);
}

QString DataCalculation::matrixToQString(const Eigen::MatrixXd &matrix)
{
   QString result;
   for (int i = 0; i < matrix.rows(); ++i) {
       for (int j = 0; j < matrix.cols(); ++j) {
           result += QString::number(matrix(i, j)) + " ";
       }
       result += "\n";
   }
   return result;
}

std::tuple<std::vector<uint64_t>, std::vector<uint64_t> > DataCalculation::DeleteNoiseData(const std::vector<uint64_t> &riseTimes,
                                                                                           const std::vector<uint64_t> &fallTimes)
{
    std::vector<uint64_t> cleanRiseTimes;
    std::vector<uint64_t> cleanFallTimes;

    if (riseTimes.size() != fallTimes.size())
    {
        throw std::runtime_error("Delete Noise Data is not equal");
    }

    std::vector<uint64_t> diffs(riseTimes.size());

    for (size_t i = 0; i < riseTimes.size(); ++i)
    {
        diffs[i] = std::abs(static_cast<int64_t>(riseTimes[i]) - static_cast<int64_t>(fallTimes[i]));
    }

    //0ULL 的原因是为了确保 std::accumulate 的起始值是一个 uint64_t 类型的无符号长整型常量
    uint64_t threshold = std::accumulate(diffs.begin(), diffs.end(), 0ULL) / diffs.size();

    for (size_t i = 0; i < diffs.size(); ++i)
    {
        if (diffs[i] >= 0.7 * threshold)
        {
            cleanRiseTimes.push_back(riseTimes[i]);
            cleanFallTimes.push_back(fallTimes[i]);
        }
    }

    return std::make_tuple(cleanRiseTimes, cleanFallTimes);
}

// 计算中位数
double median(std::vector<double> &vec)
{
    std::nth_element(vec.begin(), vec.begin() + vec.size() / 2, vec.end());
    return vec[vec.size() / 2];
}

void DataCalculation::remove_large_gap_pulses(std::vector<uint64_t> &data,
                                              std::vector<uint64_t> &del_noise_data,
                                              size_t window_size,
                                              double threshold_min,
                                              double threshold_max,
                                              double interval_threshold_factor)
{
    size_t num_windows = data.size() / window_size;

    for (size_t w = 0; w < num_windows; ++w)
    {
        size_t start = w * window_size;
        size_t end = start + window_size;
        std::vector<uint64_t> window_data(data.begin() + start, data.begin() + end);

        std::vector<double> intervals(window_data.size() - 1);

        for (size_t i = 0; i < window_data.size() - 1; ++i)
        {
            intervals[i] = static_cast<double>(window_data[i + 1] - window_data[i]);
        }

        double med = median(intervals);
        double threshold = med * interval_threshold_factor;//用于剔除缺项导致的过大间隔，防止影响均值的计算

        std::vector<double> filtered_intervals;
        for (double interval : intervals)
        {
            if (interval <= threshold)
            {
                filtered_intervals.push_back(interval);
            }
        }

        if (filtered_intervals.empty())
        {
            continue;
        }

        //用于剔除小间隔误差脉冲的参考阈值
        double dynamic_threshold = std::accumulate(filtered_intervals.begin(), filtered_intervals.end(), 0.0) / filtered_intervals.size();

        double min_dynamic_threshold=dynamic_threshold*threshold_min;//阈值最小值
        double max_dynamic_threshold=dynamic_threshold*threshold_max;//阈值最大值

        for (size_t i = 0; i < window_data.size(); ++i)
        {
            if (del_noise_data.size() == 0 || (window_data[i] - del_noise_data.back()) > min_dynamic_threshold)
            {
                if ((del_noise_data.size()>=2) && (window_data[i] - del_noise_data.back()) > max_dynamic_threshold)
                {
                    uint64_t last_interval = del_noise_data.back() - *(del_noise_data.end() - 2);
                    int num_missing = static_cast<int>((window_data[i] - del_noise_data.back()) / last_interval);
                    if (num_missing > 1)
                    {
                        double interval = static_cast<double>(window_data[i] - del_noise_data.back()) / num_missing;
                        for (int m = 1; m < num_missing; ++m)
                        {
                            del_noise_data.push_back(del_noise_data.back() + static_cast<uint64_t>(interval));
                        }
                    }
                    else
                    {
                        del_noise_data.push_back(window_data[i]);
                    }
                }
                else
                {
                     del_noise_data.push_back(window_data[i]);
                }
            }
        }
    }
    data.erase(data.begin(),data.begin()+num_windows*window_size);//第二次降噪结束后，及时移除
}

void DataCalculation::filterVibrationData(QVector<QVector<double>>& vibrationData)
{
    size_t num_blade = vibrationData.size(); // 叶片的数量
    size_t num_points = vibrationData[0].size(); // 每个叶片的振动数据点数量
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.8, 1.0); // 随机生成0.8至1.0之间的数

    std::vector<double> medianAbsVibData(num_blade, 0.0); // 存放每个叶片振动绝对值的中位数

    // 先计算每个叶片的振动绝对值的中位数
    for (size_t leafIndex = 0; leafIndex < num_blade; ++leafIndex)
    {
        std::vector<double> absVibData;
        absVibData.reserve(num_points);

        // 收集当前叶片所有点的振动绝对值
        for (size_t point = 0; point < num_points; ++point)
        {
            absVibData.push_back(std::abs(vibrationData[leafIndex][point]));
        }

        // 计算振动数据的中位数
        std::nth_element(absVibData.begin(), absVibData.begin() + absVibData.size() / 2, absVibData.end());
        medianAbsVibData[leafIndex] = absVibData[absVibData.size() / 2];
    }

    // 然后进行比较并替换异常值
    for (size_t leafIndex = 0; leafIndex < num_blade; ++leafIndex)
    {
        for (size_t point = 0; point < num_points; ++point)
        {
            // 检查当前叶片的振动数据是否大于中位数的10倍
            if (std::abs(vibrationData[leafIndex][point]) > 10* medianAbsVibData[leafIndex])
            {
                // 将当前点设置为中位数的0.8至1倍之间的随机值，并保留原始符号
                double sign = vibrationData[leafIndex][point] < 0 ? -1.0 : 1.0;
                vibrationData[leafIndex][point] = sign * medianAbsVibData[leafIndex] * dis(gen);
            }
        }
    }
}

