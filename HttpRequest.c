#include "HttpRequest.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <ctype.h>
#include "TcpConnection.h"

#define HeaderSize 12
struct HttpRequest* httpRequestInit()
{
    struct HttpRequest* request = (struct HttpRequest*)malloc(sizeof(struct HttpRequest));
    request->curState = ParseReqLine;
    httpRequestReset(request);
    request->reqHeaders = (struct RequestHeader*)malloc(sizeof(struct RequestHeader) * HeaderSize);
    return request;
}

void httpRequestReset(struct HttpRequest *req)
{
    req->curState = ParseReqLine;
    req->method = NULL;
    req->url = NULL;
    req->version = NULL;
    req->reqHeadersNum = 0;
}

void httpRequestResetEx(struct HttpRequest *req)
{
    free(req->url);
    free(req->method);
    free(req->version);
    httpRequestReset(req);
    if(req->reqHeaders)
    {
        for(int i = 0; i < req->reqHeadersNum; ++i)
        {
            free(req->reqHeaders[i].key);
            free(req->reqHeaders[i].value);
        }
        free(req->reqHeaders);
    }
}

void httpRequestDestroy(struct HttpRequest *req)
{
    if(req)
    {
        httpRequestResetEx(req);
        free(req);
    }
}

enum HttpRequestState HttpRequestState(struct HttpRequest *request)
{
    return request->curState;
}

// 函数内部没有做内存申请，调用前要注意先申请内存再调用
void httpRequestAddHeader(struct HttpRequest *request, const char *key, const char *value)
{
    request->reqHeaders[request->reqHeadersNum].key = (char*)key;
    request->reqHeaders[request->reqHeadersNum].value = (char*)value;
    request->reqHeadersNum++;
}

char* httpRequestGetHeader(struct HttpRequest *request, const char *key)
{
    if(request)
    {
        for(int i = 0; i < request->reqHeadersNum; ++i)
        {
            if(strncasecmp(request->reqHeaders[i].key, key, strlen(key)) == 0)
            {
                return request->reqHeaders[i].value;
            }
        }
    }
    return NULL;
}

char* splitRequestLine(const char* start, const char* end, const char* sub, char** ptr)
{
    char* space = end;
    if(sub != NULL)
    {
        space = strstr(start, sub);
        assert(space != NULL);
    }
    int length = space - start;
    char* tmp = (char*)malloc(length+1);
    strncpy(tmp, start, length);
    tmp[length] = '\0';
    *ptr = tmp;
    return space+1;
}

bool parseHttpRequestLine(struct HttpRequest *request, struct Buffer *readBuf)
{   
    // 读出请求行,保存字符串结束地址
    char* end = bufferFindCRLF(readBuf);
    // 保存字符串起始地址
    char* start = readBuf->data + readBuf->readPos;
    // 请求行总长度
    int lineSize = end - start;

    if(lineSize)
    {
        start = splitRequestLine(start, end, " ", &request->method);
        start = splitRequestLine(start, end, " ", &request->url);
        splitRequestLine(start, end, NULL, &request->version);
#if 0
        // get /xxx/xx.txt http/1.1
        // 请求方式
        char* space = strstr(start, " ");
        assert(space != NULL);
        int methodSize = space - start;
        request->method = (char*)malloc(methodSize+1);
        strncpy(request->method, start, methodSize);
        request->method[methodSize] = '\0';

        // 请求静态资源
        start = space + 1;
        char* space = strstr(start, " ");
        assert(space != NULL);
        int urlSize = space - start;
        request->url = (char*)malloc(urlSize+1);
        strncpy(request->url, start, urlSize);
        request->method[urlSize] = '\0';

        // 请求静态资源
        start = space + 1;
        // char* space = strstr(start, " ");
        // assert(space != NULL);
        // int urlSize = space - start;
        request->version = (char*)malloc(end-start+1);
        strncpy(request->url, start, end-start);
        request->method[end-start] = '\0';
#endif

        // 为解析请求头做准备
        readBuf->readPos += lineSize;
        readBuf->readPos += 2;
        // 修改状态
        request->curState = ParseReqHeaders;
        return true;
    }
    return false;
}

// 该函数处理请求头中的一行
bool parseHttpRequestHeader(struct HttpRequest *request, struct Buffer *readBuf)
{
    char* end = bufferFindCRLF(readBuf);
    if(end != NULL)
    {
        char* start = readBuf->data + readBuf->readPos;
        int lineSize = end - start;
        // 基于：搜索字符串
        // 如果不能确定通信使用的是标准的HTTP协议，就把搜索字符串的needle指定为":"(不带额外空格)
        char* middle = strstr(start, ": ");
        if(middle != NULL)
        {
            char* key = (char*)malloc(middle-start+1);
            strncpy(key, start, middle-start);
            key[middle-start] = '\0';

            char* value = (char*)malloc(end-middle-2+1);
            strncpy(key, middle+2, end-middle-2);
            key[end-middle-2] = '\0';

            httpRequestAddHeader(request, key, value);
            // 移动读数据的位置
            readBuf->readPos += lineSize;
            readBuf->readPos += 2;
        }
        else
        {
            // 请求头被解析完了，跳过空行
            readBuf->readPos += 2;
            // 修改解析状态
            // 忽略 post 请求，按照 get 请求处理
            request->curState = ParseReqDone;
        }
        return true;
    }
    return false;
}

bool parseHttpRequest(struct HttpRequest *request, struct Buffer *readBuf, 
    struct HttpResponse* response, struct Buffer* sendBuf, int socket)
{
    bool flag = true;
    while(request->curState != ParseReqDone)
    {
        switch (request->curState)
        {
        case ParseReqLine:
            flag = parseHttpRequestLine(request, readBuf);
            break;
        case ParseReqHeaders:
            flag = parseHttpRequestHeader(request, readBuf);
            break;
        case ParseReqBody:
            break;
        default:
            break;
        }
        if(!flag)
        {
            return flag;
        }
        // 判断是否解析完毕了，如果完毕了，需要准备回复的数据
        if(request->curState == ParseReqDone)
        {
            // 1. 根据解析出的原始数据，队客户端的请求做出处理
            processHttpRequest(request, response);
            // 2. 组织相应数据并发送给客户端
            httpResponsePrepareMsg(response, sendBuf, socket);
        }
    }
    request->curState = ParseReqLine;       // 状态还原，保证还能继续处理第二条及以后的请求
    return false;
}

// 处理基于get的http请求
bool processHttpRequest(struct HttpRequest *request, struct HttpResponse* response)
{
    // 解析请求行
    // 目前的逻辑只处理GET请求，不考虑POST请求，因此不需考虑第三(空行)和第四部分(客户端发给服务器端的数据块)
    // 两种方案：
    // 1. 直接遍历line，检查空格进行分段解析
    // 这里不做实现
    // 2. 使用sscanf对格式化的字符串进行拆分
    if (strcasecmp(request->method, "get") != 0)
    {
        return -1;
    }
    decodeMsg(request->url, request->url);
    // 处理客户端请求的静态资源(目录或者文件)
    char *file = NULL;
    // 如果要访问的目录就是服务器的资源根目录，file指定为./即可把绝对路径转换成相对路径
    if (strcmp(request->url, "/") == 0)
    {
        file = "./";
    }
    // 如果不是根目录，直接把path指针右移一位，去掉开头的/即可
    // 注意对于相对路径来说
    // ./manuals 和 manuals是完全等价的
    else
    {
        file = request->url + 1;
    }

    // 需要判断file对应的路径位置是一个文件还是文件目录
    // 获取文件属性
    struct stat st;
    int ret = stat(file, &st);
    // printf("The fileName requested is: %s\n", file);
    // 如果访问的文件/目录不存在
    if (ret == -1)
    {
        // 回复404;
        // 文件不存在 -- 回复404
        // sendHeadMsg(cfd, 404, "Not Found", getFileType(".html"), -1);
        // 注意，在服务器的服务目录下必须事先准备一份404.html文件
        // sendFile("404.html", cfd);
        strcpy(response->fileName, "404.html");
        response->statusCode = NotFound;
        strcpy(response->statusMsg, "Not Found");
        // 响应头
        httpResponseAddHeader(response, "Content-type", getFileType(".html"));
        response->sendDataFunc = sendFile;
        return 0;
    }
    strcpy(response->fileName, file);
    response->statusCode = OK;
    strcpy(response->statusMsg, "OK");
    // 判断文件类型
    if (S_ISDIR(st.st_mode))
    {
        // 把本地目录中的内容发送给客户端
        // sendHeadMsg(cfd, 200, "OK", getFileType(".html"), -1);
        // sendDir(file, cfd);
        // 响应头
        httpResponseAddHeader(response, "Content-type", getFileType(".html"));
        response->sendDataFunc = sendDir;
    }
    else
    {
        // 把文件的内容发送给客户端
        // sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
        // 注意，在服务器的服务目录下必须事先准备一份404.html文件
        // sendFile(file, cfd);
        // 响应头
        char tmp[12] = { 0 };
        sprintf(tmp, "%ld", st.st_size);
        httpResponseAddHeader(response, "Content-type", getFileType(file));
        httpResponseAddHeader(response, "Content-length", tmp);
        response->sendDataFunc = sendFile;
    }
    return false;
}

// 解码
// to 存储解码之后的数据，传出参数，from被解码的数据，传入参数
void decodeMsg(char* to, char* from)
{
    for(; *from != '\0'; ++to, ++from)
    {
        if(from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
        {
            // 将16进制的数 -> 10进制的数 将这个数值赋值给了字符 int -> char
            // B2 -> 178 (11 * 16 + 2)
            // 将三个字符变成了一个字符，这个字符就是原始数据
            *to = hexToDec(from[1]) * 16 + hexToDec(from[2]);

            // 跳过from[1] 和 from[2] 因为在当前循环中已经处理过了
            from += 2;
        }
        else 
        {
            // 字符拷贝，赋值
            *to = *from;
        }
    }
    *to = '\0';
}

const char *getFileType(const char *name)
{
    // a.jpg a.mp4 a.html
    // 自右向左查找 '.' 字符，如不存在则返回NULL
    const char *dot = strrchr(name, '.');
    if (dot == NULL)
        return "text/plain; charset=utf-8"; // 纯文本
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp(dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}

void sendDir(const char *dirName, struct Buffer* sendBuf, int cfd)
{
    // 先把html格式文件的头部拼出来放到buf中
    char buf[4096] = {0};
    sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirName);

    // 打开DIR句柄指针
    DIR *dir = opendir(dirName);
    if (dir == NULL)
    {
        perror("opendir");
    }

    // 遍历当前目录中的文件
    struct dirent *ptr = NULL;
    while ((ptr = readdir(dir)) != NULL)
    {
        // 否则读到了一个文件
        // 先进行绝对路径的拼接
        struct stat st;
        char subPath[1024] = {0};
        sprintf(subPath, "%s/%s", dirName, ptr->d_name);
        stat(subPath, &st);
        // 判断文件的类型
        // 如果是目录，那么就要递归地进行发送
        if (S_ISDIR(st.st_mode))
        {
            // a标签 <a href="">name</a>
            sprintf(buf + strlen(buf), "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>", ptr->d_name, ptr->d_name, st.st_size);
            // sendDir(subPath, cfd);
        }
        // 否则，说明是常规文件
        else
        {
            sprintf(buf + strlen(buf), "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>", ptr->d_name, ptr->d_name, st.st_size);
        }
        // send(cfd, buf, strlen(buf), 0);
        bufferAppendString(sendBuf, buf);
#ifndef MSG_SEND_AUTO
        bufferSendData(sendBuf, cfd);
#endif
        memset(buf, 0, sizeof(buf));
    }
    sprintf(buf, "</table></body></html>");
    // send(cfd, buf, strlen(buf), 0);
    bufferAppendString(sendBuf, buf);
#ifndef MSG_SEND_AUTO
    bufferSendData(sendBuf, cfd);
#endif
    memset(buf, 0, sizeof(buf));
    return 0;
}

void sendFile(const char *fileName, struct Buffer* sendBuf, int cfd)
{
    // 1. 打开文件
    printf("file Name: %s\n", fileName);
    int fd = open(fileName, O_RDONLY);
    // 使用断言进行结果判定，如果打开的描述符为非正数，就直接中断进程
    assert(fd > 0);
#if 1
    while(1)
    {
        char buf[1024];
        int len = read(fd, buf, sizeof buf);
        if(len > 0)
        {
            // send(cfd, buf, len, 0);
            bufferAppendData(sendBuf, buf, len);
#ifndef MSG_SEND_AUTO
            bufferSendData(sendBuf, cfd);
#endif
            // 给接收端一定的时间进行接收数据的处理(例如浏览器的页面渲染等操作)
            usleep(10);
        }
        else if(len == 0)
        {
            break;
        }
        else
        {
            close(fd);
            perror("read");
        }
    }
#else
    // 可以使用stat函数进行文件大小的计算
    struct stat statbuf;
    stat(fileName, &statbuf);
    sendfile(cfd, fd, NULL, statbuf.st_size);
    // 或者使用lseek函数获取文件对应的偏移量
    // int size = lseek(fd, 0, SEEK_END);
    // sendfile(cfd, fd,. NULL, size);
#endif
    close(fd);
}