#include "workfunction.h"
#include "savefile.h"
#include <vector> // 添加这行，包含vector头文件
#include<QDebug>
#include <QFile>
#include <QDataStream>
#include<QDir>
#include<QDateTime>
#include<cmath>
#include <future>
//数据采集
void DataAcquire(int coutnumber,std::vector<std::ofstream> &channelFiles,
                 std::vector<std::ofstream> &TimestampFiles,int chooseChannel,
                 int64_t llAvailBytes16,int64_t llBytePos16,double CSamplerate ,
                 int16_t* int16DataBuffer,bool* setzero,
                 QVector<QVector<double>>& riseedgeTimes,
                 QVector<QVector<double>>& falledgeTimes)
{
   static std::vector<int> lastSampleValue(coutnumber, 0);  // 记录原始数据的前一个时刻的值
   static std::vector<double> lastSampleFallTime(coutnumber, 0);  // 初始下降沿的前一时刻的时间
   static std::vector<double> currentKeyTime(coutnumber, 0);  // 初始下降沿的当前时刻的键相时间
   static std::vector<double> lastKeyTime(coutnumber, 0);  // 初始下降沿前一个时刻的键相时间
   static int globalSampleIndex = 0;  // 添加这行代码来创建一个静态的全局样本索引变量
   if(*setzero)
   {
     globalSampleIndex = 0;  // 在这里重置globalSampleIndex
     lastSampleValue.assign(coutnumber, 0);  // 重置lastSampleValue
     lastSampleFallTime.assign(coutnumber,0);
     currentKeyTime.assign(coutnumber,0);
     lastKeyTime.assign(coutnumber,0);
   }
   //只启动16通道
   if(chooseChannel==10086||chooseChannel==10010)
   {
       for (int i = 0; i < llAvailBytes16; i++)
       {
               int16_t sample = int16DataBuffer[llBytePos16 + i];
               for (int channel = 15; channel >=0; --channel)
               {
                   double channel_time = (globalSampleIndex + i);//计算每个样本的采用时间
                   int16_t channelValue = (sample >> channel) & 0x1;
                   if (channelValue == 1 && lastSampleValue[channel] == 0)
                       {

                       }
                   else if (channelValue == 0 && lastSampleValue[channel] == 1)
                       {

                       }
                       channelFiles[channel].write(reinterpret_cast<const char*>(&channelValue), sizeof(int16_t));
               }
        }
   }
   //启动32通道
   if(chooseChannel==10000)
   {
       for (int i = 0; i <llAvailBytes16; i= i+2)
       {
               int16_t sampleA = int16DataBuffer[llBytePos16 + i];
               int16_t sampleB = int16DataBuffer[llBytePos16 + i + 1];
               for (int bit = 15; bit >= 0; --bit)
               {
                   double channel_time16 = globalSampleIndex+i;//计算A每个样本的采用时间，这个是每2个样本算一次采样
                   double channel_time32 = globalSampleIndex+i;//计算B每个样本的采用时间
                   int16_t channelValueA = (sampleA >> bit) & 0x1;
                   if (channelValueA == 1 && lastSampleValue[bit] == 0)
                       {
                       // 上升沿
                       }
                   else if (channelValueA == 0 && lastSampleValue[bit] == 1)
                       {
                       // 下降沿
                       }
                       lastSampleValue[bit] = channelValueA;//更新上一个值
                       channelFiles[bit].write(reinterpret_cast<const char*>(&channelValueA), sizeof(int16_t));
                   //------------------------分割符--------------------------------------------------------
                   int16_t channelValueB = (sampleB >> bit) & 0x1;
                   if (channelValueB == 1 && lastSampleValue[bit + 16] == 0)
                       {
                        // 上升沿

                       }
                   else if (channelValueB == 0 && lastSampleValue[bit + 16] == 1)
                       {
                       // 下降沿
                       }
                       lastSampleValue[bit + 16] = channelValueB;
                       channelFiles[bit + 16].write(reinterpret_cast<const char*>(&channelValueB), sizeof(int16_t));
               }
       }
    }
   globalSampleIndex += llAvailBytes16;  // 更新全局样本索引
   *setzero=false;
}



void DataAcquire(int16_t* int16DataBuffer,uint32_t AvibleDataByteSize,uint32_t llBytePos)
{

    static int16_t lastSampleState = int16DataBuffer[0]; // 函数内静态变量，用于跨函数调用保存最后一个样本状态,初始化为开始采集后的第一个样本
    static uint64_t global_index = 0;//样本索引，用来计数

    uint32_t int16samle = AvibleDataByteSize / 2;//看看有多少个16位的样本
    uint32_t int16bytepos=llBytePos / 2;//16位样本起始位置

    uint32_t num_threads = std::thread::hardware_concurrency(); // 获取硬件线程数
    uint32_t chunk_size = int16samle / num_threads;//把可用字节分到目前可用的线程上

    std::vector<std::thread> threads;//创建一个线程属性的容器
    for (uint32_t t = 0; t < num_threads; ++t)
    {
            uint32_t start = t * chunk_size;
            uint32_t end = (t == num_threads - 1) ? int16samle : (t + 1) * chunk_size;
            uint32_t startSampleIndex = start + int16bytepos;

            // 第一个线程使用lastSampleState作为前一个样本的状态
            int16_t prevSampleState = (t == 0) ? lastSampleState : int16DataBuffer[startSampleIndex - 1];
            threads.emplace_back(ProcessSamples, int16DataBuffer, start,
                                 t,int16bytepos, end,prevSampleState,global_index);
    }

    for (auto &thread : threads)
    {
        thread.join();
    }
    // 更新lastSampleState为当前处理的最后一个数据块的最后一个样本状态
    lastSampleState = int16DataBuffer[int16bytepos+int16samle-1];
    global_index += int16samle;
}

std::vector<std::vector<uint64_t>>
       AsyncOfDataAcquire(int16_t* int16DataBuffer,uint32_t AvibleDataByteSize,
                        uint32_t llBytePos,const QString& savePath,bool* ReSet)
{   
    // 函数内静态变量，用于跨函数调用保存最后一个样本状态,初始化为开始采集后的第一个样本
    static int16_t lastSampleState = int16DataBuffer[0];
    static uint64_t global_index = 0;//样本索引，用来计数
    if(*ReSet)
    {
        lastSampleState = int16DataBuffer[0];
        global_index = 0;//样本索引，用来计数
        qDebug()<<"已经重置"<<global_index <<*ReSet;//样本索引，用来计数
    }
    uint32_t int16samle = AvibleDataByteSize / 2;//看看有多少个16位的样本
    uint32_t int16bytepos=llBytePos / 2;//16位样本起始位置

    uint32_t num_threads = std::thread::hardware_concurrency(); // 获取硬件线程数
    uint32_t chunk_size = int16samle / num_threads;//把可用字节分到目前可用的线程上

    //future是用来获取线程中返回的结果，并保存在一个容器中
    std::vector<std::future<std::tuple<std::vector<std::vector<uint64_t>>,
                                       std::vector<std::vector<uint64_t>>,
                                       std::vector<std::vector<uint8_t>>>>> futures;

    for (uint32_t t = 0; t < num_threads; ++t)
    {
            uint32_t start = t * chunk_size;
            uint32_t end = (t == num_threads - 1) ? int16samle : (t + 1) * chunk_size;
            uint32_t startPreSampleIndex = int16bytepos+start;//这个是用来获取不同线程之间前一个上升沿

            // 第一个线程使用lastSampleState作为前一个样本的状态
            int16_t prevSampleState = (t == 0) ? lastSampleState : int16DataBuffer[startPreSampleIndex - 1];
            futures.push_back(std::async(std::launch::async, ProcessSamples, int16DataBuffer,
                                         start, t ,int16bytepos,end,
                                         //每个数据块块的起始索引也得加上起始的值，但注意是加上start
                                         prevSampleState, global_index));

    }

    // 存储最终结果，每个通道一个容器
    std::vector<std::vector<uint64_t>> finalRiseChannelData(16);
    std::vector<std::vector<uint64_t>> finalFallChannelData(16);
    std::vector<std::vector<uint8_t>> finalRowChannelData(16);

    // 等待所有线程完成并汇总数据
    for (auto &f : futures) //futures是一种获取线程处理结果的方式
    {
        // 获取由 std::future 返回的 tuple
        auto result = f.get();

        // 解包 tuple 获取三种数据
        auto& localRiseData = std::get<0>(result);
        auto& localFallData = std::get<1>(result);
        //auto& localRowData =  std::get<2>(result);

        // 汇总上升沿时间数据
        for (size_t channel = 0; channel < localRiseData.size(); ++channel) {
            finalRiseChannelData[channel].insert(finalRiseChannelData[channel].end(), localRiseData[channel].begin(), localRiseData[channel].end());
        }

        // 汇总下降沿时间数据
        for (size_t channel = 0; channel < localFallData.size(); ++channel) {
            finalFallChannelData[channel].insert(finalFallChannelData[channel].end(), localFallData[channel].begin(), localFallData[channel].end());
        }

//        // 汇总原始数据
//        for (size_t channel = 0; channel < localRowData.size(); ++channel) {
//            finalRowChannelData[channel].insert(finalRowChannelData[channel].end(), localRowData[channel].begin(), localRowData[channel].end());
//        }
    }

    //保存数据
    AppendDataToFile(ReSet,savePath,finalRiseChannelData,finalFallChannelData,finalRowChannelData);
    *ReSet = false;//把标志位置零

    // 更新lastSampleState为当前处理的最后一个数据块的最后一个样本状态
    lastSampleState = int16DataBuffer[int16bytepos+int16samle-1];
    global_index += int16samle;

    return finalRiseChannelData;//返回上升沿用于计算
}

std::tuple<std::vector<std::vector<uint64_t>>, // 上升沿时间
           std::vector<std::vector<uint64_t>>, // 下降沿时间
           std::vector<std::vector<uint8_t>>>  // 原始数据
ProcessSamples(int16_t *int16DataBuffer, uint32_t start,int index,uint32_t int16bytepos,
               uint32_t end,int16_t prevSampleState, uint64_t global_index)
{
    std::vector<int16_t> prevChannelValues(16);// 用于存储前一个样本中每个通道的状态
    std::vector<std::vector<uint64_t>> localRiseChannelTimes(16); // 临时存储每个通道的上升沿时间64bit
    std::vector<std::vector<uint64_t>> localFallChannelTimes(16); // 临时存储每个通道的下降沿时间64bit
    std::vector<std::vector<uint8_t>> localRowData(16); // 临时存储每个通道的原始数据8bit

    for (int channel = 0; channel < 16; ++channel) {
        prevChannelValues[channel] = (prevSampleState >> channel) & 0x1;
        localRowData[channel].reserve(end - start);
    }// 初始化每个通道的前一个状态
    for (uint32_t i = start; i < end; i++)
      {
            int16_t sample = int16DataBuffer[int16bytepos+i];
            for (int channel = 15; channel >= 0; --channel)
            {
                uint64_t channel_time = (global_index + i); //计算每个样本的采集时间
                uint8_t channelValue = (sample >> channel) & 0x1;//与运算取出各通道的数据
//                localRowData[channel].push_back(channelValue);  //原始0/1二进制数据
                // 检测上升沿和下降沿
                if (channelValue != prevChannelValues[channel])
                {
                   if (channelValue == 1) {   
                       localRiseChannelTimes[channel].push_back(channel_time);  // 上升沿
                   }
                   else
                   {
                       localFallChannelTimes[channel].push_back(channel_time);   // 下降沿
                   }
                }
                prevChannelValues[channel] = channelValue;  // 更新当前通道的前一个值
            }
        }

     return std::make_tuple(localRiseChannelTimes, localFallChannelTimes, localRowData);//返回一个数据集合
}



void Read_32Channel_DataToSingle(const QString &inputFilePath, const QString &outputDirPath, int chooseChannel)
{
    qDebug() <<"进去读取循环中，这个过程耗时非常长，请耐心等待";
    QFile inputFile(inputFilePath);
    if (!inputFile.open(QIODevice::ReadOnly))
    {
        qDebug() << "Failed to open input file for reading:" << inputFilePath;
        return;
    }

    QDir outputDir(outputDirPath);
    if (!outputDir.exists())
    {
        qDebug() << "Output directory does not exist:" << outputDirPath;
        return;
    }

    std::vector<std::ofstream> riseEdgeFiles(32);
    std::vector<std::ofstream> fallEdgeFiles(32);
    std::vector<std::ofstream> originalDataFiles(32); // 用于保存原始0和1数据的文件

    // 创建输出文件
    for (int i = 0; i < 32; ++i)
    {
        QString riseFileName = outputDir.filePath(QString("Channel_%1_Rise.bin").arg(i));
        QString fallFileName = outputDir.filePath(QString("Channel_%1_Fall.bin").arg(i));
        QString originalFileName = outputDir.filePath(QString("Channel_%1_Original.bin").arg(i)); // 原始数据文件
        riseEdgeFiles[i].open(riseFileName.toStdString(), std::ios::binary | std::ios::out);
        fallEdgeFiles[i].open(fallFileName.toStdString(), std::ios::binary | std::ios::out);
        originalDataFiles[i].open(originalFileName.toStdString(), std::ios::binary | std::ios::out); // 打开原始数据文件
    }

    const int bufferSize = 100*1024;//每次读取100k个int16，这个不能过大，不然会内存泄漏导致奔溃
    int16_t buffer[bufferSize];//sizeof(buffer)是4096*sizeof()

    // 初始化 lastSampleValue 为每个通道的第一个样本
    std::vector<int> lastSampleValue(32, 0);
    if (!inputFile.atEnd())
    {
        qint64 bytesRead = inputFile.read(reinterpret_cast<char*>(buffer), sizeof(buffer[0]) * 32);
        int samplesRead = bytesRead / sizeof(int16_t);
        if (samplesRead >= 32)
        {
            if (chooseChannel == 10086 || chooseChannel == 10010)
            {
                int16_t firstSample = buffer[0];
                for (int channel = 15; channel >= 0; --channel)
                {
                    lastSampleValue[channel] = (firstSample >> channel) & 0x1;
                }
            }
            else if (chooseChannel == 10000)
            {
                int16_t firstSampleA = buffer[0];
                int16_t firstSampleB = buffer[1];
                for (int bit = 15; bit >= 0; --bit) {
                    lastSampleValue[bit] = (firstSampleA >> bit) & 0x1;
                    lastSampleValue[bit + 16] = (firstSampleB >> bit) & 0x1;
                }
            }
        }
    }
    inputFile.seek(0); // 重新定位到文件开头以进行后续的读取处理
    std::fill(std::begin(buffer), std::end(buffer), 0);
    uint64_t globalSampleIndex = 0;

    while (!inputFile.atEnd())
    {
        qint64 bytesRead = inputFile.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
        int samplesRead = bytesRead / sizeof(int16_t);

        if (chooseChannel == 10086 || chooseChannel == 10010)
        {
            for (int i = 0; i < samplesRead; ++i) {
                int16_t sample = buffer[i];
                for (int channel = 15; channel >= 0; --channel)
                {
                    int16_t channelValue = (sample >> channel) & 0x1;
                    uint64_t timestamp = globalSampleIndex + i;

                    originalDataFiles[channel].write(reinterpret_cast<const char*>(&channelValue), sizeof(int16_t));

                    if (channelValue == 1 && lastSampleValue[channel] == 0)
                    {
                        riseEdgeFiles[channel].write(reinterpret_cast<const char*>(&timestamp), sizeof(uint64_t));
                    }
                    else if (channelValue == 0 && lastSampleValue[channel] == 1)
                    {
                        fallEdgeFiles[channel].write(reinterpret_cast<const char*>(&timestamp), sizeof(uint64_t));
                    }
                    lastSampleValue[channel] = channelValue;
                }
            }
            globalSampleIndex += samplesRead;
        }
        else if (chooseChannel == 10000)
        {
            for (int i = 0; i < samplesRead; i += 2)
            {
                int16_t sampleA = buffer[i];
                int16_t sampleB = buffer[i + 1];

                for (int bit = 15; bit >= 0; --bit)
                {
                    int16_t channelValueA = (sampleA >> bit) & 0x1;
                    uint64_t timestampA = globalSampleIndex + i/2; // 时间戳
                    originalDataFiles[bit].write(reinterpret_cast<const char*>(&channelValueA), sizeof(int16_t));

                    if (channelValueA == 1 && lastSampleValue[bit] == 0)
                    {
                        riseEdgeFiles[bit].write(reinterpret_cast<const char*>(&timestampA), sizeof(uint64_t));
                    }
                    else if (channelValueA == 0 && lastSampleValue[bit] == 1)
                    {
                        fallEdgeFiles[bit].write(reinterpret_cast<const char*>(&timestampA), sizeof(uint64_t));
                    }
                    lastSampleValue[bit] = channelValueA;

                    int16_t channelValueB = (sampleB >> bit) & 0x1;
                    uint64_t timestampB = globalSampleIndex + i/2; // 微秒时间戳
                    originalDataFiles[bit + 16].write(reinterpret_cast<const char*>(&channelValueB), sizeof(int16_t));

                    if (channelValueB == 1 && lastSampleValue[bit + 16] == 0)
                    {
                        riseEdgeFiles[bit + 16].write(reinterpret_cast<const char*>(&timestampB), sizeof(uint64_t));
                    }
                    else if (channelValueB == 0 && lastSampleValue[bit + 16] == 1)
                    {
                        fallEdgeFiles[bit + 16].write(reinterpret_cast<const char*>(&timestampB), sizeof(uint64_t));
                    }
                    lastSampleValue[bit + 16] = channelValueB;
                }
            }
            globalSampleIndex += samplesRead/2;//32通道时，2个样本为一次采样
        }
    }

    // 关闭所有文件
    for (auto &file : riseEdgeFiles) {
        file.close();
    }
    for (auto &file : fallEdgeFiles) {
        file.close();
    }

    for (auto &file : originalDataFiles)
    { // 关闭原始数据文件
        file.close();
    }
}


std::pair<std::vector<std::vector<uint64_t> >, std::vector<std::vector<uint64_t> > >
     DouleReturnAsyncOfDataAcquire(int16_t *int16DataBuffer, uint32_t AvibleDataByteSize,
                                   uint32_t llBytePos, const QString &savePath, bool *ReSet)
{
    static int16_t lastSampleState = int16DataBuffer[0];
    static uint64_t global_index = 0; // 样本索引，用来计数
    if(*ReSet)
    {
        lastSampleState = int16DataBuffer[0];
        global_index = 0; // 样本索引，用来计数
        qDebug() << "返回上升沿和下降沿的函数的参考索引已经重置" << global_index << *ReSet;
    }

    uint32_t int16samle = AvibleDataByteSize / 2; // 多少个16位的样本
    uint32_t int16bytepos = llBytePos / 2; // 16位样本起始位置

    uint32_t num_threads = std::thread::hardware_concurrency(); // 获取硬件线程数
    uint32_t chunk_size = int16samle / num_threads; // 把可用字节分到目前可用的线程上

    std::vector<std::future<std::tuple<std::vector<std::vector<uint64_t>>,
                                       std::vector<std::vector<uint64_t>>,
                                       std::vector<std::vector<uint8_t>>>>> futures;

    for (uint32_t t = 0; t < num_threads; ++t)
    {
        uint32_t start = t * chunk_size;
        uint32_t end = (t == num_threads - 1) ? int16samle : (t + 1) * chunk_size;
        uint32_t startPreSampleIndex = int16bytepos + start;

        int16_t prevSampleState = (t == 0) ? lastSampleState : int16DataBuffer[startPreSampleIndex - 1];
        futures.push_back(std::async(std::launch::async, ProcessSamples, int16DataBuffer,
                                     start, t, int16bytepos, end,
                                     prevSampleState, global_index));
    }

    std::vector<std::vector<uint64_t>> finalRiseChannelData(16);
    std::vector<std::vector<uint64_t>> finalFallChannelData(16);
    std::vector<std::vector<uint8_t>> finalRowChannelData(16);

    for (auto &f : futures)
    {
        auto result = f.get();

        auto& localRiseData = std::get<0>(result);
        auto& localFallData = std::get<1>(result);

        for (size_t channel = 0; channel < localRiseData.size(); ++channel)
        {
            finalRiseChannelData[channel].insert(finalRiseChannelData[channel].end(), localRiseData[channel].begin(), localRiseData[channel].end());
        }

        for (size_t channel = 0; channel < localFallData.size(); ++channel)
        {
            finalFallChannelData[channel].insert(finalFallChannelData[channel].end(), localFallData[channel].begin(), localFallData[channel].end());
        }
    }

    AppendDataToFile(ReSet, savePath, finalRiseChannelData, finalFallChannelData, finalRowChannelData);
    *ReSet = false;

    lastSampleState = int16DataBuffer[int16bytepos + int16samle - 1];
    global_index += int16samle;

    return {finalRiseChannelData, finalFallChannelData};
}
