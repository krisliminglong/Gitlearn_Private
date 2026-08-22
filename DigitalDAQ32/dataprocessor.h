#ifndef DATAPROCESSOR_H
#define DATAPROCESSOR_H

#include <QObject>
#include<ringbuffer.h>
#include<workfunction.h>
#include<QTimer>
//线程处理数据类
class DataProcessor : public QObject {
    Q_OBJECT
public:
    explicit DataProcessor(ringbuffer* pBuffer,QObject *parent = nullptr);
    void processData();
private:
    ringbuffer* m_buffer;
    bool m_stopthread;
    QElapsedTimer timer;
public slots:
    void begin2();
    void stop2();

signals:
    void PlotData(const QVector<QVector<double>>& xData, const QVector<QVector<double>>& yData);//传参绘图信号

};

#endif // DATAPROCESSOR_H
