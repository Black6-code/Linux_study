#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

// ./timerAPP <filename>  
// ./timerAPP  /dev/timer  

#define CLOSE_CMD       _IO(0xEF, 1)        /* 关闭命令 */
#define OPEN_CMD        _IO(0xEF, 2)        /* 打开命令 */
#define SETPERIOD_CMD   _IOW(0xEF, 3, int)  /* 设置周期命令 */

int main(int argc, char *argv[])
{
    int fd, retvalue;
    char *filename;
    int ret = 0;
    unsigned int cmd;
    unsigned int arg;
    unsigned char str[100];

    if(argc != 2)
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

    /* 循环读取 */
    while(1)
    {
        printf("Input CMD:");
        ret = scanf("%d", &cmd);
        if(ret != 1)
        {
            gets(str);
        }

        if(cmd == 1)
        {
            ioctl(fd, CLOSE_CMD, &arg);
        }
        else if(cmd == 2)
        {
            ioctl(fd, OPEN_CMD, &arg);
        }
        else if(cmd == 3)
        {
            printf("input 定时器周期:");
            ret = scanf("%d", &arg);
            if(ret != 1)
            {
                gets(str);
            }
            ioctl(fd, SETPERIOD_CMD, &arg);
        }

    }

    close(fd);
    return 0;
}

