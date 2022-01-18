#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
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
    void *arg;  //
    //��Ӧ�Ļص�����
    int (*callback)(int fd, int events, void *arg);

    int status;
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

void nty_event_set(struct ntyevent *ev, int fd, NCALLBACK callback, void *arg)
{
    ev->fd = fd;
    ev->callback = callback;
    ev->events = 0;
    ev->arg = arg;
    ev->last_active = time(NULL);
}
int nty_event_add(int epfd, int events, struct ntyevent *ev);
int nty_event_del(int epfd, struct ntyevent *ev);
