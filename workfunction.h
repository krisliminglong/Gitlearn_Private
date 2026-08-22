#ifndef WORKFUNCTION_H
#define WORKFUNCTION_H
#include<QString>
#include <QFile>  // 包含头文件
#include <fstream>
#include<QDataStream>
#include<vector>
#include<iostream>
#include<datathread.h>
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

//这个是最原始的版本的
void DataAcquire(int coutnumber,std::vector<std::ofstream> &channelFiles,
                 std::vector<std::ofstream> &TimestampFiles,int chooseChannel,
                 int64_t llAvailBytes16,int64_t llBytePos16,double CSamplerate ,
                 int16_t* int16DataBuffer,bool* setzero,
                 QVector<QVector<double>>& riseedgeTimes,
                 QVector<QVector<double>>& falledgeTimes);


void Read_32Channel_DataToSingle(const QString &inputFilePath, const QString &outputDirPath, int chooseChannel);


void DataAcquire(int16_t* int16DataBuffer,uint32_t AvibleDataByteSize,uint32_t llBytePos);

std::vector<std::vector<uint64_t>>//只返回下降沿
       AsyncOfDataAcquire(int16_t* int16DataBuffer,uint32_t AvibleDataByteSize,
                        uint32_t llBytePos,const QString& savePath,bool* ReSet);

std::pair<std::vector<std::vector<uint64_t>>, std::vector<std::vector<uint64_t>>>//上升沿下降沿都返回
DouleReturnAsyncOfDataAcquire(int16_t* int16DataBuffer, uint32_t AvibleDataByteSize,
                   uint32_t llBytePos, const QString& savePath, bool* ReSet);

std::tuple<std::vector<std::vector<uint64_t>>, // 上升沿时间
           std::vector<std::vector<uint64_t>>, // 下降沿时间
           std::vector<std::vector<uint8_t>>>  // 原始数据
ProcessSamples(int16_t* int16DataBuffer, uint32_t start,int index,uint32_t int16bytepos,
                                                uint32_t end, int16_t prevSampleState,uint64_t global_index);

#endif // WORKFUNCTION_H
