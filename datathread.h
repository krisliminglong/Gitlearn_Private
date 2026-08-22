#ifndef DATATHREAD_H
#define DATATHREAD_H
#include<QThread>
#include <QObject>
#include <QElapsedTimer>
#include <QVector>
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
#include <QFile>
#include<ringbuffer.h>
class DataThread : public QObject
{
    Q_OBJECT
public:
    explicit DataThread(QObject *parent = nullptr);
    void dataprocced();//线程的工作函数，这种方式比起run那种方式，可以直接传递参数

    QString CreateAllDataSave(const QString &path);//返回bin文件的路径
    double median(std::vector<double> &vec);
    //这个函数是用来读取已经采集的时间戳，离线分析用的
    std::vector<std::vector<uint64_t>> ReadChannelData(const std::string& filePath);
public slots:
    void setSample(int value);//设置采样率
    void stop();//停止线程
    void begin();//开始线程
    void setChannels(uint32_t channels,uint32_t select);//设置通道的数量
    void reveivepath(const QString& path);//接收数据保存路径

private:
    int Sample;//设置采样率
    bool m_stop;//用于线程的启动和停止的标志符合
    uint32_t m_channels;//用于激活通道的数量
    uint32_t m_choosechannel;//使用这个去依据不同的通道数量执行不同片段的代码
    QString dataPath;//数据保存路径
    QFile* AlldataFile;//用于保存全部通道的全部数据
    QString binfilepath;//用于返回32的大文件路径
    bool SetZero;//这个变量是用来重置采集函数中的起始时间

signals:
    void EmitAllChannelsData(const std::vector<std::vector<uint64_t>>& allData);//只发送上升沿或者下降沿
    void EmitAllChannelsRiseAndFallData(const std::vector<std::vector<uint64_t>>& riseData,const std::vector<std::vector<uint64_t>>& fallData);

};

#endif // DATATHREAD_H
