#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
int main()
{
  int fd[2];
  pipe(fd);
  pid_t pid;
  pid =fork();
  if(pid<0){
    perror("error in fork!\n");
    return -1;
  }
  else if(pid==0){
    printf("I am the child process!\n");
    close(fd[0]);//关闭管道的读端
    int i=0;
    for(i=100;i<=120;i++){
      printf("write %d\n",i);
      write(fd[1],&i,sizeof(int));
      sleep(1);
    }
    close(fd[1]);//关闭写端
    exit(0);
  }else{
    printf("I am the parent process!\n");
    close(fd[1]);
    int x,i=0;
    for(;i<=20;i++){
      read(fd[0],&x,sizeof(int));
      printf("%d\n",x);
      setbuf(stdout,NULL);
    }
    close(fd[0]);
  }
  printf("pid==%d\n",pid);
  return 0;
}