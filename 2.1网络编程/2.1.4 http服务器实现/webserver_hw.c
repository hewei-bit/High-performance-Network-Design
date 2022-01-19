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
#define MAX_EPOLL_EVENTS 1024
#define SERVER_PORT 8888
#define PORT_COUNT 100

typedef int NCALLBACK(int, int, void *);

//����ṹ��
struct ntyevent
{
    int fd;     //�����fd
    int events; // epoll events����
    void *arg;  //��Ҫ�����ص������Ĳ���,һ�㴫����reactor��ָ��

    int (*callback)(int fd, int events, void *arg); //��Ӧ�Ļص�����

    int status; // 0:�½� 1���Ѵ���
    char buffer[BUFFER_LENGTH];
    int length;
    long last_active;
};

struct eventblock
{
    struct eventblock *next; //ָ����һ��block
    struct ntyevent *events; //��ǰblock��Ӧ��event����
};

// reatorʹ�õĽṹ��
struct ntyreactor
{
    int epfd; // reatctor��fd
    int blkcnt;
    struct eventblock *evblk; // fd --> 100w
	 // reactor����Ļ�����Ԫ��������block��ʵ��C1000K
};

int recv_cb(int fd, int events, void *arg);
int send_cb(int fd, int events, void *arg);
int accept_cb(int fd, int events, void *arg);

void nty_event_set(struct ntyevent *ev, int fd, NCALLBACK callback, void *arg);
int nty_event_add(int epfd, int events, struct ntyevent *ev);
int nty_event_del(int epfd, struct ntyevent *ev);
struct ntyevent *ntyreactor_idx(struct ntyreactor *reactor, int sockfd);

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
    //��һ���ǳ��ؼ�����������data��ptr�����ntyeventָ��
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

//���ջص�����
int recv_cb(int fd, int events, void *arg)
{
    //��紫�λ�õ�ntyreactorָ��
    struct ntyreactor *reactor = (struct ntyreactor *)arg;
    //���ں˵�������ȡ����ǰ��event
    struct ntyevent *ev = ntyreactor_idx(reactor, fd);
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
        // printf("[fd=%d] pos[%ld], closed\n", fd, ev - reactor->events);
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
    struct ntyevent *ev = ntyreactor_idx(reactor, fd);
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

//����һ��block�Ŀռ�
int ntyreactor_alloc(struct ntyreactor *reactor)
{
    //�ռ��
    if (reactor == NULL)
        return -1;
    if (reactor->evblk == NULL)
        return -1;

    //���ҵ�ǰblock��ĩβ
    struct eventblock *blk = reactor->evblk;
    while (blk->next != NULL)
    {
        blk = blk->next;
    }

    //�����ڴ�
    //����event
    struct ntyevent *evs = (struct ntyevent *)malloc((MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));
    if (evs == NULL)
    {
        printf("ntyreactor_alloc ntyevents failed\n");
        return -2;
    }
    memset(evs, 0, (MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));
    //����block����
    struct eventblock *block = (struct eventblock *)malloc(sizeof(struct eventblock));
    if (block == NULL)
    {
        printf("ntyreactor_alloc eventblock failed\n");
        return -2;
    }
    memset(block, 0, sizeof(struct eventblock));
    //��������ĩβ
    block->events = evs;
    block->next = NULL;

    blk->next = block;
    reactor->blkcnt++;
    return 0;
}

//ͨ��fd�ҵ���֮��Ӧ��events
struct ntyevent *ntyreactor_idx(struct ntyreactor *reactor, int sockfd)
{
    //��ȡ��ǰfd��Ӧevents���ڵ�blk
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

// reactor��ʼ��
int ntyreactor_init(struct ntyreactor *reactor)
{
    if (reactor == NULL)
    {
        return -1;
    }
    memset(reactor, 0, sizeof(struct ntyreactor));

    //��������
    reactor->epfd = epoll_create(1);
    if (reactor->epfd <= 0)
    {
        printf("create epfd in %s err %s\n", __func__, strerror(errno));
        return -2;
    }

    //����events�Ŀռ�
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
    //�ͷŵ�֮ǰmalloc��������Դ
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

//�������Ļص�����
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
        // 100��Ϊ��λ�Ľ��м���Ƿ�ʱ
        long now = time(NULL);
        for (int i = 0; i < 100; i++, checkpos++)
        {
            //���˾����
            if (checkpos == MAX_EPOLL_EVENT)
            {
                checkpos = 0;
            }
            //δ��������
            if (reactor->events[checkpos].status != 1)
            {
                continue;
            }
            //�������ʱ��̫���˾Ͷϵ�
            long duration = now - reactor->events[checkpos].last_active;

            if (duration >= 60)
            {
                close(reactor->events[checkpos].fd);
                printf("[fd=%d] timeout\n", reactor->events[checkpos].fd);
                nty_event_del(reactor->epfd, &reactor->events[checkpos]);
            }
        }
*/
        //���ں��а�������ȡ�����ŵ�events����
        int nready = epoll_wait(reactor->epfd, events, MAX_EPOLL_EVENTS, 1000);
        if (nready < 0)
        {
            printf("epoll_wait error, exit\n");
            continue;
        }

        //������ؼ��ĵط�ʵ�ֻص�����
        for (i = 0; i < nready; i++)
        {
            //����data��һ��union�������ľ�ֻ��֮ǰ�Ž�ȥ��ָ��ntyevent��ָ��
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
    //Ĭ�϶˿ں�Ϊȫ�ֱ���
    unsigned short port = SERVER_PORT;
    //������˲ξͰ����Ĳ�����
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
