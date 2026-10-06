#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// ./ledAPP <filename>  <0:1> 
// ./ledAPP  /dev/dtsled  0  1     开灯 / 关灯

#define LED_ON  0
#define LED_OFF 1

int main(int argc, char *argv[])
{
    int fd, retvalue;
    char *filename;
    unsigned char databuf[1];

    if(argc != 3)
    {
        printf("Usage: %s <filename> <0:1>\r\n", argv[0]);
        printf("Error: Invalid arguments\r\n");
        return -1;
    }
    
    filename = argv[1];

    fd = open(filename, O_RDWR);

    if(fd < 0)
    {
        printf("Cnt't open file %s\r\n", filename);
        return -1;
    }

    databuf[0] = atoi(argv[2]);

    retvalue = write(fd, databuf, 1);
    if(retvalue < 0)
    {
        printf("Write %s error, LED control failed\r\n", filename);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

