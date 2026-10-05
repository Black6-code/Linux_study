#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// ./chrdevbaseAPP <filename>  <1:2> 1表示读 2表示写
// ./chrdevbaseAPP  /dev/chrdevbase 1/2     将读写操作分开
int main(int argc, char *argv[])
{
    int ret = 0;
    int fd = 0;
    char *filename = argv[1];
    char readbuf[100], writebuf[100];
    static char usrdata[] = {"usr data!"};

    if(argc != 3)
    {
        printf("Usage: %s <filename> <1:2>\r\n", argv[0]);
        printf("Error: Invalid arguments\r\n");
        return -1;
    }

    fd = open(filename, O_RDWR);

    if(fd < 0)
    {
        printf("Cnt't open file %s\r\n", filename);
        return -1;
    }

    // read
    if(atoi(argv[2]) == 1)
    {
        ret = read(fd, readbuf, 50);
        if(ret < 0)
        {
            printf("Read %s error\r\n", filename);
            return -1;
        }
        else
        {
            printf("Read %s success: %s\r\n", filename, readbuf);
        }
    }


    // write
    if(atoi(argv[2]) == 2)
    {
        memcpy(writebuf, usrdata, sizeof(usrdata));
        ret = write(fd, writebuf, 50);
        if(ret < 0)
        {
            printf("Write %s error\r\n", filename);
            return -1;
        }
        else
        {
            printf("Write %s success: %s\r\n", filename, writebuf);
        }
    }


    // close
    ret = close(fd);
    if(ret < 0)
    {
        printf("Close %s error\r\n", filename);
        return -1;
    }
    else{}

    return 0;
}
