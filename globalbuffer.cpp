#include "globalbuffer.h"

void* GlobalDataBuffer = nullptr;  // 具体定义
size_t GlobalBufferSize = 20*1024 * 1024; // 初始化为20MB
size_t currentEnd = 0;//缓冲区末尾索引
QMutex gMutex; // 在这里定义
QWaitCondition gNewdataAvailable; // 在这里定义
