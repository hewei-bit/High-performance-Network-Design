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

// 4G / 8m = 512 假设运行内存只有4g，每个客户端连接占8m，只能支持512个连接
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
    //使用socket套接字创建listenfd
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }
    //设置sockaddr结构体
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(9999);

    //绑定地址
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        printf("bind socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }
    //开启listen
    if (listen(listenfd, 10) == -1)
    {
        printf("listen socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }

#if 0
    //首先是最基本的方案，在while循环之前accept
    //只能实现单个客户端的连接
    //定义客户端地址
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
            //加上字符串的尾部，以便显示和转发
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
    //这份代码有问题
    //使用select实现多路复用
    //分别声明select中的读集合，写集合，读操作，写操作
    fd_set rfds, wfds, rset, wset;

    //全都置0
    FD_ZERO(&rfds);
    //将listen_fd加入读集合
    FD_SET(listenfd, &rfds);

    FD_ZERO(&wfds);

    int max_fd = listenfd;

    while (1)
    {
        //将新的读写操作更新读写集合
        rset = rfds;
        wset = wfds;
        //登记读写集合
        int nready = select(max_fd + 1, &rset, &wset, NULL, NULL);

        //监听到新的套接字
        if (FD_ISSET(listenfd, &rset))
        {
            struct sockaddr_in client;
            socklen_t len = sizeof(client);
            if ((connfd = accept(listenfd, (struct sockaddr *)&client, &len)) == -1)
            {
                printf("accept socket error: %s(errno: %d)\n", strerror(errno), errno);
                return 0;
            }
            //加入到写集合中
            FD_SET(connfd, &rfds);
            //更新max_fd
            if (connfd > max_fd)
                max_fd = connfd;
            //返回的套接字里面啥也没有
            if (--nready == 0)
                continue;
        }

        //循环读操作里面的
        int i = 0;
        for (i = listenfd + 1; i <= max_fd; i++)
        {
            //在读操作里
            if (FD_ISSET(i, &rset))
            {
                n = recv(i, buff, MAXLNE, 0);
                if (n > 0)
                {
                    buff[n] = '\0';
                    printf("recv msg from client: %s\n", buff);
                    //设置为可写
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

            //在写操作里
            else if (FD_ISSET(i, &wset))
            {
                send(i, buff, n, 0);
                //设置为可读
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