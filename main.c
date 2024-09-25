#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
    if(argc < 3)
    {
        printf("./a.out port path\n");
        return -1;
    }
    // 用户传入main函数的参数列表中的参数都是c字符串类型，需要转换成对应的整型(port为16位，可转成short类型)
    unsigned short port = atoi(argv[1]);
    // 切换服务器的工作路径
    chdir(argv[2]);

    return 0;
}