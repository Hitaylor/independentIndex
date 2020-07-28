#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdint.h>
//#include "micros_mmap.h"

#define EPISODE_IOC_ALLOCRBUF	_IOW('f', 101, long)
#define EPISODE_GET_RBUFSIZE  _IOR('f', 102, long)
#define EPISODE_GET_KERNRBUFID	_IOR('f', 103, long)
#define EPISODE_SET_USERRBUF	_IOW('f', 104, long)
#define EPISODE_GET_USERRBUF	_IOR('f', 105, long)
#define EPISODE_IOC_SETRBUFREADY	_IO('f', 106)
#define EPISODE_IOC_CLEARRBUFR	_IO('f', 107)
#define EPISODE_GET_RECORDNUMBER _IOR('f',108, long)

#define BUFSIZE 1024

struct timeIndex{
    uint64_t prev;
    uint64_t next;
    uint32_t timestamp;
    uint64_t offset;
    uint32_t recLen;
};
struct timeIndexQueryStruct{
     uint32_t timeStart;
     uint32_t timeEnd;
     uint64_t startPos;
     uint8_t  forward =;
     struct timeIndex *ti;
     uint32_t count;
};

void main()
{
	char *name = "/mnt/episode/idx1.txt";

  int fd;
  int loopnum = 1;
  struct timeIndex * ti = NULL;
  struct timeIndexQueryStruct  tiqs ;
  tiqs.ti=ti;
  tiqs.timeStart = 0;
  tiqs.timeStart= 0;
  tiqs.startPos =0;
  tiqs.count =0;
  unsigned long  addr = &tiqs;
  for (int i = 0; i < loopnum; i++)
  {
    printf("loopnum = %d\n", i);
    fd = open(name,  O_RDONLY, 0600);
    if (fd < 0)
    {
      printf("open file failed!");
      exit(1);
    }
    int32_t size = 0;
    char buf[BUFSIZE];
    char buf1[] = "abdc123", buf2[] = "qwertyui", buf3[] = "qazwsxedcvfr4321";
    int32_t len1 = sizeof(buf1) - 1, len2 = sizeof(buf2) - 1, len3 = sizeof(buf3) - 1;

 

	ioctl(fd, EPISODE_GET_RECORDNUMBER, &addr);
    lseek(fd, 0, SEEK_SET);
    if ((size = read(fd, buf, BUFSIZE)) < 0)
    {
      perror("read:");
      exit(1);
    }
    else
      printf("bytes read from file:%d\n", size);

   

    for (int j = 0; j < BUFSIZE; j++)
    {

      if ((j+1)%16==0){
        printf("%02x \n",buf[j]);
      }else{
        printf("%02x ",buf[j]);
      }
    }
    printf("\n");
    
   //再读一次
    if ((size = read(fd, buf, BUFSIZE)) < 0)
    {
      perror("read:");
      exit(1);
    }
    else
      printf("bytes read from file:%d\n", size);
 for (int j = 0; j < BUFSIZE; j++)
    {

      if ((j+1)%16==0){
        printf("%02x \n",buf[j]);
      }else{
        printf("%02x ",buf[j]);
      }
    }

    printf("buf addr:%0x\n",buf);


    if (close(fd) < 0)
    {
      perror("close:");
      exit(1);
    }
    else
      printf("Close hello.c\n");
  }
  exit(0);
}
