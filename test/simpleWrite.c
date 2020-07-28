#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

 main(){
  int fd,i;
  char fileName[10];
  for(i=1;i<=2;i++){
    sprintf(fileName,"%d.dat",i);//int转成char
    fd = open(fileName,O_RDWR|O_CREAT,644);
    if(fd==-1){
      perror("创建失败！\n");
      return -1;
    }
    int ssize = write(fd,&i,sizeof(int));
    if(ssize==-1){
      perror("写入失败!\n");
      return -1;
    }
  }

}