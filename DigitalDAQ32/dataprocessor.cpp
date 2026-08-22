#include "dataprocessor.h"
#include<workfunction.h>
#include<QDebug>
#include<globalbuffer.h>
#include "semmanager.h"
DataProcessor::DataProcessor(ringbuffer *pBuffer, QObject *parent)
             : QObject(parent)
             ,m_buffer(pBuffer)
             ,m_stopthread(false)
{
             timer.start();//用于定时绘图
}

void DataProcessor::processData()
{
    SemManager *sm=SemManager::getInstance();
    while (m_stopthread)
    {
            void* GetData=malloc(2*1024);//分配后记得释放
            QVector<QVector<double>> xPlotData;
            QVector<QVector<double>> yPlotData;
            xPlotData.resize(16*1024);
            yPlotData.resize(16*1024);
            if(RingBuffer_AvaiData(m_buffer)>=2*1024)
            {
                sm->semB.acquire();
                RingBuffer_Read(m_buffer,GetData,2*1024);
                sm->semA.release();
                int16_t* int16DataBuffer = static_cast<int16_t*>(GetData);
                DataAcquire(int16DataBuffer,2*1024,xPlotData,yPlotData);
            }
            if (timer.elapsed() > 100&&!RingBuffer_isfull(m_buffer))
            {
                timer.restart();
                emit PlotData(xPlotData,yPlotData);
            }
            free(GetData);//每次分配都有释放内存，不然会内存泄露，导致程序异常结束
    }
    QThread::currentThread()->quit();//循环结束后，自动暂停线程

}

void DataProcessor::begin2()
{
    m_stopthread=true;
}

void DataProcessor::stop2()
{
    m_stopthread=false;
}
