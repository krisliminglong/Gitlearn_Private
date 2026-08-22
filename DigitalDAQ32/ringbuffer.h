#ifndef RINGBUFFER_H
#define RINGBUFFER_H
#include<stdint.h>
#include<stdlib.h>
#include<limits.h>
#include <string.h>
#include <QMutex>
#include <algorithm> // 为了 std::min
typedef struct
{
    uint8_t* buf;
    uint32_t size;
    uint32_t read_pos;
    uint32_t write_pos;
    QMutex mutex;
}ringbuffer;

static inline int is_power_of_two(uint32_t sz)//判断是不是2的n次幂
{
    if (sz < 2) return 0;
    return (sz & (sz - 1) )== 0;
}

static inline uint32_t roundup_power_of_two(uint32_t sz)//把缓冲区长度变为2的n次幂
{
    if (sz < 2)
        sz = 2;
    int i = 0;
    for (; sz != 0; i++)
        sz >>= 1;
    return 1U << i;
}

// 函数声明
uint32_t RingBuffer_Init(ringbuffer* buffer, uint32_t buffersize);
uint32_t RingBuffer_isempty(ringbuffer* buffer);
uint32_t RingBuffer_isfull(ringbuffer* buffer);
void RingBuffer_free(ringbuffer* buffer);
uint32_t RingBuffer_AvaiData(ringbuffer* buffer);
uint32_t RingBuffer_Remain(ringbuffer* buffer);
uint32_t RingBuffer_Write(ringbuffer* buffer, void* wdatabuf, uint32_t writesize);
bool RingBuffer_Write(ringbuffer* buffer, void* wdatabuf, uint32_t writesize,uint32_t llBytePos);
uint32_t RingBuffer_Read(ringbuffer* buffer, void* rdatabuf, uint32_t readsize);
#endif // RINGBUFFER_H
