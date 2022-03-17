/**
 * @File Name: webserver_hw.c
 * @Brief : http服务器实现
 * @Author : hewei (hewei_1996@qq.com)
 * @Version : 1.0
 * @Creat Date : 2022-03-16
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/sendfile.h>

#define BUFFER_LENGTH 4096    //缓冲区长度
#define MAX_EPOLL_EVENTS 1024 // epoll中的事物数量
#define SERVER_PORT 8888      //默认端口号
#define PORT_COUNT 100        //连接客户端数量

#define HTTP_WEBSERVER_HTML_ROOT "html" //文件夹名

#define HTTP_METHOD_GET 0
#define HTTP_METHOD_POST 1

typedef int NCALLBACK(int, int, void *);

//事务结构体
struct ntyevent
{
    int fd;                                         //事务的fd
    int events;                                     // epoll events类型
    void *arg;                                      //需要传给回调函数的参数,一般传的是reactor的指针
    int (*callback)(int fd, int events, void *arg); //对应的回调函数
    int status;                                     // 0:新建 1：已存在
    char buffer[BUFFER_LENGTH];                     //接收到的消息
    int length;
    long last_active;

    // http 参数
    int method;
    char resource[BUFFER_LENGTH];
    int ret_code;
};

struct eventblock
{
    struct eventblock *next; //指向下一个block
    struct ntyevent *events; //当前block对应的event数组
};

// reator使用的结构体
struct ntyreactor
{
    int epfd; // reatctor的fd
    int blkcnt;
    struct eventblock *evblk; // reactor管理的基础单元，现在是block，实现C1000K
};

int recv_cb(int fd, int events, void *arg);
int send_cb(int fd, int events, void *arg);
int accept_cb(int fd, int events, void *arg);

void nty_event_set(struct ntyevent *ev, int fd, NCALLBACK callback, void *arg);
int nty_event_add(int epfd, int events, struct ntyevent *ev);
int nty_event_del(int epfd, struct ntyevent *ev);
struct ntyevent *ntyreactor_idx(struct ntyreactor *reactor, int sockfd);

//设置ntyevent的参数
void nty_event_set(struct ntyevent *ev, int fd, NCALLBACK callback, void *arg)
{
    ev->fd = fd;
    ev->callback = callback;
    ev->events = 0;
    ev->arg = arg;
    ev->last_active = time(NULL);

    return;
}

//增加或修改
int nty_event_add(int epfd, int events, struct ntyevent *ev)
{
    //使用的是linux内核里的epoll
    struct epoll_event ep_ev = {0, {0}};
    //这一步非常关键传到联合体data的ptr里的是ntyevent指针
    ep_ev.data.ptr = ev;
    ep_ev.events = ev->events = events;
    //判断该事件是否已经添加过
    int op;
    if (ev->status == 1)
    {
        op = EPOLL_CTL_MOD;
    }
    else
    {
        op = EPOLL_CTL_ADD;
        ev->status = 1;
    }
    // epoll_ctl进行对应操作
    if (epoll_ctl(epfd, op, ev->fd, &ep_ev) < 0)
    {
        printf("event add failed [fd=%d], events[%d]\n", ev->fd, events);
        return -1;
    }
    return 0;
}

int nty_event_del(int epfd, struct ntyevent *ev)
{
    struct epoll_event ep_ev = {0, {0}};
    if (ev->status != 1)
    {
        return -1;
    }
    ep_ev.data.ptr = ev;
    ev->status = 0;
    epoll_ctl(epfd, EPOLL_CTL_DEL, ev->fd, &ep_ev);

    return 0;
}

//读一行http请求,将有用的信息放进linebuf
int readline(char *allbuf, int idx, char *linebuf)
{
    int len = strlen(allbuf);

    for (; idx < len; idx++)
    {
        //遇到\r\n
        if (allbuf[idx] == '\r' && allbuf[idx + 1] == '\n')
        {
            return idx + 2;
        }
        else
        {
            *(linebuf++) = allbuf[idx];
        }
    }

    return -1;
}

//服务器接收请求
int http_request(struct ntyevent *ev)
{
    // GET,POST
    char linebuf[1024] = {0};
    int idx = readline(ev->buffer, 0, linebuf);

    if (strstr(linebuf, "GET"))
    {
        ev->method = HTTP_METHOD_GET;

        //获取GET前面和HTTP之前的资源位，并塞进linebuf里面
        int i = 0;
        while (linebuf[sizeof("GET ") + i] != ' ')
        {
            i++;
        }
        linebuf[sizeof("GET ") + i] = '\0';
        //将请求的资源路径放进去
        sprintf(ev->resource, "./%s/%s", HTTP_WEBSERVER_HTML_ROOT, linebuf + sizeof("GET "));
    }
    else if (strstr(linebuf, "POST"))
    {
    }
}

//接收回调函数
int recv_cb(int fd, int events, void *arg)
{
    //外界传参获得的ntyreactor指针
    struct ntyreactor *reactor = (struct ntyreactor *)arg;
    //从内核的链表中取出当前的event
    struct ntyevent *ev = ntyreactor_idx(reactor, fd);
    //此次收到数据的长度
    int len = recv(fd, ev->buffer, BUFFER_LENGTH, 0);
    //收到就直接删除对应的fd，以免多次响应
    nty_event_del(reactor->epfd, ev);
    //正确接收，大于0
    if (len > 0)
    {
        ev->length = len;
        ev->buffer[len] = '\0';

        printf("C[%d]:%s\n", fd, ev->buffer);

        //接收http请求
        http_request(ev);

        //收到以后直接发送回去
        nty_event_set(ev, fd, send_cb, reactor);
        nty_event_add(reactor->epfd, EPOLLOUT, ev);
    }
    else if (len == 0)
    {
        close(ev->fd);
        // printf("[fd=%d] pos[%ld], closed\n", fd, ev - reactor->events);
    }
    else
    {
        close(ev->fd);
        printf("recv[fd=%d] error[%d]:%s\n", fd, errno, strerror(errno));
    }
    return len;
}

// http响应
int http_response(struct ntyevent *ev)
{
    if (ev == NULL)
        return -1;
    memset(ev->buffer, 0, BUFFER_LENGTH);

#if 0
	const char *html = "<html><head><title>hello http</title></head><body><H1>King</H1></body></html>\r\n\r\n";
						  	   
	ev->length = sprintf(ev->buffer, 
		"HTTP/1.1 200 OK\r\n\
		 Date: Thu, 11 Nov 2021 12:28:52 GMT\r\n\
		 Content-Type: text/html;charset=ISO-8859-1\r\n\
		 Content-Length: 83\r\n\r\n%s", 
		 html);

#else
    printf("resource: %s\n", ev->resource);
    int filefd = open(ev->resource, O_RDONLY);
    //如果找不到
    if (filefd == -1)
    {
        ev->ret_code = 404;
        ev->length = sprintf(ev->buffer,
                             "HTTP/1.1 404 Not Found\r\n"
                             "Date: Thu, 11 Nov 2021 12:28:52 GMT\r\n"
                             "Content-Type: text/html;charset=ISO-8859-1\r\n"
                             "Content-Length: 85\r\n\r\n"
                             "<html><head><title>404 Not Found</title></head><body><H1>404</H1></body></html>\r\n\r\n");
    }
    else
    {
        //获取文件状态
        struct stat stat_buf;
        fstat(filefd, &stat_buf);
        close(filefd);
        //文件处于保护状态
        if (S_ISDIR(stat_buf.st_mode))
        {
            ev->ret_code = 404;
            ev->length = sprintf(ev->buffer,
                                 "HTTP/1.1 404 Not Found\r\n"
                                 "Date: Thu, 11 Nov 2021 12:28:52 GMT\r\n"
                                 "Content-Type: text/html;charset=ISO-8859-1\r\n"
                                 "Content-Length: 85\r\n\r\n"
                                 "<html><head><title>404 Not Found</title></head><body><H1>404</H1></body></html>\r\n\r\n");
        }
        else if (S_ISREG(stat_buf.st_mode))
        {
            ev->ret_code = 200;
            ev->length = sprintf(ev->buffer,
                                 "HTTP/1.1 200 OK\r\n"
                                 "Date: Thu, 11 Nov 2021 12:28:52 GMT\r\n"
                                 "Content-Type: text/html;charset=ISO-8859-1\r\n"
                                 "Content-Length: %ld\r\n\r\n",
                                 stat_buf.st_size);
        }
    }
#endif
    return ev->length;
}

int send_cb(int fd, int events, void *arg)
{
    struct ntyreactor *reactor = (struct ntyreactor *)arg;
    struct ntyevent *ev = ntyreactor_idx(reactor, fd);

    http_response(ev);

    //返回的是发送的字节数
    int len = send(fd, ev->buffer, ev->length, 0);
    //正确发送,删除发送fd，注册接收的fd
    if (len > 0)
    {
        printf("send[fd=%d], [%d]%s\n", fd, len, ev->buffer);

        if (ev->ret_code == 200)
        {
            int filefd = open(ev->resource, O_RDONLY);
            struct stat stat_buf;
            fstat(filefd, &stat_buf);
            //通过内存映射实现，零拷贝
            sendfile(fd, filefd, NULL, stat_buf.st_size);
            close(filefd);
        }

        nty_event_del(reactor->epfd, ev);
        nty_event_set(ev, fd, recv_cb, reactor);
        nty_event_add(reactor->epfd, EPOLLIN, ev);
    }
    else
    {
        close(ev->fd);
        nty_event_del(reactor->epfd, ev);
        printf("send[fd=%d] error %s\n", fd, strerror(errno));
    }
    return len;
}

int accept_cb(int fd, int events, void *arg)
{
    struct ntyreactor *reactor = (struct ntyreactor *)arg;
    if (reactor == NULL)
    {
        return -1;
    }
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);

    int clientfd;

    if ((clientfd = accept(fd, (struct sockaddr *)&client_addr, &len)) == -1)
    {
        if (errno != EAGAIN && errno != EINTR)
        {
            printf("accept: %s\n", strerror(errno));
            return -1;
        }
    }
    int flag = 0;
    if ((flag = fcntl(clientfd, F_SETFL, O_NONBLOCK)) < 0)
    {
        printf("%s: fcntl nonblocking failed, %d\n", __func__, MAX_EPOLL_EVENTS);
        return -1;
    }

    struct ntyevent *event = ntyreactor_idx(reactor, clientfd);

    nty_event_set(event, clientfd, recv_cb, reactor);
    nty_event_add(reactor->epfd, EPOLLIN, event);
    printf("new connect [%s:%d], pos[%d]\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), clientfd);

    return 0;
}

int init_sock(short port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(fd, F_SETFL, O_NONBLOCK);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (listen(fd, 20) < 0)
    {
        printf("listen failed : %s\n", strerror(errno));
    }

    return fd;
}

//申请一个block的空间
int ntyreactor_alloc(struct ntyreactor *reactor)
{
    //空检查
    if (reactor == NULL)
        return -1;
    if (reactor->evblk == NULL)
        return -1;

    //查找当前block的末尾
    struct eventblock *blk = reactor->evblk;
    while (blk->next != NULL)
    {
        blk = blk->next;
    }

    //分配内存
    //分配event
    struct ntyevent *evs = (struct ntyevent *)malloc((MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));
    if (evs == NULL)
    {
        printf("ntyreactor_alloc ntyevents failed\n");
        return -2;
    }
    memset(evs, 0, (MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));
    //分配block本身
    struct eventblock *block = (struct eventblock *)malloc(sizeof(struct eventblock));
    if (block == NULL)
    {
        printf("ntyreactor_alloc eventblock failed\n");
        return -2;
    }
    memset(block, 0, sizeof(struct eventblock));
    //加入链表末尾
    block->events = evs;
    block->next = NULL;

    blk->next = block;
    reactor->blkcnt++;
    return 0;
}

//通过fd找到与之对应的events
struct ntyevent *ntyreactor_idx(struct ntyreactor *reactor, int sockfd)
{
    //获取当前fd对应events所在的blk
    int blkidx = sockfd / MAX_EPOLL_EVENTS;

    while (blkidx >= reactor->blkcnt)
    {
        ntyreactor_alloc(reactor);
    }

    int i = 0;
    struct eventblock *blk = reactor->evblk;
    while (i++ < blkidx && blk != NULL)
    {
        blk = blk->next;
    }

    return &blk->events[sockfd % MAX_EPOLL_EVENTS];
}

// reactor初始化
int ntyreactor_init(struct ntyreactor *reactor)
{
    if (reactor == NULL)
    {
        return -1;
    }
    memset(reactor, 0, sizeof(struct ntyreactor));

    //创建大房子
    reactor->epfd = epoll_create(1);
    if (reactor->epfd <= 0)
    {
        printf("create epfd in %s err %s\n", __func__, strerror(errno));
        return -2;
    }

    //申请events的空间
    struct ntyevent *evs = (struct ntyevent *)malloc((MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));
    if (evs == NULL)
    {
        printf("ntyreactor_alloc ntyevents failed\n");
        return -2;
    }
    memset(evs, 0, (MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));

    struct eventblock *block = (struct eventblock *)malloc(sizeof(struct eventblock));
    if (block == NULL)
    {
        printf("ntyreactor_alloc eventblock failed\n");
        return -2;
    }
    memset(block, 0, sizeof(struct eventblock));

    block->events = evs;
    block->next = NULL;

    reactor->evblk = block;
    reactor->blkcnt = 1;

    return 0;
}

int ntyreactor_destroy(struct ntyreactor *reactor)
{
    close(reactor->epfd);
    //释放调之前malloc的所有资源
    struct eventblock *blk = reactor->evblk;
    struct eventblock *blk_next = NULL;

    while (blk != NULL)
    {

        blk_next = blk->next;

        free(blk->events);
        free(blk);

        blk = blk_next;
    }

    return 0;
}

//添加最早的回调函数
int ntyreactor_addlistener(struct ntyreactor *reactor, int sockfd, NCALLBACK *acceptor)
{
    if (reactor == NULL)
    {
        return -1;
    }
    if (reactor->evblk == NULL)
    {
        return -1;
    }
    // reactor->evblk->events[sockfd];
    struct ntyevent *event = ntyreactor_idx(reactor, sockfd);
    nty_event_set(event, sockfd, acceptor, reactor);
    nty_event_add(reactor->epfd, EPOLLIN, event);
    return 0;
}

int ntyreactor_run(struct ntyreactor *reactor)
{
    if (reactor == NULL)
        return -1;
    if (reactor->epfd < 0)
        return -1;
    if (reactor->evblk == NULL)
        return -1;

    struct epoll_event events[MAX_EPOLL_EVENTS + 1];
    int checkpos = 0, i;

    while (1)
    {
        /*
        // 100个为单位的进行检查是否超时
        long now = time(NULL);
        for (int i = 0; i < 100; i++, checkpos++)
        {
            //满了就清空
            if (checkpos == MAX_EPOLL_EVENT)
            {
                checkpos = 0;
            }
            //未创建跳过
            if (reactor->events[checkpos].status != 1)
            {
                continue;
            }
            //检测连接时间太长了就断掉
            long duration = now - reactor->events[checkpos].last_active;

            if (duration >= 60)
            {
                close(reactor->events[checkpos].fd);
                printf("[fd=%d] timeout\n", reactor->events[checkpos].fd);
                nty_event_del(reactor->epfd, &reactor->events[checkpos]);
            }
        }
*/
        //从内核中把事务提取出来放到events里面
        int nready = epoll_wait(reactor->epfd, events, MAX_EPOLL_EVENTS, 1000);
        if (nready < 0)
        {
            printf("epoll_wait error, exit\n");
            continue;
        }

        //最最最关键的地方实现回调函数
        for (i = 0; i < nready; i++)
        {
            //这里data是一个union传过来的就只有之前放进去的指向ntyevent的指针
            struct ntyevent *ev = (struct ntyevent *)events[i].data.ptr;

            if ((events[i].events & EPOLLIN) && (ev->events & EPOLLIN))
            {
                ev->callback(ev->fd, events[i].events, ev->arg);
            }
            if ((events[i].events & EPOLLOUT) && (ev->events & EPOLLOUT))
            {
                ev->callback(ev->fd, events[i].events, ev->arg);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    //默认端口后为全局变量
    unsigned short port = SERVER_PORT;
    //如果传了参就按传的参数来
    if (argc == 2)
    {
        port = atoi(argv[1]);
    }
    struct ntyreactor *reactor = (struct ntyreactor *)malloc(sizeof(struct ntyreactor));
    ntyreactor_init(reactor);

    int i = 0;
    int sockfds[PORT_COUNT] = {0};
    for (i = 0; i < PORT_COUNT; i++)
    {
        sockfds[i] = init_sock(port + i);
        ntyreactor_addlistener(reactor, sockfds[i], accept_cb);
    }

    ntyreactor_run(reactor);

    ntyreactor_destroy(reactor);
    for (i = 0; i < PORT_COUNT; i++)
    {
        close(sockfds[i]);
    }
    free(reactor);
    return 0;
}
