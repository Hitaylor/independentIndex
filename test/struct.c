struct timeIndexQuerySturct{
  unsigned long timeStart;
  unsigned long timeEnd;
  
  uint64_t startPos;
  struct timeIndex *ti;
  int count;
}

struct timeIndex{
  uint64_t prev;
  uint64_t next;
  uint32_t timestamp;
  uint64_t offset;
  uint32_t recLen;
}


struct timeIndex{
  __u32 timestamp;
  __u64 offset;
  __u32 len;
}