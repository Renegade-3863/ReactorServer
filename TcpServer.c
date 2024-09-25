#include "TcpServer.h"
#include <arpa/inet.h>
#include "TcpConnection.h"

struct TcpServer* tcpServerInit(unsigned short port, int threadNum)
{
    struct TcpServer* tcp = (struct TcpServer*)malloc(sizeof(struct TcpServer));
    tcp->listener = listenerInit(port);
    tcp->mainLoop = eventLoopInit();
    tcp->threadNum = threadNum;
    tcp->threadPool = threadPoolInit(tcp->mainLoop, threadNum);
    return tcp;
}

struct Listener* listenerInit(unsigned short port)
{
    struct Listener* listener = (struct Listener*)malloc(sizeof(struct Listener));
    // 1. 创建监听的套接字
    // 创建一个使用IPv4协议，基于流式协议中的TCP协议的套接字(用来指定一块内存空间，这块内存空间用来存储对应的接收数据以及发送数据，以及一些其它控制信息)
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    // 如果创建失败(返回-1)，就打印(p)错误信息(error)
    if (lfd == -1)
    {
        perror("socket");
        return NULL;
    }
    // 2. 设置端口复用
    int opt = 1;
    // 设置监听套接字在套接字层面上的地址/端口可重用属性为opt(1，意即启动)，由于不同选项的状态信息可能不尽相同，这里还需要额外传入一个选项状态参数的长度信息
    int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret == -1)
    {
        perror("setsocketopt");
        return NULL;
    }
    // 3. 绑定
    // 给当前的监听文件描述符(lfd)绑定一个套接字内容信息的描述性结构体(addr)，由于使用的套接字内容结构体中包含可变字段用于适配不同的网络传输协议，这里需要传入一块地址+地址块长度用以明确划分内容块的边界
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    // 所有要在网络上进行传输的数据都必须统一成大端格式，由于大多数主机系统的数据存储格式都是小端，这里将主机识别出的小端数据块转换成大端格式以便后续握手中进行套接字信息交换
    addr.sin_port = htons(port);
    // 正常这里也需要进行IP地址的大小端转换，不过这里全0的地址大小端表示均相同，那么就不必消耗资源进行无意义的转换了
    // 不过我们尝试进行类型转换，因为要明确理解各个步骤的意义
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY 就是0x00000000(全0的32位整型数)
    // 实际进行套接字和文件描述符(地址空间)的绑定
    // 将lfd代表的文件地址绑定到addr套接字对应的内存信息控制块上
    ret = bind(lfd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == -1)
    {
        perror("bind");
        return NULL;
    }
    // 4. 设置监听
    // 规定一次最多可以与多少个客户端同时建立连接
    // 设置当前的lfd监听文件描述符最多可以同时接收128个客户端的连接请求(注意不是最多能连接128个，超出128个之外的连接请求会排队等候前面的连接建立完成)
    // 由于是TCP连接，这里超出128个的情况后续会重传连接请求，也有其它的底层协议会直接返回一个connection refused(连接被拒绝)消息
    ret = listen(lfd, 128);
    if (ret == -1)
    {
        perror("listen");
        return NULL;
    }
    listener->lfd = lfd;
    listener->port = port;
    // 返回fd
    return listener;
}

int acceptConnection(void* arg)
{
    struct TcpServer* server = (struct TcpServer*)arg;
    // 和客户端建立连接
    int cfd = accept(server->listener->lfd, NULL, NULL);
    // 从线程池中取出一个子线程的反应堆实例，取处理这个cfd
    struct EventLoop* evLoop = takeWorkerEventLoop(server->threadPool);
    // 将cfd放到 TcpConnection中处理
    tcpConnectionInit(cfd, evLoop);
    return 0;
}

void tcpServerRun(struct TcpServer* server)
{
    // 启动线程池
    threadPoolRun(server->threadPool);
    // 初始化一个channel实例
    struct Channel* channel = channelInit(server->listener->lfd, ReadEvent, acceptConnection, NULL, NULL, server);
    // 添加检测的任务
    eventLoopAddTask(server->mainLoop, channel, ADD);
    // 启动反应堆模型
    eventLoopRun(server->mainLoop);
}
