#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>

#include <sys/poll.h>
#include <sys/epoll.h>

#include <pthread.h>

#define MAXLNE 4096

#define POLL_SIZE 1024

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

//������������ķ�������whileѭ��֮ǰaccept
//ֻ��ʵ�ֵ����ͻ��˵�����
void accept_not_in_while_test(int listenfd)
{

    //����ͻ��˵�ַ
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    if ((connfd = accept(listenfd, (struct sockaddr *)&client, &len)) == -1)
    {
        printf("accept socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }
    printf("=================wait for client===================\n");
    while (1)
    {
        n = recv(connfd, buff, MAXLNE, 0);
        if (n > 0)
        {
            //�����ַ�����β�����Ա���ʾ��ת��
            buff[n] = '\0';
            printf("recv msg from clientL %s \n", buff);
            send(connfd, buff, n, 0);
        }
        else if (n == 0)
        {
            close(connfd);
        }
    }
}

// ��accept����whileѭ��������ʵ�ֶ���ͻ������ӣ�
//����ÿ��ֻ�ܽ��жԻ�һ��
void accept_in_while_test(int listenfd)
{

    printf("========waiting for client's request========\n");
    while (1)
    {

        struct sockaddr_in client;
        socklen_t len = sizeof(client);
        if ((connfd = accept(listenfd, (struct sockaddr *)&client, &len)) == -1)
        {
            printf("accept socket error: %s(errno: %d)\n", strerror(errno), errno);
            return 0;
        }

        n = recv(connfd, buff, MAXLNE, 0);
        if (n > 0)
        {
            buff[n] = '\0';
            printf("recv msg from client: %s\n", buff);

            send(connfd, buff, n, 0);
        }
        else if (n == 0)
        {
            close(connfd);
        }

        // close(connfd);
    }
}

// 4G / 8m = 512 ���������ڴ�ֻ��4g��ÿ���ͻ�������ռ8m��ֻ��֧��512������
// C10K
void *client_routine(void *arg)
{
    int connfd = *(int *)arg;

    char buff[MAXLNE];

    while (1)
    {
        int n = recv(connfd, buff, MAXLNE, 0);
        if (n > 0)
        {
            buff[n] = '\0';
            printf("recv msg from client: %s\n", buff);
            send(connfd, buff, n, 0);
        }
        else if (n == 0)
        {
            close(connfd);
            break;
        }
    }
    return NULL;
}

//���̣߳�����ʵ�ֶ���ͻ������ӣ�
//�����������ޣ�������
void pthread_test(int listenfd)
{

    while (1)
    {

        struct sockaddr_in client;
        socklen_t len = sizeof(client);
        if ((connfd = accept(listenfd, (struct sockaddr *)&client, &len)) == -1)
        {
            printf("accept socket error: %s(errno: %d)\n", strerror(errno), errno);
            return 0;
        }

        pthread_t threadid;
        pthread_create(&threadid, NULL, client_routine, (void *)&connfd);
    }
}

//ʹ��selectʵ�ֶ�·����
void select_test(int listenfd)
{

    //�ֱ�����select�еĶ����ϣ�д���ϣ���������д����
    fd_set rset, wset, rfds, wfds;

    //ȫ����0
    FD_ZERO(&rfds);
    //��listen_fd���������
    FD_SET(listenfd, &rfds);

    FD_ZERO(&wfds);

    int max_fd = listenfd;

    while (1)
    {
        //���µĶ�д�������¶�д����
        rset = rfds;
        wset = wfds;
        //�ǼǶ�д����
        int nready = select(max_fd + 1, &rset, &wset, NULL, NULL);

        //�������µ��׽���
        if (FD_ISSET(listenfd, &rset))
        {
            struct sockaddr_in client;
            socklen_t len = sizeof(client);
            if ((connfd = accept(listenfd, (struct sockaddr *)&client, &len)) == -1)
            {
                printf("accept socket error: %s(errno: %d)\n", strerror(errno), errno);
                return 0;
            }
            //���뵽д������
            FD_SET(connfd, &rfds);
            //����max_fd
            if (connfd > max_fd)
                max_fd = connfd;
            //���ص��׽�������ɶҲû��
            if (--nready == 0)
                continue;
        }

        //ѭ�������������
        int i = 0;
        for (i = listenfd + 1; i <= max_fd; i++)
        {
            //�ڶ�������
            if (FD_ISSET(i, &rset))
            {
                n = recv(i, buff, MAXLNE, 0);
                if (n > 0)
                {
                    buff[n] = '\0';
                    printf("recv msg from client: %s\n", buff);
                    //����Ϊ��д
                    FD_SET(i, &wfds);
                    // reactor
                    // send(i, buff, n, 0);
                }
                else if (n == 0)
                {
                    FD_CLR(i, &rfds);
                    close(i);
                }
                if (--nready == 0)
                    break;
            }

            //��д������
            else if (FD_ISSET(i, &wset))
            {
                send(i, buff, n, 0);
                FD_CLR(i, &wfds);
                //����Ϊ�ɶ�
                FD_SET(i, &rfds);
            }
        }
    }
}

void poll_test(int listenfd)
{
    // poll �Ȱ�listenfd�Ž�ȥ����ע���¼�
    struct pollfd fds[POLL_SIZE] = {0};
    fds[0].fd = listenfd;
    fds[0].events = POLLIN;

    int max_fd = listenfd;
    int i = 0;
    for (i = 1; i < POLL_SIZE; i++)
    {
        fds[i].fd = -1;
    }

    while (1)
    {
        int nready = poll(fds, max_fd + 1, -1);
        // listenfd�������¼�
        if (fds[0].revents & POLLIN)
        {
            struct sockaddr_in client;
            socklen_t len = sizeof(client);
            if ((connfd = accept(listenfd, (struct sockaddr *)&client, &len)) == -1)
            {
                printf("accept socket error: %s(errno: %d)\n", strerror(errno), errno);
                return 0;
            }
            //���մ˴��յ��Ŀͻ����ļ�������
            printf("accept \n");
            fds[connfd].fd = connfd;
            fds[connfd].events = POLLIN;

            if (connfd > max_fd)
                max_fd = connfd;

            if (--nready == 0)
                continue;
        }
        //����fds�ڲ�,��ȡreventȷ���Ƿ��յ�����
        for (i = listenfd + 1; i <= max_fd; i++)
        {
            if (fds[i].revents & POLLIN)
            {
                n = recv(i, buff, MAXLNE, 0);
                if (n > 0)
                {
                    buff[n] = '\0';
                    printf("recv msg from client: %s\n", buff);
                    //�Ѹո��յ��ķ���ȥ
                    send(i, buff, n, 0);
                }
                //�ͻ��˶Ͽ����ӣ�������Դ
                else if (n == 0)
                {
                    fds[i].fd = -1;
                    close(i);
                }
                if (--nready == 0)
                    break;
            }
        }
    }
}

void epoll_test(int listenfd)
{
    // poll/select -->
    //  epoll_create
    //  epoll_ctl(ADD, DEL, MOD)
    //  epoll_wait

    //����һ���ռ䣬������һ������
    int epfd = epoll_create(1);
    // epoll��Ե����¼�������һ���¼���������������Ҫ��ע���¼�
    struct epoll_event events[POLL_SIZE] = {0};
    //ĳ�ν��յ������¼�
    struct epoll_event ev;
    //�Ȱ�listenfd������ȥ
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    // ���listenfd��������������ӵ����ں˹����һ�ú��������
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    while (1)
    {
        //�������������Ǵ��ں��е���������ȡ�����Ž�events������
        //�鿴events���������ע���¼���������û����1�����һ���ǳ�ʱʱ��
        //û���򷵻�-1�����򷵻��¼�������
        int nready = epoll_wait(epfd, events, POLL_SIZE, 5);
        if (nready == -1)
        {
            continue;
        }

        int i = 0;
        for (i = 0; i < nready; i++)
        {
            //��ȡ��ȡ��fd
            int clientfd = events[i].data.fd;
            //�����listenfd�����յ����ǿͻ��˵�fd
            if (clientfd == listenfd)
            {
                struct sockaddr_in client;
                socklen_t len = sizeof(client);
                if ((connfd = accept(listenfd, (struct sockaddr *)&client, &len)) == -1)
                {
                    printf("accept socket error: %s(errno: %d)\n", strerror(errno), errno);
                    return 0;
                }
                printf("accept\n");
                //ͨ��epoll_clt��ӵ��ں˵ĺ������
                ev.events = EPOLLIN;
                ev.data.fd = connfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
            }
            // ��ȡ������Ϣ
            else if (events[i].events & EPOLLIN)
            {
                n = recv(clientfd, buff, MAXLNE, 0);
                if (n > 0)
                {
                    buff[n] = '\0';
                    printf("recv msg from client: %s\n", buff);
                    //������ֱ�ӷ��ͻ�ȥ�������淶
                    send(clientfd, buff, n, 0);
                }
                else if (n == 0)
                {
                    //�������ɾ����fd
                    ev.events = EPOLLIN;
                    ev.data.fd = clientfd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, &ev);
                    close(clientfd);
                }
            }
        }
    }
}

int main(int argc, char **argv)
{
    int listenfd, connfd, n;
    struct sockaddr_in servaddr;
    char buff[MAXLNE];

    listenfd = init_sock(htons(9999));

#if 0
    accept_not_in_while_test(int listenfd);

#elif 0
    accept_in_while_test(int listenfd);

#elif 0
    pthread_test(int listenfd);

#elif 0
    select_test(int listenfd);

#elif 0
    poll_test(int listenfd);

#elif 1

    epoll_test(int listenfd);

#endif
    close(listenfd);
    return 0;
}