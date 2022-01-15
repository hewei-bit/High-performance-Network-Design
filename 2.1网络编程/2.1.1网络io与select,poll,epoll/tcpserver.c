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
 
#define MAXLNE  4096

#define POLL_SIZE	1024

//8m * 4G = 128 , 512
//C10k
void *client_routine(void *arg) { //

	int connfd = *(int *)arg;

	char buff[MAXLNE];

	while (1) {

		int n = recv(connfd, buff, MAXLNE, 0);
        if (n > 0) {
            buff[n] = '\0';
            printf("recv msg from client: %s\n", buff);

	    	send(connfd, buff, n, 0);
        } else if (n == 0) {
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
 
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }
 
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(9999);
 
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        printf("bind socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }
 
    if (listen(listenfd, 10) == -1) {
        printf("listen socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }
 #if 0
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    if ((connfd = accept(listenfd, (struct sockaddr *)&client, &len)) == -1) {
        printf("accept socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }

    printf("========waiting for client's request========\n");
    while (1) {

        n = recv(connfd, buff, MAXLNE, 0);
        if (n > 0) {
            buff[n] = '\0';
            printf("recv msg from client: %s\n", buff);

	    	send(connfd, buff, n, 0);
        } else if (n == 0) {
            close(connfd);
        }
        
        //close(connfd);
    }

#elif 0


    printf("========waiting for client's request========\n");
    while (1) {

		struct sockaddr_in client;
	    socklen_t len = sizeof(client);
	    if ((connfd = accept(listenfd, (struct sockaddr *)&client, &len)) == -1) {
	        printf("accept socket error: %s(errno: %d)\n", strerror(errno), errno);
	        return 0;
	    }

        n = recv(connfd, buff, MAXLNE, 0);
        if (n > 0) {
            buff[n] = '\0';
            printf("recv msg from client: %s\n", buff);

	    	send(connfd, buff, n, 0);
        } else if (n == 0) {
            close(connfd);
        }
        
        //close(connfd);
    }

#elif 0

	while (1) {

		struct sockaddr_in client;
	    socklen_t len = sizeof(client);
	    if ((connfd = accept(listenfd, (struct sockaddr *)&client, &len)) == -1) {
	        printf("accept socket error: %s(errno: %d)\n", strerror(errno), errno);
	        return 0;
	    }

		pthread_t threadid;
		pthread_create(&threadid, NULL, client_routine, (void*)&connfd);

    }

#elif 0

	// 
	fd_set rfds, rset, wfds, wset;

	FD_ZERO(&rfds);
	FD_SET(listenfd, &rfds);

	FD_ZERO(&wfds);

	int max_fd = listenfd;

	while (1) {

		rset = rfds;
		wset = wfds;

		int nready = select(max_fd+1, &rset, &wset, NULL, NULL);


		if (FD_ISSET(listenfd, &rset)) { //

			struct sockaddr_in client;
		    socklen_t len = sizeof(client);
		    if ((connfd = accept(listenfd, (struct sockaddr *)&client, &len)) == -1) {
		        printf("accept socket error: %s(errno: %d)\n", strerror(errno), errno);
		        return 0;
		    }

			FD_SET(connfd, &rfds);

			if (connfd > max_fd) max_fd = connfd;

			if (--nready == 0) continue;

		}

		int i = 0;
		for (i = listenfd+1;i <= max_fd;i ++) {

			if (FD_ISSET(i, &rset)) { // 

				n = recv(i, buff, MAXLNE, 0);
		        if (n > 0) {
		            buff[n] = '\0';
		            printf("recv msg from client: %s\n", buff);

					FD_SET(i, &wfds);

					//reactor
					//send(i, buff, n, 0);
		        } else if (n == 0) { //

					FD_CLR(i, &rfds);
					//printf("disconnect\n");
		            close(i);
					
		        }
				if (--nready == 0) break;
			} else if (FD_ISSET(i, &wset)) {

				send(i, buff, n, 0);
				FD_SET(i, &rfds);
			
			}

		}
		

	}

#elif 0


	struct pollfd fds[POLL_SIZE] = {0};
	fds[0].fd = listenfd;
	fds[0].events = POLLIN;

	int max_fd = listenfd;
	int i = 0;
	for (i = 1;i < POLL_SIZE;i ++) {
		fds[i].fd = -1;
	}

	while (1) {

		int nready = poll(fds, max_fd+1, -1);
	
		if (fds[0].revents & POLLIN) {

			struct sockaddr_in client;
		    socklen_t len = sizeof(client);
		    if ((connfd = accept(listenfd, (struct sockaddr *)&client, &len)) == -1) {
		        printf("accept socket error: %s(errno: %d)\n", strerror(errno), errno);
		        return 0;
		    }

			printf("accept \n");
			fds[connfd].fd = connfd;
			fds[connfd].events = POLLIN;

			if (connfd > max_fd) max_fd = connfd;

			if (--nready == 0) continue;
		}

		//int i = 0;
		for (i = listenfd+1;i <= max_fd;i ++)  {

			if (fds[i].revents & POLLIN) {
				
				n = recv(i, buff, MAXLNE, 0);
		        if (n > 0) {
		            buff[n] = '\0';
		            printf("recv msg from client: %s\n", buff);

					send(i, buff, n, 0);
		        } else if (n == 0) { //

					fds[i].fd = -1;

		            close(i);
					
		        }
				if (--nready == 0) break;

			}

		}

	}

#else

	//poll/select --> 
	// epoll_create 
	// epoll_ctl(ADD, DEL, MOD)
	// epoll_wait

	int epfd = epoll_create(1); //int size

	struct epoll_event events[POLL_SIZE] = {0};
	struct epoll_event ev;

	ev.events = EPOLLIN;
	ev.data.fd = listenfd;

	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

	while (1) {

		int nready = epoll_wait(epfd, events, POLL_SIZE, 5);
		if (nready == -1) {
			continue;
		}

		int i = 0;
		for (i = 0;i < nready;i ++) {

			int clientfd =  events[i].data.fd;
			if (clientfd == listenfd) {

				struct sockaddr_in client;
			    socklen_t len = sizeof(client);
			    if ((connfd = accept(listenfd, (struct sockaddr *)&client, &len)) == -1) {
			        printf("accept socket error: %s(errno: %d)\n", strerror(errno), errno);
			        return 0;
			    }

				printf("accept\n");
				ev.events = EPOLLIN;
				ev.data.fd = connfd;
				epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);

			} else if (events[i].events & EPOLLIN) {

				n = recv(clientfd, buff, MAXLNE, 0);
		        if (n > 0) {
		            buff[n] = '\0';
		            printf("recv msg from client: %s\n", buff);

					send(clientfd, buff, n, 0);
		        } else if (n == 0) { //


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

