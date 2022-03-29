/**
 * @File Name: client.c
 * @Brief :
 * @Author : hewei (hewei_1996@qq.com)
 * @Version : 1.0
 * @Creat Date : 2022-03-29
 *
 */

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

// ����:  gcc -o client client.c ikcp.c delay.c  -lpthread

typedef struct
{
    unsigned char *ipstr;
    int port;

    ikcpcb *pkcp;

    int sockfd;
    struct sockaddr_in addr; //��ŷ������Ľṹ��

    char buff[UDP_RECV_BUF_SIZE]; //����շ�����Ϣ
} kcpObj;

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

    //  printf("ʹ��udp_output��������\n");

    kcpObj *send = (kcpObj *)user;

    //������Ϣ
    int n = sendto(send->sockfd, buf, len, 0, (struct sockaddr *)&send->addr, sizeof(struct sockaddr_in)); //����
    if (n >= 0)
    {
        //���ظ����ͣ������������
        printf("send:%d bytes\n", n); // 24�ֽڵ�KCPͷ��
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
    send->sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (send->sockfd < 0)
    {
        perror("socket error��");
        exit(1);
    }

    bzero(&send->addr, sizeof(send->addr));

    //���÷�����ip��port
    send->addr.sin_family = AF_INET;
    send->addr.sin_addr.s_addr = inet_addr((char *)send->ipstr);
    send->addr.sin_port = htons(send->port);

    printf("sockfd = %d ip = %s  port = %d\n", send->sockfd, send->ipstr, send->port);
}

// �ر�˵����������ʹ��kcp����rtt��ʱ���������rtt���󣬺ܴ�һ�ֿ����Ƿ�Ƭ����û�м�ʱ���ͳ�ȥ����Ҫ����ikcp_flush�����ٽ���Ƭ���ͳ�ȥ��
void delay_test2(kcpObj *send)
{
    // ��ʼ�� 100�� delay obj
    char buf[UDP_RECV_BUF_SIZE];
    unsigned int len = sizeof(struct sockaddr_in);

    size_t obj_size = sizeof(t_delay_obj);
    t_delay_obj *objs = malloc(DELAY_TEST2_N * sizeof(t_delay_obj));
    int ret = 0;

    int recv_objs = 0;
    // ikcp_update����ikcp_flush��ikcp_flush�����Ͷ����е�����ͨ���²�Э��UDP���з���
    ikcp_update(send->pkcp, iclock()); //���ǵ���һ�����ξ������ã�Ҫloop����
    for (int i = 0; i < DELAY_TEST2_N; i++)
    {
        //  isleep(1);
        delay_set_seqno_send_time(&objs[i], i);
        ret = ikcp_send(send->pkcp, (char *)&objs[i], obj_size);
        if (ret < 0)
        {
            printf("send %d seqno:%u failed, ret:%d, obj_size:%ld\n", i, objs[i].seqno, ret, obj_size);
            return;
        }
        // ikcp_flush(send->pkcp);		// ����flush�ܸ����ٰѷ�Ƭ���ͳ�ȥ
        // ikcp_update����ikcp_flush��ikcp_flush�����Ͷ����е�����ͨ���²�Э��UDP���з���
        ikcp_update(send->pkcp, iclock()); //���ǵ���һ�����ξ������ã�Ҫloop����

        int n = recvfrom(send->sockfd, buf, UDP_RECV_BUF_SIZE, MSG_DONTWAIT, (struct sockaddr *)&send->addr, &len);
        // printf("print recv1:%d\n", n);
        if (n < 0)
        { //����Ƿ���UDP���ݰ�
            // isleep(1);
            continue;
        }
        ret = ikcp_input(send->pkcp, buf, n); // �� linux api recvfrom���ӵ�kcp����
        if (ret < 0)                          //���ikcp_input�Ƿ���ȡ������������
        {
            // printf("ikcp_input ret = %d\n",ret);
            continue; // û�ж�ȡ������
        }
        ret = ikcp_recv(send->pkcp, (char *)&objs[i], obj_size);
        if (ret < 0) //���ikcp_recv��ȡ��������
        {
            printf("ikcp_recv1 ret = %d\n", ret);
            continue;
        }
        delay_set_recv_time(&objs[recv_objs]);
        recv_objs++;
        printf("recv1 %d seqno:%d, ret:%d\n", recv_objs, objs[i].seqno, ret);
        if (ret != obj_size)
        {
            printf("recv1 %d seqno:%d failed, size:%d\n", i, objs[i].seqno, ret);
            delay_print_rtt_time(objs, i);
            return;
        }
    }

    // ����û�з�����ϵ�����
    for (int i = recv_objs; i < DELAY_TEST2_N;)
    {
        //  isleep(1);
        // ikcp_update����ikcp_flush��ikcp_flush�����Ͷ����е�����ͨ���²�Э��UDP���з���
        ikcp_update(send->pkcp, iclock()); //���ǵ���һ�����ξ������ã�Ҫloop����
        // ikcp_flush(send->pkcp);		// ����flush�ܸ����ٰѷ�Ƭ���ͳ�ȥ
        int n = recvfrom(send->sockfd, buf, UDP_RECV_BUF_SIZE, MSG_DONTWAIT, (struct sockaddr *)&send->addr, &len);
        // printf("recv2:%d\n", n);
        if (n < 0)
        { //����Ƿ���UDP���ݰ�
            // printf("recv2:%d\n", n);
            isleep(1);
            continue;
        }

        ret = ikcp_input(send->pkcp, buf, n);
        if (ret < 0) //���ikcp_input�Ƿ���ȡ������������
        {
            printf("ikcp_input2 ret = %d\n", ret);
            continue; // û�ж�ȡ������
        }
        ret = ikcp_recv(send->pkcp, (char *)&objs[i], obj_size);
        if (ret < 0) //���ikcp_recv��ȡ��������
        {
            // printf("ikcp_recv2 ret = %d\n",ret);
            continue;
        }
        printf("recv2 %d seqno:%d, ret:%d\n", recv_objs, objs[i].seqno, ret);
        delay_set_recv_time(&objs[recv_objs]);
        recv_objs++;
        i++;
        if (ret != obj_size)
        {
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
    int n, ret;

    // while(1)
    {
        isleep(1);
        delay_test2(send);
    }
    printf("loop finish\n");
    close(send->sockfd);
}

int main(int argc, char *argv[])
{
    // printf("this is kcpClient,����������� ip��ַ�Ͷ˿ںţ�\n");
    if (argc != 3)
    {
        printf("�����������ip��ַ�Ͷ˿ں�\n");
        return -1;
    }
    printf("this is kcpClient\n");
    int64_t cur = iclock64();
    printf("main started t:%ld\n", cur); // prints Hello World!!!
    unsigned char *ipstr = (unsigned char *)argv[1];
    unsigned char *port = (unsigned char *)argv[2];

    kcpObj send;
    send.ipstr = ipstr;
    send.port = atoi(argv[2]);

    init(&send); //��ʼ��send,��Ҫ�������������ͨ�ŵ��׽��ֶ���

    bzero(send.buff, sizeof(send.buff));

    // ÿ�����Ӷ�����Ҫ��Ӧһ��ikcpcb
    ikcpcb *kcp = ikcp_create(0x1, (void *)&send); //����kcp�����send����kcp��user����
    kcp->output = udp_output;                      //����kcp����Ļص�����
    ikcp_nodelay(kcp, 0, 10, 0, 0);                //(kcp1, 0, 10, 0, 0); 1, 10, 2, 1
    ikcp_wndsize(kcp, 128, 128);
    ikcp_setmtu(kcp, 1400);
    send.pkcp = kcp;
    loop(&send); //ѭ������
    ikcp_release(send.pkcp);
    printf("main finish t:%ldms\n", iclock64() - cur);
    return 0;
}