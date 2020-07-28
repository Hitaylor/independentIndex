#include <stdio.h>
#include <malloc.h>

void print(char *, int);
int g1=12;
long g2;
void main(){
  char *s1 = "abcde";
  char *s2 = "abcde";
  char s3[] = "abcd";
  long int *s4[100];
  char *s5 = "abcde";//常量字符串"abcde"在常量区，但是s1,s2,s5本身在stack上，但它们用有相同的地址
  int a = 5;
  int b = 6; //a和b在stack上，所以&a>&b
  const int c =10;
  sleep(60);
  printf("变量地址\n&s1=%p\n&s2=%p\n&s3=%p\n&s4=%p\n&s5=%p\ns1=%p\ns2=%p\ns3=%p\ns4=%p\ns5=%p\na=%p\nb=%p\n",&s1,&s2,&s3,&s4,&s5,s1,s2,s3,s4,s5,&a,&b);
  printf("&g1=%p\n &g2=%p\n&c=%p\n",&g1,&g2,&c);
  printf("变量地址在进程调用中");
  print("ddddddd",5);
  printf("main=%p, print=%p\n",main,print);
  while(1){}
}
void print(char *str, int p)
{
  char *s1 = "abcde";
  char *s2 = "abcde";
  char s3[] = "abcd";
  long int *s4[100];
  char *s5 = "abcde";//常量字符串"abcde"在常量区，但是s1,s2,s5本身在stack上，但它们用有相同的地址
  int a = 5;
  int b = 6; //a和b在stack上，所以&a>&b
  int c;
  int d;
  char *q = str;
  int m =p;
  char *r=(char*)malloc(1);
  char *w = (char*)malloc(1);
   printf("变量地址\n&s1=%p\n&s2=%p\n&s3=%p\n&s4=%p\n&s5=%p\ns1=%p\ns2=%p\ns3=%p\ns4=%p\ns5=%p\na=%p\nb=%p\n",&s1,&s2,&s3,&s4,&s5,s1,s2,s3,s4,s5,&a,&b);
   printf("str=%p\nq=%p\n&q=%p\n&p=%p\n&m=%p\nr=%p\nw=%p\n&r=%p\n&w=%p\n",&str,q,&q,&p,&m,r,w,&r,&w);
}