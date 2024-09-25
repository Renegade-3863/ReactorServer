#pragma once
#include "EventLoop.h"
#include "WorkerThread.h"
#include <stdbool.h>

// 定义线程池
struct ThreadPool
{
    // 主线程的反应堆模型
    struct EventLoop* mainLoop;         /* 如果线程池没有任何子线程，这个mainLoop才负责进行除了监听之外的其它连接处理 */
    bool isStart;
    int threadNum;
    struct WorkerThread* workerThreads;
    int index;
};

// 初始化线程池
struct ThreadPool* threadPoolInit(struct EventLoop* mainLoop, int count);
// 启动线程池
void threadPoolRun(struct ThreadPool* pool);
// 取出线程池中的某个子线程的反应堆实例
struct EventLoop* takeWorkerEventLoop(struct ThreadPool* pool);