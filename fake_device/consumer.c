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
		read(fd, read_buf, sizeof(read_buf));
		if(strlen(read_buf) > 0){
			printf("device: %s\n", read_buf);
		}
	}
	
	close(fd);

	return 0;
}