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

    struct sockaddr_in addr;      //��ŷ�������Ϣ�Ľṹ��
    struct sockaddr_in CientAddr; //��ſͻ�����Ϣ�Ľṹ��

    char buff[RECV_BUF]; //����շ�����Ϣ

} kcpObj;
// ����:  gcc -o server server.c ikcp.c
// �ر���Ҫע�⣬����ķ�������Ҳֻ��һ��ʹ�ã����ǵȿͻ����˳��󣬷����ҲҪֹͣ��������
// ֮��������������Ҫ����Ϊsn�����⣬����ͻ��˵�һ������ sn 0~5�� �ڶ����������͵�sn����0 ~5 ����������˲�ֹͣ���Լ���Ϊ0~5�Ѿ��յ����˾Ͳ���ظ���

// ������ʹ�õ�ʱ�򣬻���Ҫ�����ͨ���ÿͻ��˺ͷ�������֮ǰ���´���ikcpcb����ƥ��ikcpcb��conv
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

    //������Ϣ
    int n = sendto(send->sockfd, buf, len, 0, (struct sockaddr *)&send->CientAddr, sizeof(struct sockaddr_in));
    if (n >= 0)
    {
        //���ظ����ͣ������������
        printf("send: %d bytes, t:%lld\n", n, iclock64() - first_recv_time); // 24�ֽڵ�KCPͷ��
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
        perror("socket error��");
        exit(1);
    }

    bzero(&send->addr, sizeof(send->addr));

    send->addr.sin_family = AF_INET;
    send->addr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY
    send->addr.sin_port = htons(send->port);

    printf("������socket: %d  port:%d\n", send->sockfd, send->port);

    if (send->sockfd < 0)
    {
        perror("socket error��");
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
    //���յ���һ�����Ϳ�ʼѭ������
    int recv_count = 0;

    isleep(1);
    ikcp_update(send->pkcp, iclock());

    char buf[RECV_BUF] = {0};

    while (1)
    {
        isleep(1);
        ikcp_update(send->pkcp, iclock());
        //��������Ϣ
        n = recvfrom(send->sockfd, buf, RECV_BUF, MSG_DONTWAIT, (struct sockaddr *)&send->CientAddr, &len);
        if (n > 0)
        {
            printf("UDP recv[%d]  size= %d   \n", recv_count++, n);
            if (first_recv_time == 0)
            {
                first_recv_time = iclock64();
            }
            //Ԥ��������:����ikcp_input�������ݽ���KCP����Щ�����п�����KCP���Ʊ��ģ�����������Ҫ�����ݡ�
            // kcp���յ��²�Э��UDP�����������ݵײ�����bufferת����kcp�����ݰ���ʽ
            ret = ikcp_input(send->pkcp, buf, n);
            if (ret < 0)
            {
                continue;
            }
            // kcp�����յ���kcp���ݰ���ԭ��֮ǰkcp���͵�buffer����
            ret = ikcp_recv(send->pkcp, buf, n); //�� buf�� ��ȡ�������ݣ�������ȡ�������ݴ�С
            if (ret < 0)
            { // û�м��ikcp_recv��ȡ��������
                isleep(1);
                continue;
            }
            int send_size = ret;
            // ikcp_sendֻ�ǰ����ݴ��뷢�Ͷ��У�û�ж����ݼӷ�kcpͷ������
            //Ӧ������kcp_update����ӷ�kcpͷ������
            // ikcp_send��Ҫ���͵�buffer��Ƭ��KCP�����ݰ���ʽ����������Ͷ����С�
            ret = ikcp_send(send->pkcp, buf, send_size);
            printf("Server reply ->  bytes[%d], ret = %d\n", send_size, ret);
            ikcp_flush(send->pkcp); // ����flushһ�� �Ը����ÿͻ����յ�����
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
        printf("������������˿ں�\n");
        return -1;
    }

    kcpObj send;
    send.port = atoi(argv[1]);
    send.pkcp = NULL;

    bzero(send.buff, sizeof(send.buff));
    char Msg[] = "Server:Hello!"; //��ͻ�����������
    memcpy(send.buff, Msg, sizeof(Msg));

    ikcpcb *kcp = ikcp_create(0x1, (void *)&send); //����kcp�����send����kcp��user����
    ikcp_setmtu(kcp, 1400);
    kcp->output = udp_output;       //����kcp����Ļص�����
    ikcp_nodelay(kcp, 0, 10, 0, 0); // 1, 10, 2, 1
    ikcp_wndsize(kcp, 128, 128);

    send.pkcp = kcp;

    init(&send); //��������ʼ���׽���
    loop(&send); //ѭ������

    return 0;
}