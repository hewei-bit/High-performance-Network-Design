#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "ikcp.h"

#include <sys/time.h>
#include <sys/wait.h>
#include <arpa/inet.h>


#include "delay.h"
#define DELAY_TEST2_N 1
#define UDP_RECV_BUF_SIZE 1500

// 编译:  gcc -o client client.c ikcp.c delay.c  -lpthread


 typedef struct {
	unsigned char *ipstr;
	int port;
	
	ikcpcb *pkcp;
	
	int sockfd;
	struct sockaddr_in addr;//存放服务器的结构体
	
	char buff[UDP_RECV_BUF_SIZE];//存放收发的消息
}kcpObj;


/* sleep in millisecond */
void isleep(unsigned long millisecond)
{
	#ifdef __unix 	/* usleep( time * 1000 ); */
	struct timespec ts;
	ts.tv_sec = (time_t)(millisecond / 1000);
	ts.tv_nsec = (long)((millisecond % 1000) * 1000000);
	/*nanosleep(&ts, NULL);*/
	usleep((millisecond << 10) - (millisecond << 4) - (millisecond << 3));
	#elif defined(_WIN32)
	Sleep(millisecond);
	#endif
}



int udp_output(const char *buf, int len, ikcpcb *kcp, void *user){
   
 //  printf("使用udp_output发送数据\n");
   
    kcpObj *send = (kcpObj *)user;

	//发送信息
    int n = sendto(send->sockfd, buf, len, 0,(struct sockaddr *) &send->addr,sizeof(struct sockaddr_in));//【】
    if (n >= 0) 
	{       
		//会重复发送，因此牺牲带宽
	 	printf("send:%d bytes\n", n);//24字节的KCP头部
        return n;
    } 
	else 
	{
        printf("udp_output: %d bytes send, error\n", n);
        return -1;
    }
}


int init(kcpObj *send)
{	
	send->sockfd = socket(AF_INET,SOCK_DGRAM,0);
	
	if(send->sockfd < 0)
	{
		perror("socket error！");
		exit(1);
	}
	
	bzero(&send->addr, sizeof(send->addr));
	
	//设置服务器ip、port
	send->addr.sin_family=AF_INET;
    send->addr.sin_addr.s_addr = inet_addr((char*)send->ipstr);
    send->addr.sin_port = htons(send->port);
	
	printf("sockfd = %d ip = %s  port = %d\n",send->sockfd,send->ipstr,send->port);
	
}

// 特别说明，当我们使用kcp测试rtt的时候，如果发现rtt过大，很大一种可能是分片数据没有及时发送出去，需要调用ikcp_flush更快速将分片发送出去。
void delay_test2(kcpObj *send) {
    // 初始化 100个 delay obj
    char buf[UDP_RECV_BUF_SIZE];
	unsigned int len = sizeof(struct sockaddr_in);

    size_t obj_size = sizeof(t_delay_obj);
    t_delay_obj *objs = malloc(DELAY_TEST2_N * sizeof(t_delay_obj));
	int ret = 0;

	int recv_objs = 0;
	//ikcp_update包含ikcp_flush，ikcp_flush将发送队列中的数据通过下层协议UDP进行发送
 	ikcp_update(send->pkcp,iclock());//不是调用一次两次就起作用，要loop调用
    for(int i = 0; i < DELAY_TEST2_N; i++) {
		//  isleep(1);
		delay_set_seqno_send_time(&objs[i], i);  
		ret = ikcp_send(send->pkcp, (char *) &objs[i], obj_size); 
        if(ret < 0) {
            printf("send %d seqno:%u failed, ret:%d, obj_size:%ld\n", i, objs[i].seqno, ret, obj_size);
            return;
        } 
        // ikcp_flush(send->pkcp);		// 调用flush能更快速把分片发送出去
		//ikcp_update包含ikcp_flush，ikcp_flush将发送队列中的数据通过下层协议UDP进行发送
		ikcp_update(send->pkcp,iclock());//不是调用一次两次就起作用，要loop调用
		
		int n = recvfrom(send->sockfd, buf, UDP_RECV_BUF_SIZE, MSG_DONTWAIT,(struct sockaddr *) &send->addr,&len);
		// printf("print recv1:%d\n", n);
		if(n < 0) {//检测是否有UDP数据包 
			// isleep(1);
			continue;
		}
		ret = ikcp_input(send->pkcp, buf, n);	// 从 linux api recvfrom先扔到kcp引擎
		if(ret < 0)//检测ikcp_input是否提取到真正的数据
		{
			//printf("ikcp_input ret = %d\n",ret);
			continue;			// 没有读取到数据
		}	
		ret = ikcp_recv(send->pkcp, (char *)&objs[i], obj_size);		
		if(ret < 0)//检测ikcp_recv提取到的数据	
		{
			printf("ikcp_recv1 ret = %d\n",ret);
			continue;
		}
		delay_set_recv_time(&objs[recv_objs]);
		recv_objs++;
		printf("recv1 %d seqno:%d, ret:%d\n", recv_objs, objs[i].seqno, ret);
        if(ret != obj_size) {
            printf("recv1 %d seqno:%d failed, size:%d\n", i, objs[i].seqno, ret);
            delay_print_rtt_time(objs, i);
            return;
        }
    }

	// 还有没有发送完毕的数据
	for(int i = recv_objs; i < DELAY_TEST2_N; ) {
		//  isleep(1);
		//ikcp_update包含ikcp_flush，ikcp_flush将发送队列中的数据通过下层协议UDP进行发送
		ikcp_update(send->pkcp,iclock());//不是调用一次两次就起作用，要loop调用
		//ikcp_flush(send->pkcp);		// 调用flush能更快速把分片发送出去  
		int n = recvfrom(send->sockfd, buf, UDP_RECV_BUF_SIZE, MSG_DONTWAIT,(struct sockaddr *) &send->addr,&len);
		// printf("recv2:%d\n", n);
		if(n < 0) {//检测是否有UDP数据包
			// printf("recv2:%d\n", n);
			isleep(1);
			continue;
		}
			
		ret = ikcp_input(send->pkcp, buf, n);	
		if(ret < 0)//检测ikcp_input是否提取到真正的数据
		{
			printf("ikcp_input2 ret = %d\n",ret);
			continue;			// 没有读取到数据
		}	
		ret = ikcp_recv(send->pkcp, (char *)&objs[i], obj_size);		
		if(ret < 0)//检测ikcp_recv提取到的数据	
		{
			// printf("ikcp_recv2 ret = %d\n",ret);
			continue;
		}
		printf("recv2 %d seqno:%d, ret:%d\n", recv_objs,  objs[i].seqno, ret);
		delay_set_recv_time(&objs[recv_objs]);
		recv_objs++;
		i++;
        if(ret != obj_size) {
            printf("recv2 %d seqno:%d failed, size:%d\n", i, objs[i].seqno, ret);
            delay_print_rtt_time(objs, i);
            return;
        }
        
	}
	ikcp_flush(send->pkcp);

    delay_print_rtt_time(objs, DELAY_TEST2_N);
}

void loop(kcpObj *send)
{
	unsigned int len = sizeof(struct sockaddr_in);
	int n,ret;

	// while(1)
	{
		isleep(1);
		delay_test2(send);
	}
	printf("loop finish\n");
	close(send->sockfd);
	
}

int main(int argc,char *argv[])
{
	//printf("this is kcpClient,请输入服务器 ip地址和端口号：\n");
	if(argc != 3)
	{
		printf("请输入服务器ip地址和端口号\n");
		return -1;
	}
	printf("this is kcpClient\n");
	int64_t cur =  iclock64();
    printf("main started t:%ld\n", cur); // prints Hello World!!!
	unsigned char *ipstr = (unsigned char *)argv[1];
	unsigned char *port  = (unsigned char *)argv[2];
	
	kcpObj send;
	send.ipstr = ipstr;
	send.port = atoi(argv[2]);
	
	init(&send);//初始化send,主要是设置与服务器通信的套接字对象
	
	bzero(send.buff,sizeof(send.buff));
	
	// 每个连接都是需要对应一个ikcpcb
	ikcpcb *kcp = ikcp_create(0x1, (void *)&send);//创建kcp对象把send传给kcp的user变量
	kcp->output = udp_output;//设置kcp对象的回调函数
	ikcp_nodelay(kcp,0, 10, 0, 0);//(kcp1, 0, 10, 0, 0); 1, 10, 2, 1
	ikcp_wndsize(kcp, 128, 128);
	ikcp_setmtu(kcp, 1400);
	send.pkcp = kcp;	
	loop(&send);//循环处理
	ikcp_release(send.pkcp);
	printf("main finish t:%ldms\n", iclock64() - cur);
	return 0;	
}