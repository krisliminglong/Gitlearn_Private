#include "ringbuffer.h"
#include<QDebug>
//缓冲区初始化的一些接口
uint32_t RingBuffer_Init(ringbuffer* buffer, uint32_t buffersize)//初始化
{
    buffer->buf = (uint8_t*)malloc(buffersize*sizeof(uint8_t));
    if (!is_power_of_two(buffersize))
        buffersize = roundup_power_of_two(buffersize);
    buffer->size = buffersize;
    buffer->write_pos = buffer->read_pos = 0;//初始值设置为最大值减2
    return 0;
}
uint32_t RingBuffer_isempty(ringbuffer* buffer)//缓冲区是空的吗
{
    return buffer->read_pos == buffer->write_pos;
}
uint32_t RingBuffer_isfull(ringbuffer* buffer)
{
    //缓冲区长度必须为2的n次幂才可以
    return buffer->size == buffer->write_pos - buffer->read_pos;
}
void RingBuffer_free(ringbuffer* buffer) //释放缓冲区
{
    if (buffer->buf != 0)
    {
        free(buffer->buf);
        buffer->buf = 0;
    }
    buffer->read_pos = buffer->write_pos = buffer->size=0;
}
//可用数据
uint32_t RingBuffer_AvaiData(ringbuffer* buffer)
{
    return buffer->write_pos - buffer->read_pos;
}
//剩余空间
uint32_t RingBuffer_Remain(ringbuffer* buffer)
{
    return buffer->size - RingBuffer_AvaiData(buffer);
}

//往里面写入数据
uint32_t RingBuffer_Write(ringbuffer* buffer, void* wdatabuf, uint32_t writesize)
{
    if (writesize > RingBuffer_Remain(buffer))
        return 0;
    uint32_t i = std::min(writesize, buffer->size - (buffer->write_pos & (buffer->size - 1)));
    //从(buffer->write_pos & (buffer->size - 1))位置开始，拷贝
    //这一段是处理末尾的拷贝
    memcpy(buffer->buf+(buffer->write_pos & (buffer->size - 1)), (uint8_t*)wdatabuf,i);
    memcpy(buffer->buf,(uint8_t*)wdatabuf+i, writesize-i);
    buffer->write_pos += writesize;
    return writesize;
}
//函数重载
bool RingBuffer_Write(ringbuffer *buffer, void *wdatabuf, uint32_t writesize, uint32_t llBytePos)
{
    QMutexLocker locker(&(buffer->mutex));//加锁保护
    if (writesize > RingBuffer_Remain(buffer))
        return false;
    uint32_t i = std::min(writesize, buffer->size - (buffer->write_pos & (buffer->size - 1)));
    //从(buffer->write_pos & (buffer->size - 1))位置开始，拷贝
    //这一段是处理末尾的拷贝
    memcpy(buffer->buf+(buffer->write_pos & (buffer->size - 1)), (uint8_t*)wdatabuf+llBytePos,i);
    memcpy(buffer->buf,(uint8_t*)wdatabuf+llBytePos+i, writesize-i);
    buffer->write_pos += writesize;
    return true;
}

uint32_t RingBuffer_Read(ringbuffer* buffer, void* rdatabuf, uint32_t readsize)
{
    QMutexLocker locker(&(buffer->mutex));//
    if (RingBuffer_isempty(buffer))
    {
        return 0;
    }
    //可用的数据和想要读取的数据最小的一个
    readsize = std::min(readsize, RingBuffer_AvaiData(buffer));
    //buffer->read_pos&(buffer->size-1)这个是正确在0-size-1的范围内找到索引
    //然后我们需要处理末尾的数据和readsize小，以看看是不是需要处理完末尾数据后，再从头开始处理
    uint32_t i = std::min(readsize,buffer->size-(buffer->read_pos&(buffer->size-1)));
    memcpy((uint8_t*)rdatabuf,buffer->buf+(buffer->read_pos&(buffer->size - 1)),i);//末尾
    memcpy((uint8_t*)rdatabuf+i,buffer->buf,readsize-i);//readsize-i如果小于0，会自动忽略
    buffer->read_pos += readsize;
    return readsize;
}
