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

// 8m * 4G = 128 , 512
// C10K
void *client_routine(void *arg)
{
}

int main(int argc, char **argv)
{
    int listenfd, connfd, n;
    struct sockaddr_in servaddr;
    char buff[MAXLNE];

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        printf("create socket error:")
    }
}