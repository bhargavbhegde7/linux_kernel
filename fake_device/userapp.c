#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include <unistd.h>

#define DEVICE "/dev/batmobile"

int main(){
	int i, fd;
	char ch, write_buf[100], read_buf[100];

	fd = open(DEVICE, O_RDWR);

	if(fd == -1){
		printf("file %s either doesn't exist or has been locked by another process\n", DEVICE);
		exit(-1);
	}

	printf("r = read from device\nw = write to device\nenter command: ");
	scanf("%c", &ch);

	switch(ch){
		case 'w':
			printf("enter data: ");
			scanf("%s", write_buf);
			write(fd, write_buf, sizeof(write_buf));
			break;

		case 'r':
			read(fd, read_buf, sizeof(read_buf));
			printf("device: %s\n", read_buf);
			break;

		default:
			printf("command not recognised\n");
			break;
	}
	close(fd);

	return 0;
}