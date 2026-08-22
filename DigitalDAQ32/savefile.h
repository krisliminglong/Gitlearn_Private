#ifndef SAVEFILE_H
#define SAVEFILE_H
#include<QString>
#include <QFile>  // 包含头文件
#include <fstream>
#include<QDataStream>
#include<vector>
#include<iostream>
#include<QDir>
void AppendDataToFile(bool* ReSet,//用来每次停止采集后，再次开启采集时，重置static变量，创建新的文件夹
                      const QString& savePath,
                      const std::vector<std::vector<uint64_t>>& riseChannelTimes,
                      const std::vector<std::vector<uint64_t>>& fallChannelTimes,
                      const std::vector<std::vector<uint8_t>>& rowData);

#endif // SAVEFILE_H
