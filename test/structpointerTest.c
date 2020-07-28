#include <stdio.h>
#include <stdlib.h>
typedef struct A { 
               int a; 
               char c;
        }AS, *AP;//*AP相当于struct A， AP是一个地址，该地址存放的是struct A的某个对象的内容
	void change(AP *ap){ //AP *是一个指针，指向地址的指针，指向存储struct A的对象的地址的指针。ap是一个指针，它的数值是一个地址
		struct A b={12,'c'};
		(*ap)=&b;
		//(*ap)->c='e'; 
		printf("&(b.a)=%p,b.a=%d,b.c=%c,&b=%p\n",&(b.a),b.a,b.c,&b);		
		printf("(*ap)->a=%d,(*ap)->c=%c,(*ap)=%p,ap=%p\n",(*ap)->a,(*ap)->c,(*ap),ap);
		 return ;
	} 
	void main(){
		struct A * a;  
		struct A c={9,'d'};
		a= &c;
		 printf("a->a=%d,a->c=%c,a=%p,&a=%p\n",a->a,a->c,a,&a);
		change(&c); 
                printf("a=%p,&a=%p\n",a,&a);
		//a =&c;
		printf("a->a=%d,a->c=%c,a=%p,&a=%p\n",a->a,a->c,a,&a);
		return ;
	}
