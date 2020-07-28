#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <malloc.h>
#include <string.h>

void main(){
	char *name = "/mnt/episode/t1.txt";

	int fd;
	int loopnum=10;
	for(int i=0;i<loopnum;i++){
		printf("loopnum = %d\n",i);
		fd =open(name,O_CREAT|O_RDWR|O_APPEND|O_DIRECT,0600);
		if(fd<0) printf("create file failed!");
		int32_t off=0;
		char buf1[]="abdc123jkasjfdoiui12398712841iolksajdrilws8o7891732oijldks";
		int32_t len1=sizeof(buf1)-1;
			
		char *content;	

		content = memalign(512,512*2);
		memset(content,0,512*2);
	
		memcpy(content+off,buf1,len1);
		off +=len1;
		int retnum = write(fd,content,512*2);
		printf("write bytes :%d\n",retnum);
		close(fd);
		
		printf("write succeed!\n");
		free(content);
	}
}
