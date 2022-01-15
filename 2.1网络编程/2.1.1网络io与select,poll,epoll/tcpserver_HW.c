#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/poll.h>
#include <sys/epoll.h>

#include <pthread.h>

#define MAXLNE 4096

#define POLL_SIZE 1024

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

int main(int argc, char **argv)
{
    int listenfd, connfd, n;
    struct sockaddr_in servaddr;
    char buff[MAXLNE];
    //ʹ��socket�׽��ִ���listenfd
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }
    //����sockaddr�ṹ��
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(9999);

    //�󶨵�ַ
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        printf("bind socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }
    //����listen
    if (listen(listenfd, 10) == -1)
    {
        printf("listen socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }

#if 0
    //������������ķ�������whileѭ��֮ǰaccept
    //ֻ��ʵ�ֵ����ͻ��˵�����
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

#elif 0
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
#elif 0
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

#elif 0
    //��ݴ���������
    //ʹ��selectʵ�ֶ�·����
    //�ֱ�����select�еĶ����ϣ�д���ϣ���������д����
    fd_set rfds, wfds, rset, wset;

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
                //����Ϊ�ɶ�
                FD_SET(i, &rfds);
            }
        }
    }
#elif 1

    //
    fd_set rfds, rset;

    FD_ZERO(&rfds);
    FD_SET(listenfd, &rfds);

    int max_fd = listenfd;

    while (1)
    {

        rset = rfds;

        int nready = select(max_fd + 1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(listenfd, &rset))
        { //

            struct sockaddr_in client;
            socklen_t len = sizeof(client);
            if ((connfd = accept(listenfd, (struct sockaddr *)&client, &len)) == -1)
            {
                printf("accept socket error: %s(errno: %d)\n", strerror(errno), errno);
                return 0;
            }

            FD_SET(connfd, &rfds);

            if (connfd > max_fd)
                max_fd = connfd;

            if (--nready == 0)
                continue;
        }

        int i = 0;
        for (i = listenfd + 1; i <= max_fd; i++)
        {

            if (FD_ISSET(i, &rset))
            { //

                n = recv(i, buff, MAXLNE, 0);
                if (n > 0)
                {
                    buff[n] = '\0';
                    printf("recv msg from client: %s\n", buff);

                    send(i, buff, n, 0);
                }
                else if (n == 0)
                {

                    FD_CLR(i, &rfds);
                    // printf("disconnect\n");
                    close(i);
                }
                if (--nready == 0)
                    break;
            }
        }
    }
#elif 0

#endif
    close(listenfd);
    return 0;
}