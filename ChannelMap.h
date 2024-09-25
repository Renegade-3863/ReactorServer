#pragma once
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

struct ChannelMap
{
    int size;                               /* 记录指针指向的数组的元素总个数 */
    struct Channel** list;                  /* 存储了struct Channel* 类型的指针数组，等价于struct Channel* list[]*/
};

// 初始化
struct ChannelMap* channelMapInit(int size);
// 清空map
void ChannelMapClear(struct ChannelMap* map);
// 重新分配内存空间
bool makeMapRoom(struct ChannelMap* map, int newSize, int unitSize);
// 是不是缺了向ChannelMap中添加键值对的操作函数？