#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// ./keyAPP <filename>  <0:1> 
// ./keyAPP  /dev/key  

#define KEY0VALUE   0xf0
#define INVAKEY     0


int main(int argc, char *argv[])
{
    int fd, retvalue;
    char *filename;
    int value;

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
        read(fd, &value, sizeof(value));

        if(value == KEY0VALUE)
        {
            printf("KRY0 Press, value = %d \r\n", value);
        }

    }

    close(fd);
    return 0;
}

