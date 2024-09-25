#include "Dispatcher.h"
#include "EventLoop.h"
#include <poll.h>

#define Max 1024
struct pollData 
{
    int maxfd;
    struct pollfd fds[Max];
};

static void* pollInit();
static int pollAdd(struct Channel* channel, struct EventLoop* evLoop);
static int pollRemove(struct Channel* channel, struct EventLoop* evLoop);
static int pollModify(struct Channel* channel, struct EventLoop* evLoop);
static int pollDispatch(struct EventLoop* evLoop, int timeout);            // 单位：s
static int pollClear(struct EventLoop* evLoop);

struct Dispatcher PollDispatcher = {
    pollInit,
    pollAdd,
    pollRemove,
    pollModify,
    pollDispatch,
    pollClear
};

static void *pollInit()
{
    struct pollData* data = (struct pollData*)malloc(sizeof(struct pollData));
    data->maxfd = 0;
    for(int i = 0; i < Max; ++i)
    {
        data->fds[i].fd = -1;
        data->fds[i].events = 0;
        data->fds[i].revents = 0;
    }
    return data;
}

static int pollAdd(struct Channel *channel, struct EventLoop *evLoop)
{
    struct pollData* data = (struct pollData*)evLoop->dispatcherData;
    int events = 0;
    if(channel->events & ReadEvent)
    {
        events |= POLLIN; 
    }
    if(channel->events & WriteEvent)
    {
        events |= POLLOUT;
    }
    int i = 0;
    for(; i < Max; ++i)
    {
        if(data->fds[i].fd == -1)
        {
            data->fds[i].events = events;
            data->fds[i].fd = channel->fd;
            data->maxfd = i > data->maxfd ? i : data->maxfd;
            break;
        }
    }
    if(i >= Max)
    {
        return -1;
    }
    return 0;
}

int pollRemove(struct Channel *channel, struct EventLoop *evLoop)
{
    struct pollData* data = (struct pollData*)evLoop->dispatcherData;
    int i = 0;
    for(; i < Max; ++i)
    {
        if(data->fds[i].fd == channel->fd)
        {
            data->fds[i].events = 0;
            data->fds[i].revents = 0;
            data->fds[i].fd = -1;
            break;
        }
    }
    // 通过 channel 释放对应的 TcpConnection 资源
    channel->destroyCallback(channel->arg);
    if(i >= Max)
    {
        return -1;
    }
    return 0;
}

int pollModify(struct Channel *channel, struct EventLoop *evLoop)
{
    struct pollData* data = (struct pollData*)evLoop->dispatcherData;
    int events = 0;
    if(channel->events & ReadEvent)
    {
        events |= POLLIN; 
    }
    if(channel->events & WriteEvent)
    {
        events |= POLLOUT;
    }
    int i = 0;
    for(; i < Max; ++i)
    {
        if(data->fds[i].fd == channel->fd)
        {
            data->fds[i].events = events;
            break;
        }
    }
    return 0;
}

int pollDispatch(struct EventLoop *evLoop, int timeout)
{
    struct pollData* data = (struct pollData*)evLoop->dispatcherData;
    int count = poll(data->fds, data->maxfd+1, timeout * 1000);
    if(count == -1)
    {
        perror("poll");
        exit(0);
    }
    for(int i = 0; i <= data->maxfd; ++i)
    {
        if(data->fds[i].fd == -1)
        {
            continue;
        }
        if(data->fds[i].revents & POLLIN)
        {
            eventActivate(evLoop, data->fds[i].fd, ReadEvent);
        }
        if(data->fds[i].revents & POLLOUT)
        {
            eventActivate(evLoop, data->fds[i].fd, WriteEvent);
        }
    }
    return 0;
}

int pollClear(struct EventLoop *evLoop)
{
    struct pollData* data = (struct pollData*)evLoop->dispatcherData;
    free(data);
    return 0;
}
