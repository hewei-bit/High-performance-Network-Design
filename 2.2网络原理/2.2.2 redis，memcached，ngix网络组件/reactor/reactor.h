
#ifndef _MARK_REACTOR_
#define _MARK_REACTOR_

#include <unistd.h>     // read write
#include <fcntl.h>      // fcntl
#include <sys/types.h>  // listen
#include <sys/socket.h> // socket
#include <errno.h>      // errno
#include <arpa/inet.h>  // inet_addr htons
// #include <netinet/tcp.h>
#include <assert.h> // assert

#include <sys/epoll.h>

#include <stdlib.h> // malloc

#include <string.h> // memcpy memmove

#include "rbt-timer.h"
#include "ringbuf.h"

#define MAX_EVENT_NUM 512
#define MAX_BUFF_SIZE 16 * 1024
#define MAX_CONN ((1 << 16) - 1)

typedef struct event_s event_t;
typedef void (*event_callback_fn)(int fd, int events, void *privdata);
typedef void (*error_callback_fn)(int fd, char *err);

typedef struct
{
    int epfd;
    int listenfd;
    int stop;
    event_t *events;
    int iter;
    struct epoll_event fire[MAX_EVENT_NUM];
} reactor_t;

struct event_s
{
    int fd;
    reactor_t *r;
    ringbuffer_t in;
    ringbuffer_t out;
    event_callback_fn read_fn; // clientfd   read decode compute encode send 业务逻辑的割裂
    event_callback_fn write_fn;
    error_callback_fn error_fn;
};

reactor_t *create_reactor()
{
    reactor_t *r = (reactor_t *)malloc(sizeof(*r));
    r->epfd = epoll_create(1);
    r->listenfd = 0;
    r->stop = 0;
    r->iter = 0;
    r->events = (event_t *)malloc(sizeof(event_t) * MAX_CONN);
    memset(r->events, 0, sizeof(event_t) * MAX_CONN);
    memset(r->fire, 0, sizeof(struct epoll_event) * MAX_EVENT_NUM);
    return r;
}

void release_reactor(reactor_t *r)
{
    free(r->events);
    close(r->epfd);
    free(r);
}

event_t *_get_event_t(reactor_t *r)
{
    r->iter++;
    while (r->events[r->iter & MAX_CONN].fd > 0)
    {
        r->iter++;
    }
    return &r->events[r->iter];
}

event_t *new_event(reactor_t *R, int fd,
                   event_callback_fn rd,
                   event_callback_fn wt,
                   error_callback_fn err)
{
    assert(rd != 0 || wt != 0 || err != 0);
    event_t *e = _get_event_t(R);
    e->r = R;
    e->fd = fd;
    rb_init(&e->in, 1024 * 16);
    rb_init(&e->out, 1024 * 16);
    e->read_fn = rd;
    e->write_fn = wt;
    e->error_fn = err;
    return e;
}

void free_event(event_t *e)
{
    e->fd = 0;
    rb_free(&e->in);
    rb_free(&e->out);
}

int set_nonblock(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

int add_event(reactor_t *R, int events, event_t *e)
{
    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = e;
    if (epoll_ctl(R->epfd, EPOLL_CTL_ADD, e->fd, &ev) == -1)
    {
        printf("add event err fd = %d\n", e->fd);
        return 1;
    }
    return 0;
}

int del_event(reactor_t *R, event_t *e)
{
    epoll_ctl(R->epfd, EPOLL_CTL_DEL, e->fd, NULL);
    free_event(e);
    return 0;
}

int enable_event(reactor_t *R, event_t *e, int readable, int writeable)
{
    struct epoll_event ev;
    ev.events = (readable ? EPOLLIN : 0) | (writeable ? EPOLLOUT : 0);
    ev.data.ptr = e;
    if (epoll_ctl(R->epfd, EPOLL_CTL_MOD, e->fd, &ev) == -1)
    {
        return 1;
    }
    return 0;
}

// 定时事件
void eventloop_once(reactor_t *r, int timeout)
{
    int n = epoll_wait(r->epfd, r->fire, MAX_EVENT_NUM, timeout);
    for (int i = 0; i < n; i++)
    {
        struct epoll_event *e = &r->fire[i];
        int mask = e->events;
        if (e->events & EPOLLERR)
            mask |= EPOLLIN | EPOLLOUT; // 具体错误由io函数去获取
        if (e->events & EPOLLHUP)
            mask |= EPOLLIN | EPOLLOUT;
        event_t *et = (event_t *)e->data.ptr;
        // if (e->events & EPOLLERR) {
        //     int error;
        //     socklen_t len = sizeof(error);
        //     int code = getsockopt(et->fd, SOL_SOCKET, SO_ERROR, &error, &len);
        //     const char *err = 0;
        //     if (code < 0) {
        //         err = strerror(errno);
        //     } else if (error != 0) {
        //         err = strerror(error);
        //     }
        //     printf("socket err %s\n", err);
        // }
        if (mask & EPOLLIN)
        {
            if (et->read_fn)
                et->read_fn(et->fd, EPOLLIN, et);
        }
        if (mask & EPOLLOUT)
        {
            if (et->write_fn)
                et->write_fn(et->fd, EPOLLOUT, et);
        }
        // if (e->events & EPOLLHUP) {
        //     del_event(et->r, et);
        //     close(et->fd);
        // }
    }
}

void stop_eventloop(reactor_t *r)
{
    r->stop = 1;
}

void eventloop(reactor_t *r)
{
    while (!r->stop)
    {
        // 定时事件的触发 ：  红黑树、最小堆、时间轮
        int timeout = find_nearest_expire_timer();
        eventloop_once(r, timeout);
        expire_timer();
    }
}

int create_server(reactor_t *R, short port, event_callback_fn func)
{
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        printf("create listenfd error!\n");
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    int reuse = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void *)&reuse, sizeof(int)) == -1)
    {
        printf("reuse address error: %s\n", strerror(errno));
        return -1;
    }

    if (bind(listenfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0)
    {
        printf("bind error %s\n", strerror(errno));
        return -1;
    }

    if (listen(listenfd, 5) < 0)
    {
        printf("listen error %s\n", strerror(errno));
        return -1;
    }

    if (set_nonblock(listenfd) < 0)
    {
        printf("set_nonblock error %s\n", strerror(errno));
        return -1;
    }

    R->listenfd = listenfd;

    event_t *e = new_event(R, listenfd, func, 0, 0);
    add_event(R, EPOLLIN, e);

    init_timer();

    printf("listen port : %d\n", port);
    return 0;
}

int read_socket(event_t *e)
{
    int fd = e->fd;
    int num = 0;
    while (1)
    {
        char buf[1024] = {0};
        int n = read(fd, buf, 1024);
        if (n == 0)
        {
            printf("close connection fd = %d\n", fd);
            if (e->error_fn)
                e->error_fn(fd, "close socket");
            del_event(e->r, e);
            close(fd);
            return 0;
        }
        else if (n < 0)
        {
            if (errno == EINTR) // 信号打断了
                continue;
            if (errno == EWOULDBLOCK)
                break;
            printf("read error fd = %d err = %s\n", fd, strerror(errno));
            if (e->error_fn)
                e->error_fn(fd, strerror(errno));
            del_event(e->r, e);
            close(fd);
            return 0;
        }
        else
        {
            rb_write(&e->in, buf, n);
        }
        num += n;
    }
    return num;
}

int readline(event_t *e, void *buf, int sz)
{
    return rb_readline(&e->in, (char *)buf, sz);
}

int write_socket_(event_t *e, void *buf, int sz)
{
    int fd = e->fd;
    while (1)
    {
        int n = write(fd, buf, sz);
        if (n < 0)
        {
            if (errno == EINTR) // 被信号打断了
                continue;
            if (errno == EWOULDBLOCK)
            { // 写缓冲区满了
                break;
            }
            if (e->error_fn)
                e->error_fn(fd, strerror(errno));
            del_event(e->r, e);
            close(e->fd);
        }
        return n;
    }
    return 0;
}

#define REAL_READ_POS(r) (r->read_pos & (r->size - 1))
#define READ_EDGE_DIS(r) (r->size - REAL_READ_POS(r))

int flush_socket(event_t *e)
{
    ringbuffer_t *r = &e->out;
    if (rb_isempty(r))
    {
        return 0;
    }
    int len = rb_len(r);
    int dis = READ_EDGE_DIS(r);

    if (dis >= len)
    {
        while (len > 0)
        {
            int n = write_socket_(e, r->buf + REAL_READ_POS(r), len);
            if (n == 0)
            {
                enable_event(e->r, e, 1, 1);
                return 0;
            }
            else if (n < 0)
                return -1;
            r->read_pos += n;
            len -= n;
        }
        enable_event(e->r, e, 1, 0);
    }
    else
    {
        int remain = len - dis;
        while (dis > 0)
        {
            int n = write_socket_(e, r->buf + REAL_READ_POS(r), dis);
            if (n == 0)
            {
                enable_event(e->r, e, 1, 1);
                return 0;
            }
            else if (n < 0)
                return -1;
            r->read_pos += n;
            dis -= n;
        }
        while (remain > 0)
        {
            int n = write_socket_(e, r->buf + REAL_READ_POS(r), remain);
            if (n == 0)
            {
                enable_event(e->r, e, 1, 1);
                return 0;
            }
            else if (n < 0)
                return -1;
            r->read_pos += n;
            remain -= n;
        }
        enable_event(e->r, e, 1, 0);
    }
    return 1;
}

int write_socket(event_t *e, void *buf, int sz)
{
    int n = write_socket_(e, buf, sz);
    if (n == 0 && sz > 0)
    {
        rb_write(&e->out, (char *)buf, sz);
        enable_event(e->r, e, 1, 1);
    }
    else if (n > 0)
    {
        enable_event(e->r, e, 1, 0);
    }
    return n;
}

#endif