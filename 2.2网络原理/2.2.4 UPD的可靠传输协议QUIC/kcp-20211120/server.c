#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include "ikcp.h"
#define RECV_BUF 1500

static int number = 0;

typedef struct
{
	unsigned char *ipstr;
	int port;

	ikcpcb *pkcp;

	int sockfd;

	struct sockaddr_in addr;	  //存放服务器信息的结构体
	struct sockaddr_in CientAddr; //存放客户机信息的结构体

	char buff[RECV_BUF]; //存放收发的消息

} kcpObj;
// 编译:  gcc -o server server.c ikcp.c  
// 特别需要注意，这里的服务器端也只能一次使用，即是等客户端退出后，服务端也要停止掉再启动
// 之所以是这样，主要是因为sn的问题，比如客户端第一次启动 sn 0~5， 第二次启动发送的sn还是0 ~5 如果服务器端不停止则自己以为0~5已经收到过了就不会回复。

// 在真正使用的时候，还需要另外的通道让客户端和服务器端之前重新创建ikcpcb，以匹配ikcpcb的conv
/* get system time */
void itimeofday(long *sec, long *usec)
{
#if defined(__unix)
	struct timeval time;
	gettimeofday(&time, NULL);
	if (sec)
		*sec = time.tv_sec;
	if (usec)
		*usec = time.tv_usec;
#else
	static long mode = 0, addsec = 0;
	BOOL retval;
	static IINT64 freq = 1;
	IINT64 qpc;
	if (mode == 0)
	{
		retval = QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
		freq = (freq == 0) ? 1 : freq;
		retval = QueryPerformanceCounter((LARGE_INTEGER *)&qpc);
		addsec = (long)time(NULL);
		addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
		mode = 1;
	}
	retval = QueryPerformanceCounter((LARGE_INTEGER *)&qpc);
	retval = retval * 2;
	if (sec)
		*sec = (long)(qpc / freq) + addsec;
	if (usec)
		*usec = (long)((qpc % freq) * 1000000 / freq);
#endif
}

/* get clock in millisecond 64 */
IINT64 iclock64(void)
{
	long s, u;
	IINT64 value;
	itimeofday(&s, &u);
	value = ((IINT64)s) * 1000 + (u / 1000);
	return value;
}

IUINT32 iclock()
{
	return (IUINT32)(iclock64() & 0xfffffffful);
}

int64_t first_recv_time = 0;
/* sleep in millisecond */
void isleep(unsigned long millisecond)
{
#ifdef __unix /* usleep( time * 1000 ); */
	struct timespec ts;
	ts.tv_sec = (time_t)(millisecond / 1000);
	ts.tv_nsec = (long)((millisecond % 1000) * 1000000);
	/*nanosleep(&ts, NULL);*/
	usleep((millisecond << 10) - (millisecond << 4) - (millisecond << 3));
#elif defined(_WIN32)
	Sleep(millisecond);
#endif
}

int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{

	kcpObj *send = (kcpObj *)user;

	//发送信息
	int n = sendto(send->sockfd, buf, len, 0, (struct sockaddr *)&send->CientAddr, sizeof(struct sockaddr_in));
	if (n >= 0)
	{
		//会重复发送，因此牺牲带宽
		printf("send: %d bytes, t:%lld\n", n, iclock64() - first_recv_time); //24字节的KCP头部
		return n;
	}
	else
	{
		printf("error: %d bytes send, error\n", n);
		return -1;
	}
}

int init(kcpObj *send)
{
	send->sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	if (send->sockfd < 0)
	{
		perror("socket error！");
		exit(1);
	}

	bzero(&send->addr, sizeof(send->addr));

	send->addr.sin_family = AF_INET;
	send->addr.sin_addr.s_addr = htonl(INADDR_ANY); //INADDR_ANY
	send->addr.sin_port = htons(send->port);

	printf("服务器socket: %d  port:%d\n", send->sockfd, send->port);

	if (send->sockfd < 0)
	{
		perror("socket error！");
		exit(1);
	}

	if (bind(send->sockfd, (struct sockaddr *)&(send->addr), sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind");
		exit(1);
	}
}

void loop(kcpObj *send)
{
	unsigned int len = sizeof(struct sockaddr_in);
	int n, ret;
	//接收到第一个包就开始循环处理
	int recv_count = 0;

	isleep(1);
	ikcp_update(send->pkcp, iclock());

	char buf[RECV_BUF] = {0};

	while (1)
	{
		isleep(1);
		ikcp_update(send->pkcp, iclock());
		//处理收消息
		n = recvfrom(send->sockfd, buf, RECV_BUF, MSG_DONTWAIT, (struct sockaddr *)&send->CientAddr, &len);
		if (n > 0)
		{
			printf("UDP recv[%d]  size= %d   \n", recv_count++, n);
			if (first_recv_time == 0)
			{
				first_recv_time = iclock64();
			}
			//预接收数据:调用ikcp_input将裸数据交给KCP，这些数据有可能是KCP控制报文，并不是我们要的数据。
			//kcp接收到下层协议UDP传进来的数据底层数据buffer转换成kcp的数据包格式
			ret = ikcp_input(send->pkcp, buf, n);
			if (ret < 0)
			{
				continue;
			}
			//kcp将接收到的kcp数据包还原成之前kcp发送的buffer数据
			ret = ikcp_recv(send->pkcp, buf, n); //从 buf中 提取真正数据，返回提取到的数据大小
			if (ret < 0)
			{ // 没有检测ikcp_recv提取到的数据
				isleep(1);
				continue;
			}
			int send_size = ret;
			//ikcp_send只是把数据存入发送队列，没有对数据加封kcp头部数据
			//应该是在kcp_update里面加封kcp头部数据
			//ikcp_send把要发送的buffer分片成KCP的数据包格式，插入待发送队列中。
			ret = ikcp_send(send->pkcp, buf, send_size);
			printf("Server reply ->  bytes[%d], ret = %d\n", send_size, ret);
			ikcp_flush(send->pkcp);	// 快速flush一次 以更快让客户端收到数据
			number++;
		}
		else if (n == 0)
		{
			printf("finish loop\n");
			break;
		}
		else
		{
			// printf("n:%d\n", n);
		}
	}
}

int main(int argc, char *argv[])
{
	printf("this is kcpServer\n");
	if (argc < 2)
	{
		printf("请输入服务器端口号\n");
		return -1;
	}

	kcpObj send;
	send.port = atoi(argv[1]);
	send.pkcp = NULL;

	bzero(send.buff, sizeof(send.buff));
	char Msg[] = "Server:Hello!"; //与客户机后续交互
	memcpy(send.buff, Msg, sizeof(Msg));

	ikcpcb *kcp = ikcp_create(0x1, (void *)&send); //创建kcp对象把send传给kcp的user变量
	ikcp_setmtu(kcp, 1400);
	kcp->output = udp_output;		//设置kcp对象的回调函数
	ikcp_nodelay(kcp, 0, 10, 0, 0); //1, 10, 2, 1
	ikcp_wndsize(kcp, 128, 128);

	send.pkcp = kcp;

	init(&send); //服务器初始化套接字
	loop(&send); //循环处理

	return 0;
}