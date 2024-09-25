/*
    总结一些和Linux的互斥锁和条件变量有关的内容
    首先，明确最基本的一点：条件变量本身也是一种共享内容
    也就是说，这种东西原则上也会有属于传统的临界资源的那种同步问题
    只不过在操作系统层面确保了对锁这种变量的操作(获取/释放)本身都是"原子的"
    即一定可以保证一次操作可以在同一个系统时间片中执行完成而不会被打断
*/
#pragma once
#include <pthread.h>
#include "EventLoop.h"

// 定义子线程对应的结构体
struct WorkerThread
{
    pthread_t threadID;                     /* ID */
    char name[24];
    pthread_mutex_t mutex;                  /* 互斥锁 */
    pthread_cond_t cond;                    /* 条件变量 */
    struct EventLoop* evLoop;               /* 反应堆模型 */
};

// 初始化
int workerThreadInit(struct WorkerThread* thread, int index);
// 启动线程
void workerThreadRun(struct WorkerThread* thread);