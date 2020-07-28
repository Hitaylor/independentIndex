#ifndef __EPISODE_TIMEINDEX__
#define __EPISODE_TIMEINDEX__
#define TIMEINDEXLEN (sizeof(struct timeIndex))
#define TIMEINDEXLEN_BITS 4

/**
 * 结构体中字段定义的位置也非常重要，一定要考虑到字节对齐。
 */ 
struct timeIndex{
    __u64 offset;
    __u32 timestamp;
    __u32 recLen;
};
struct timeIndex2{
    
    __u32 timestamp;
    __u64 offset;
    __u32 recLen;
};
//sizeof(struct timeIndex2)= 24
//sizeof(struct timeIndex)=16

struct timeIndexQueryStruct{
     __u32 timeStart;
     __u32 timeEnd;
     __u64 startPos;
    // unsigned char forward = 1;
     struct timeIndex *ti;
     __u32 count;
};


#endif