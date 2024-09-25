#include "ChannelMap.h"
#include <string.h>

struct ChannelMap *channelMapInit(int size)
{
    struct ChannelMap* map = (struct ChannelMap*)malloc(sizeof(struct ChannelMap));
    map->size = size;
    map->list = (struct Channel**)malloc(size*sizeof(struct Channel*));
    return map;
}

void ChannelMapClear(struct ChannelMap* map)
{
    if(map)
    {
        for(int i = 0; i < map->size; ++i)
        {
            if(map->list[i])
            {
                free(map->list[i]);   
            }
        }
        free(map->list);
        map->list = NULL;
    }
    map->size = 0;
}

bool makeMapRoom(struct ChannelMap *map, int newSize, int unitSize)
{
    if(map->size < newSize)
    {
        int curSize = map->size;
        // 容量每次扩大原来的一倍
        while(curSize < newSize)
        {
            curSize *= 2;
        }
        // 扩容 realloc
        // realloc的返回地址有可能不再是原来分配的内存空间首地址，而是一块新的大小足够存储curSize * unitSize字节的内存空间首地址，
        // 故这里需要对map->list重新分配以确保获取到正确的内存空间
        struct Channel** temp = realloc(map->list, curSize * unitSize);
        if(!temp)
        {
            return false;
        }
        map->list = temp;
        map->size = curSize;
        // memset被调用后，内存才真正被进行了映射，分配器分配的虚拟地址才有了相应物理地址的映射
        memset(&map->list[map->size], 0, (curSize - map->size) * unitSize);
    }
    return true;
}
