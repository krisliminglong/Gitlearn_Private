#ifndef GLOBALBUFFER_H
#define GLOBALBUFFER_H

#include <cstddef>  // 包含 size_t 的定义
#include <QMutex>
#include <QWaitCondition>
#include<QDebug>
#include<workfunction.h>

extern void* GlobalDataBuffer;
extern size_t GlobalBufferSize;
extern size_t currentEnd;
extern QMutex gMutex;
extern QWaitCondition gNewdataAvailable;


#endif // GLOBALBUFFER_H
