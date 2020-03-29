#include<stdio.h>

int main(){
	char write_buf[100];
	
	printf("enter data: ");
	//scanf("%s", write_buf);
	fgets(write_buf, 99, stdin);
	printf("\n%s\n",write_buf);		

	return 0;
}