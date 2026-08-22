#include "savefile.h"
#include <QDateTime>
#include<QDebug>
void AppendDataToFile(bool* ReSet,
                      const QString& savePath,
                      const std::vector<std::vector<uint64_t>>& riseChannelTimes,
                      const std::vector<std::vector<uint64_t>>& fallChannelTimes,
                      const std::vector<std::vector<uint8_t>>& rowData)
{
    static QString lastFolderPath; // 判断是否需要重新创建文件夹
    QString folderPath;
    if (*ReSet || lastFolderPath.isEmpty()) //如果*Reset为真或者路径为空，这创建一个新的文件夹
    {
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        folderPath = savePath + "/Data_" + timestamp;// 使用当前时间戳创建一个新的文件夹名
        lastFolderPath = folderPath;//更新路径
    }
    else
    {
        folderPath = lastFolderPath;// 使用上次的文件夹路径
    }

    //创建文件夹
    QString riseFolderPath = folderPath + "/RiseData";
    QString fallFolderPath = folderPath + "/FallData";
    QString rowFolderPath = folderPath + "/RowData";

    //为各自的数据创建文件夹
    QDir().mkpath(riseFolderPath);
    QDir().mkpath(fallFolderPath);
    QDir().mkpath(rowFolderPath);

    for (size_t channel = 0; channel < 16; ++channel) {
           // 为每个通道构造文件名
           QString riseFileName = riseFolderPath + "/riseChannelTimes_" + QString::number(channel+1) + ".bin";
           QString fallFileName = fallFolderPath + "/fallChannelTimes_" + QString::number(channel+1) + ".bin";
           QString rowFileName = rowFolderPath + "/rowData_" + QString::number(channel+1) + ".bin";
           //qDebug()<<"数据路径是"<<riseFileName;
           std::ofstream riseFile(riseFileName.toStdString(), std::ios::binary | std::ios::app);
           std::ofstream fallFile(fallFileName.toStdString(), std::ios::binary | std::ios::app);
           std::ofstream rowFile(rowFileName.toStdString(), std::ios::binary | std::ios::app);

           // 打开上升沿时间文件
           if (riseFile && !riseChannelTimes[channel].empty())
            {
                riseFile.write(reinterpret_cast<const char*>(riseChannelTimes[channel].data()),
                               riseChannelTimes[channel].size() * sizeof(uint64_t));
            }

            // 打开下降沿时间文件
           if (fallFile && !fallChannelTimes[channel].empty())
            {
                fallFile.write(reinterpret_cast<const char*>(fallChannelTimes[channel].data()),
                               fallChannelTimes[channel].size() * sizeof(uint64_t));
            }

            // 打开原始数据文件
           if (rowFile && !rowData[channel].empty()) {
               rowFile.write(reinterpret_cast<const char*>(rowData[channel].data()),
                             rowData[channel].size() * sizeof(uint8_t));
           }
        // 关闭文件
        riseFile.close();
        fallFile.close();
        rowFile.close();
    }
}
