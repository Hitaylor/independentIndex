#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <malloc.h>
#include <string.h>

void testA(int **p){
	
	printf("before ,p=%p *p=%p\n",p,*p);
	//*p=b;
	//printf("after,p=%p \n",p);
	int c=5;	
	*p=&c;
	printf("after,p=%p *p=%p\n",p,*p);
}
void main(){
	int *a;
	
	printf("&a=%p \n",&a);
	testA(&a);
	printf("*a=%d,   a=%p ,&a=%p\n",*a,a,&a);
}
