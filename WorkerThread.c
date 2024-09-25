#include "WorkerThread.h"

int workerThreadInit(struct WorkerThread *thread, int index)
{
    thread->evLoop = NULL;
    thread->threadID = 0;
    sprintf(thread->name, "SubThread-%d", index);
    pthread_mutex_init(&thread->mutex, NULL);
    pthread_cond_init(&thread->cond, NULL);
    return 0;
}

// 子线程的回调函数
void* subThreaedRunning(void* arg)
{
    struct WorkerThread* thread = (struct WorkerThread*)arg;
    pthread_mutex_lock(&thread->mutex);
    thread->evLoop = eventLoopInitEx(thread->name);
    pthread_cond_signal(&thread->cond); 
    pthread_mutex_unlock(&thread->mutex);
    eventLoopRun(thread->evLoop);
    return NULL;
}

void workerThreadRun(struct WorkerThread *thread)
{
    // 创建子线程
    pthread_create(&thread->threadID, NULL, subThreaedRunning, thread);
    // 阻塞主线程，让当前线程不会结束
    pthread_mutex_lock(&thread->mutex);
    while(thread->evLoop == NULL)
    {
        pthread_cond_wait(&thread->cond, &thread->mutex);
    } 
    pthread_mutex_unlock(&thread->mutex);
}
