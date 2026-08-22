#include "datathread.h"
#include "workfunction.h"
#include<QDebug>
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
#include <QFile>  // 包含头文件
#include <fstream>
#include<queue>
#include<QDataStream>
#include <QElapsedTimer>
#include <cmath>
#include "calculation.h"
#include <QDir>
#include <QDateTime>
DataThread::DataThread(QObject *parent)
           : QObject(parent)
           ,Sample(200*1000)//初始化采样率位200k
           ,m_stop(false)//初始化停止位
           ,m_channels(0)//这个是看看那些通道被激活
           ,m_choosechannel(0)//这个是主线程传过来的一个标准位，没啥用
           ,SetZero(false)//用来重置每次缓冲区中上一次循环中的最后一个样本0/1状态
           ,AlldataFile(nullptr)//把路径文件初始化
{

}

void DataThread::dataprocced()
{
    drv_handle hDrv=spcm_hOpen("/dev/spcm0");
    if (!hDrv) {
        qDebug() << "无法打开数据采集卡。";
        return;
    }
    int32 dwError;//没有错误的时候值是0
    int64 llTotalBytes = 0;//总的字节数
    int64 llAvailBytes =0;//可用字节数
    int64 llBytePos=0;//当前数据开始的位置
    int64 lSamplerate;//当前的采样率
    int32 lChCount;//激活的通道数量
    //采样率的设置
    spcm_dwSetParam_i32 (hDrv, SPC_CLOCKMODE, SPC_CM_INTPLL); // Enables internal programmable quartz 1
    spcm_dwSetParam_i64 (hDrv, SPC_SAMPLERATE, Sample);// Set internal sampling rate to 125MHz
    spcm_dwSetParam_i32 (hDrv, SPC_CLOCKOUT, 0);// enable the clock output of the card（1是输出，0输出）
    spcm_dwGetParam_i64 (hDrv, SPC_SAMPLERATE, &lSamplerate); // Read back the programmed sample rate and print

    //激活通道数量的设置
    spcm_dwSetParam_i32 (hDrv, SPC_CHENABLE, m_channels);// only one channel activated
    spcm_dwGetParam_i32 (hDrv, SPC_CHCOUNT,&lChCount);//读取激活的通道数据

    //设置采集模式
    spcm_dwSetParam_i32 (hDrv, SPC_CARDMODE, SPC_REC_FIFO_SINGLE);// set the FIFO single recording mode

    spcm_dwSetParam_i64 (hDrv, SPC_PRETRIGGER, 1024);// 1 kSample of data before trigger

    //分配512KB的缓冲区，256个数据 2 bytes per sample
    int64        llSWBufSize =      MEGA_B(16);
    int64        llNotifySize =     MEGA_B(1);

    void* pvData = pvAllocMemPageAligned (llSWBufSize);//实际的缓冲区
    spcm_dwDefTransfer_i64 (hDrv, SPCM_BUF_DATA, SPCM_DIR_CARDTOPC, llNotifySize,pvData, 0, llSWBufSize);

    // now we start the acquisition and wait for the first block
    dwError = spcm_dwSetParam_i32 (hDrv, SPC_M2CMD, M2CMD_CARD_START | M2CMD_CARD_ENABLETRIGGER);
    dwError = spcm_dwSetParam_i32 (hDrv, SPC_M2CMD, M2CMD_DATA_STARTDMA | M2CMD_DATA_WAITDMA);


//---------需要32通道采集是需要放开。以数据保存的路径创建32通道的bin文件---------------------------

//    if(m_choosechannel==10000)//如果32通道都启动了，才创建文件，只有32通道时才启动
//    {
//        binfilepath=CreateAllDataSave(dataPath);//创建一个存储全部通道数据的bin文件
//        if (!AlldataFile || !AlldataFile->isOpen()) //判断文件在不在或者是不是能打开
//        {
//           qDebug() << "File is not open for writing!";
//           return;
//        }
//    }

//---------------------32通道创建一个bin文件存储到这里结束--------------------------------------



//----------------------模拟用的采集过程用的，真正采集时需要注释掉---------------------------------

    //这是用来离线模拟的
    std::string RisefilePath = "E:\\DifferentSampleRate\\GOOD_NEWDATA_32_100M_69000um\\Data_20240713_121037\\RiseData\\riseChannelTimes_4.bin";
    std::string FallfilePath = "E:\\DifferentSampleRate\\GOOD_NEWDATA_32_100M_69000um\\Data_20240713_121037\\FallData\\fallChannelTimes_4.bin";

    std::vector<std::vector<uint64_t>> Risenewdata = ReadChannelData(RisefilePath);
    std::vector<std::vector<uint64_t>> Fallnewdata = ReadChannelData(FallfilePath);


    int dataStartIndex = 0;//其实索引
    const int dataChunkSize = 6000;//每次只读取200个数据
    QElapsedTimer mainTimer;//定时发送用的
    mainTimer.start();

//----------------------模拟到这里结束-----------------------------------------------


    while (!dwError&&!m_stop)
    {
        spcm_dwGetParam_i64 (hDrv, SPC_DATA_AVAIL_USER_LEN, &llAvailBytes); // read out the available bytes
        spcm_dwGetParam_i64 (hDrv, SPC_DATA_AVAIL_USER_POS, &llBytePos);//读取缓冲区的起始位置

        int64 llBufferFillPromille;//读取硬件的填充情况
        spcm_dwGetParam_i64 (hDrv, SPC_FILLSIZEPROMILLE, &llBufferFillPromille);

        if ((llAvailBytes+llBytePos)>= llSWBufSize)
         {
            llAvailBytes = llSWBufSize-llBytePos;//超过缓冲区后，只处理末尾
         }
//        llTotalBytes += llAvailBytes;
//        int64 llBytePos16 = llBytePos/2;  // 把位置从字节转换为 int16_t 的数量
//        int64 llAvailBytes16 = llAvailBytes/2 ;  // 把可用字节转换为 int16_t 的数量
//        qDebug() <<"可用数据量"<<llAvailBytes16;
//        qDebug() <<"数据的位置"<< llBytePos16   ;
//        qDebug() <<"数据填充情况"<<llBufferFillPromille/10<<"%";

        if (llAvailBytes> 0)
        {
//-------------------------------这里是真正采集时用的代码，使用时需要放开-------------------------------


            //这是16通道的采集代码
            if(m_choosechannel==10086||m_choosechannel==10010)//16通道的代码
            {
                int16_t* int16DataBuffer = static_cast<int16_t*>(pvData);
                QElapsedTimer timer3;
                timer3.start();
                //DataAcquire(int16DataBuffer,llAvailBytes,llBytePos);//多线程采集的join版本

                //async异步多线程采集的版本，只返回上升沿或者下降沿，无降噪
                //std::vector<std::vector<uint64_t>> data=AsyncOfDataAcquire(int16DataBuffer,
                //                              llAvailBytes,llBytePos,dataPath,&SetZero);
                //emit EmitAllChannelsData(data);//只发送上升沿或者下降沿

                //同时返回上升沿和下降沿用于，有降噪处理
//                std::pair<std::vector<std::vector<uint64_t>>, std::vector<std::vector<uint64_t>>>
//                                               RiseAndFallResult=DouleReturnAsyncOfDataAcquire
//                                               (int16DataBuffer,llAvailBytes,llBytePos,dataPath,&SetZero);
//                emit EmitAllChannelsRiseAndFallData(RiseAndFallResult.first,RiseAndFallResult.second);//同时发送上升沿和下降

                //qDebug()<<"多线程处理用时"<<timer3.elapsed()<<"毫秒";
           }

           //下面是32通道的采集代码，只能进行32通道的采集的保存，但是没有处理显示过程
           if(m_choosechannel==10000)//32通道
           {
                 char* dataBuffer = static_cast<char*>(pvData) + llBytePos;
                 if (AlldataFile && AlldataFile->isOpen())
                 {
                    AlldataFile->write(dataBuffer, llAvailBytes);
                 }
           }


//==============这是用来模拟用的,而且只有16通道时，才能才启动模拟===================================//

            if(mainTimer.elapsed()>1000 && (m_choosechannel==10086||m_choosechannel==10010))
            {
                mainTimer.restart();
                std::vector<std::vector<uint64_t>> data;
                std::vector<std::vector<uint64_t>> data1;
                // 从data中提取当前数据块
                for (auto &channel : Risenewdata)
                {
                    std::vector<uint64_t> chunk(channel.begin() + dataStartIndex,
                                                channel.begin() + std::min(dataStartIndex + dataChunkSize,
                                                                           static_cast<int>(channel.size())));
                    data.push_back(chunk);
                }
                for (auto &channel : Fallnewdata)
                {
                    std::vector<uint64_t> chunk(channel.begin() + dataStartIndex,
                                                channel.begin() + std::min(dataStartIndex + dataChunkSize,
                                                                           static_cast<int>(channel.size())));
                    data1.push_back(chunk);
                }

                emit EmitAllChannelsRiseAndFallData(data,data1);

                if((dataStartIndex + dataChunkSize)>=static_cast<int>(Risenewdata[0].size()))
                {
                    break;//如果读取完成就跳出循环
                }
                dataStartIndex += dataChunkSize; // 更新开始索引
            }

//====================模拟数据发送的过程到这里结束=============================================//
        }

    // now we free the number of bytes and wait for the next buffer
    spcm_dwSetParam_i64 (hDrv, SPC_DATA_AVAIL_CARD_LEN, llAvailBytes);
    dwError = spcm_dwSetParam_i32 (hDrv, SPC_M2CMD, M2CMD_DATA_WAITDMA);
    }
    // 停止数据采集和传输
    spcm_dwSetParam_i32 (hDrv, SPC_M2CMD, M2CMD_CARD_STOP | M2CMD_DATA_STOPDMA);
    vFreeMemPageAligned(pvData,llSWBufSize);//释放缓存，一定要释放，不然会内存泄漏
    spcm_vClose (hDrv);


//    if(m_choosechannel==10000)//如果32通道被打开，先关闭文件后，再进入数据分离的函数
//    {
//        if (AlldataFile && AlldataFile->isOpen()) //如果32通道的文件打开或者文件创建了，记得关闭文件
//        {
//            AlldataFile->close();
//            qDebug() <<"文件已经关闭";
//        }
//        qDebug() <<"准备进入32通道函数";
//        Read_32Channel_DataToSingle(binfilepath,dataPath,m_choosechannel);//采集完后分离数据
//        qDebug() <<"数据已经读取完毕";
//    }

    QThread::currentThread()->quit();
}

QString DataThread::CreateAllDataSave(const QString &path)
{
    QDir dir(path);
    if (!dir.exists()) {
       qWarning() << "Directory does not exist:" << path;
       return path;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString fileName = QString("All_Channel_Data_%1.bin").arg(timestamp);
    QString filePath = dir.filePath(fileName);

    AlldataFile = new QFile(filePath);
    if (!AlldataFile->open(QIODevice::WriteOnly))
    {
        qWarning() << "Failed to open file for writing:" << filePath;
        delete AlldataFile;
        AlldataFile = nullptr;
    }
   qDebug()<<"filePath"<<filePath;
   return filePath;
}



void DataThread::setSample(int value)
{
    Sample= value;//设置采样率
}

void DataThread::stop()
{
    m_stop=true;
    SetZero=false;
}
void DataThread::begin()
{
    m_stop=false;
    SetZero=true;
}

void DataThread::setChannels(uint32_t channels,uint32_t choosechannel)
{
     m_channels = channels;
     m_choosechannel=choosechannel;
}

void DataThread::reveivepath(const QString &path)
{
     dataPath=path;
}

std::vector<std::vector<uint64_t> > DataThread::ReadChannelData(const std::string &filePath)
{
   std::vector<std::vector<uint64_t>> data;

   std::ifstream inputFile(filePath, std::ios::binary);
   if (!inputFile)
   {
       std::cerr << "Failed to open file: " << filePath << std::endl;
       return data;
   }

   std::vector<uint64_t> channelData;
   uint64_t value;

   // 读取文件中的 uint64_t 数据
   while (inputFile.read(reinterpret_cast<char*>(&value), sizeof(uint64_t))) {
       channelData.push_back(value);
   }

   inputFile.close();

   // 假设这是单通道数据，添加到二维向量中
   data.push_back(channelData);

   return data;
}

