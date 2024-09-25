#include "TcpConnection.h"
#include "HttpRequest.h"

int processRead(void* arg)
{
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    // 接收数据
    int count = bufferSocketRead(conn->readBuf, conn->channel->fd);
    if(count > 0)
    {
        // 接收到了 http 请求，解析http请求
        int socket = conn->channel->fd;
#ifdef MSG_SEND_AUTO
        writeEventEnable(conn->channel, true);
        eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
#endif
        bool flag = parseHttpRequest(conn->request, conn->readBuf, conn->response, conn->writeBuf, socket);
        if(!flag)
        {
            // 解析失败，回复一个简单的html
            char* errMsg = "Http/1.1 400 Bad Request\r\n\r\n";
            bufferAppendString(conn->writeBuf, errMsg);
        }
    }
#ifndef MSG_SEND_AUTO
    // 断开连接
    eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
#endif
    return 0;
}

int processWrite(void* arg)
{
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    // 发送数据
    int count = bufferSendData(conn->writeBuf, conn->channel->fd);
    if(count > 0)
    {
        // 判断数据是否被全部发送出去了
        if(bufferReadableSize(conn->writeBuf) == 0)
        {
            // 1. 不再检测写事件 -- 修改channel中保存的事件
            writeEventEnable(conn->channel, false);
            // 2. 修改dispatcher检测的集合 -- 添加任务节点
            eventLoopAddTask(conn->evLoop, conn->channel, MODIFY);
            // 3. 删除这个节点
            eventLoopAddTask(conn->evLoop, conn->channel, DELETE);
        }
    }
    return 0;
}

struct TcpConnection *tcpConnectionInit(int fd, struct EventLoop *evLoop)
{
    struct TcpConnection* conn = (struct TcpConnection*)malloc(sizeof(struct TcpConnection));
    conn->evLoop = evLoop;
    conn->readBuf = bufferInit(10240);
    conn->writeBuf = bufferInit(10240);
    // http
    conn->request = httpRequestInit();
    conn->response = httpResponseInit();
    sprintf(conn->name, "Connection-%d", fd);
    conn->channel = channelInit(fd, ReadEvent, processRead, processWrite, tcpConnectionDestroy, conn);
    eventLoopAddTask(evLoop, conn->channel, ADD);
    return conn;
}

int tcpConnectionDestroy(void *arg)
{
    struct TcpConnection* conn = (struct TcpConnection*)arg;
    if(conn != NULL)
    {
        if(conn->readBuf && bufferReadableSize(conn->readBuf) == 0 &&
           conn->writeBuf && bufferReadableSize(conn->writeBuf) == 0)
        {
            destroyChannel(conn->evLoop, conn->channel);
            bufferDestroy(conn->readBuf);
            bufferDestroy(conn->writeBuf);
            httpRequestDestroy(conn->request);
            httpResponseDestroy(conn->response);
            free(conn);
        }
    }
    return 0;
}
