#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include <unistd.h>

#define DEVICE "/dev/batmobile"
#define true 1

int main(){
	int i, fd;
	char ch, write_buf[100], read_buf[100];

	fd = open(DEVICE, O_RDWR);

	if(fd == -1){
		printf("file %s either doesn't exist or has been locked by another process\n", DEVICE);
		exit(-1);
	}

	while(true){
		printf("enter data: ");
		//scanf("%s", write_buf);
		fgets(write_buf, 99, stdin);
		write(fd, write_buf, sizeof(write_buf));
	}
	
	close(fd);

	return 0;
}