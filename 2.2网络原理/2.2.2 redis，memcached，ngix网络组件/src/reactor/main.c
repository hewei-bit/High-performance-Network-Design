#include "reactor.h"

// typedef void (*event_callback_fn)(int fd, int events, void *privdata);
// typedef void (*error_callback_fn)(int fd, char * err);

void read_cb(int fd, int events, void *privdata)
{
    event_t *e = (event_t *)privdata;
    if (read_socket(e) > 0)
    {
        char buf[1024] = {0};
        int n = readline(e, buf, 1024);
        if (n > 0)
        {
            printf("recv data from client:%s", buf);
            write_socket(e, buf, n + 1);
        }
    }
}

void write_cb(int fd, int events, void *privdata)
{
    printf("recv  write_cb fd = %d\n", fd);
    event_t *e = (event_t *)privdata;
    flush_socket(e);
}

void error_cb(int fd, char *err)
{
    printf("connection fd = %d, close  err: %s", fd, err);
}

void accept_cb(int fd, int events, void *privdata)
{
    event_t *e = (event_t *)privdata;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(addr);

    int clientfd = accept(fd, (struct sockaddr *)&addr, &len);
    if (clientfd <= 0)
    {
        return;
    }

    char str[INET_ADDRSTRLEN] = {0};
    printf("recv from %s at port %d\n", inet_ntop(AF_INET, &addr.sin_addr, str, sizeof(str)),
           ntohs(addr.sin_port));

    event_t *ne = new_event(e->r, clientfd, read_cb, write_cb, error_cb);
    add_event(e->r, EPOLLIN, ne);
    set_nonblock(clientfd);
}

int main()
{
    reactor_t *R = create_reactor();

    if (create_server(R, 8989, accept_cb) != 0)
    {
        release_reactor(R);
        return 1;
    }

    eventloop(R);

    release_reactor(R);
    return 0;
}