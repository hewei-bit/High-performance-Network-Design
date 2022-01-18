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

//����������
#define BUFFER_LENGTH 4096
// epoll�е���������
#define MAX_EPOLL_EVENT 1024
#define SERVER_PORT 8888

typedef int NCALLBACK(int, int, void *);

//����ṹ��
struct ntyevent
{
    int fd;     //�����fd
    int events; //��������
    void *arg;  //��Ҫ�����ص������Ĳ���,һ�㴫����reactor��ָ��

    int (*callback)(int fd, int events, void *arg); //��Ӧ�Ļص�����

    int status; // 0:�½� 1���Ѵ���
    char buffer[BUFFER_LENGTH];
    int length;
    long last_active;
};

// reatorʹ�õĽṹ��
struct ntyreactor
{
    int epfd;                // reatctor��fd
    struct ntyevent *events; // reactor����Ļ�����Ԫ
};

int recv_cb(int fd, int events, void *arg);
int send_cb(int fd, int events, void *arg);
int accpet_cb(int fd, int events, void *arg);

void nty_event_set(struct ntyevent *ev, int fd, NCALLBACK callback, void *arg);
int nty_event_add(int epfd, int events, struct ntyevent *ev);
int nty_event_del(int epfd, struct ntyevent *ev);

//����ntyevent�Ĳ���
void nty_event_set(struct ntyevent *ev, int fd, NCALLBACK callback, void *arg)
{
    ev->fd = fd;
    ev->callback = callback;
    ev->events = 0;
    ev->arg = arg;
    ev->last_active = time(NULL);

    return;
}

//���ӻ��޸�
int nty_event_add(int epfd, int events, struct ntyevent *ev)
{
    //ʹ�õ���linux�ں����epoll
    struct epoll_event ep_ev = {0, {0}};
    ep_ev.data.ptr = ev;
    ep_ev.events = ev->events = events;
    //�жϸ��¼��Ƿ��Ѿ���ӹ�
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
    // epoll_ctl���ж�Ӧ����
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

//���ջص�����
int recv_cb(int fd, int events, void *arg)
{
    //��紫�λ�õ�ntyreactorָ��
    struct ntyreactor *reactor = (struct ntyreactor *)arg;
    struct ntyevent *ev = reactor->events + fd;

    //�˴��յ����ݵĳ���
    int len = recv(fd, ev->buffer, BUFFER_LENGTH, 0);
    //�յ���ֱ��ɾ����Ӧ��fd����������Ӧ
    nty_event_del(reactor->epfd, ev);
    //��ȷ���գ�����0
    if (len > 0)
    {
        ev->length = len;
        ev->buffer[len] = '\0';

        printf("C[%d]:%s\n", fd, ev->buffer);
        //�յ��Ժ�ֱ�ӷ��ͻ�ȥ
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
    //���ص��Ƿ��͵��ֽ���
    int len = send(fd, ev->buffer, ev->length, 0);
    //��ȷ����,ɾ������fd��ע����յ�fd
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
