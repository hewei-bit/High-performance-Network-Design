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

//缓冲区长度
#define BUFFER_LENGTH 4096
// epoll中的事物数量
#define MAX_EPOLL_EVENT 1024
#define SERVER_PORT 8888

typedef int NCALLBACK(int, int, void *);

//事务结构体
struct ntyevent
{
    int fd;     //事务的fd
    int events; //事务类型
    void *arg;  //需要传给回调函数的参数,一般传的是reactor的指针

    int (*callback)(int fd, int events, void *arg); //对应的回调函数

    int status; // 0:新建 1：已存在
    char buffer[BUFFER_LENGTH];
    int length;
    long last_active;
};

// reator使用的结构体
struct ntyreactor
{
    int epfd;                // reatctor的fd
    struct ntyevent *events; // reactor管理的基础单元
};

int recv_cb(int fd, int events, void *arg);
int send_cb(int fd, int events, void *arg);
int accpet_cb(int fd, int events, void *arg);

void nty_event_set(struct ntyevent *ev, int fd, NCALLBACK callback, void *arg);
int nty_event_add(int epfd, int events, struct ntyevent *ev);
int nty_event_del(int epfd, struct ntyevent *ev);

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
    struct epoll_event ep_dv = {0, {0}};
    if (ev->status != 1)
    {
        return -1;
    }
    ep_dv.data.ptr = ev;
    ev->status = 0;
    epoll_ctl(epfd, EPOLL_CTL_DEL, ev->fd, &ep_dv);

    return 0;
}

//接收回调函数
int recv_cb(int fd, int events, void *arg)
{
    //外界传参获得的ntyreactor指针
    struct ntyreactor *reactor = (struct ntyreactor *)arg;
    struct ntyevent *ev = reactor->events + fd;

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
        //收到以后直接发送回去
        nty_event_set(ev, fd, send_cb, reactor);
        nty_event_add(reactor->epfd, EPOLLOUT, ev);
    }
    else if (len == 0)
    {
        close(ev->fd);
        printf("[fd=%d] pos[%ld], closed\n", fd, ev - reactor->events);
    }
    else
    {
        close(ev->fd);
        printf("recv[fd=%d] error[%d]:%s\n", fd, errno, strerror(errno));
    }
    return len;
}

int send_cb(int fd, int events, void *arg)
{
    struct ntyreactor *reactor = (struct ntyreactor *)arg;
    struct ntyevent *ev = reactor->events + fd;
    //返回的是发送的字节数
    int len = send(fd, ev->buffer, ev->length, 0);
    //正确发送,删除发送fd，注册接收的fd
    if (len > 0)
    {
        printf("send[fd=%d], [%d]%s\n", fd, len, ev->buffer);
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

int accpet_cb(int fd, int events, void *arg)
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
}
