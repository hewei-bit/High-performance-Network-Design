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
    //将accept放入while循环，可以实现多个客户端连接，
    //但是每次只能进行对话一次
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
    //开线程，可以实现多个客户端连接，
    //但是数量有限，开销大
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
    //这份代码有问题，已修改
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
                FD_CLR(i, &wfds);
                //设置为可读
                FD_SET(i, &rfds);
            }
        }
    }
#elif 0
    //放了一个正常的select
    fd_set rfds, rset;

    FD_ZERO(&rfds);
    FD_SET(listenfd, &rfds);

    int max_fd = listenfd;

    while (1)
    {
        //每次都会被清空，需要重新放入
        rset = rfds;

        int nready = select(max_fd + 1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(listenfd, &rset))
        {
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
            {
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
                    close(i);
                }
                if (--nready == 0)
                    break;
            }
        }
    }
#elif 0
    // poll 先把listenfd放进去，关注读事件
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
        // listenfd发生读事件
        if (fds[0].revents & POLLIN)
        {
            struct sockaddr_in client;
            socklen_t len = sizeof(client);
            if ((connfd = accept(listenfd, (struct sockaddr *)&client, &len)) == -1)
            {
                printf("accept socket error: %s(errno: %d)\n", strerror(errno), errno);
                return 0;
            }
            //接收此次收到的客户端文件描述符
            printf("accept \n");
            fds[connfd].fd = connfd;
            fds[connfd].events = POLLIN;

            if (connfd > max_fd)
                max_fd = connfd;

            if (--nready == 0)
                continue;
        }
        //遍历fds内部,读取revent确定是否收到数据
        for (i = listenfd + 1; i <= max_fd; i++)
        {
            if (fds[i].revents & POLLIN)
            {
                n = recv(i, buff, MAXLNE, 0);
                if (n > 0)
                {
                    buff[n] = '\0';
                    printf("recv msg from client: %s\n", buff);
                    //把刚刚收到的发出去
                    send(i, buff, n, 0);
                }
                //客户端断开连接，回收资源
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
#elif 1
    // poll/select -->
    //  epoll_create
    //  epoll_ctl(ADD, DEL, MOD)
    //  epoll_wait

    //创建一个空间，假设是一个大房子
    int epfd = epoll_create(1);
    // epoll针对的是事件，创建一个事件数组用来管理需要关注的事件
    struct epoll_event events[POLL_SIZE] = {0};
    //某次接收到的新事件
    struct epoll_event ev;
    //先把listenfd先塞进去
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    // 添加listenfd，持续监听，添加到了内核管理的一棵红黑树上面
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    while (1)
    {
        //非阻塞，这里是从内核中的链表中提取出来放进events数组里
        //查看events数组里面关注的事件描述符有没有置1，最后一个是超时时间
        //没有则返回-1，有则返回事件的数量
        int nready = epoll_wait(epfd, events, POLL_SIZE, 5);
        if (nready == -1)
        {
            continue;
        }

        int i = 0;
        for (i = 0; i < nready; i++)
        {
            //读取获取的fd
            int clientfd = events[i].data.fd;
            //如果是listenfd，则收到的是客户端的fd
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
                //通过epoll_clt添加到内核的红黑树里
                ev.events = EPOLLIN;
                ev.data.fd = connfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
            }
            // 获取到的消息
            else if (events[i].events & EPOLLIN)
            {
                n = recv(clientfd, buff, MAXLNE, 0);
                if (n > 0)
                {
                    buff[n] = '\0';
                    printf("recv msg from client: %s\n", buff);
                    //这里是直接发送回去，并不规范
                    send(clientfd, buff, n, 0);
                }
                else if (n == 0)
                {
                    //传输完成删除该fd
                    ev.events = EPOLLIN;
                    ev.data.fd = clientfd;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, &ev);
                    close(clientfd);
                }
            }
        }
    }

#endif
    close(listenfd);
    return 0;
}