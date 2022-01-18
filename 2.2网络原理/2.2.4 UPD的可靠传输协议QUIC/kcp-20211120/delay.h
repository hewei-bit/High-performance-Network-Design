#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>



#define DELAY_BODY_SIZE 1300
typedef struct delay_obj
{
    uint16_t seqno; // 序列号
    int64_t send_time;  // 发送时间
    int64_t recv_time;  // 回来时间
    uint8_t body[DELAY_BODY_SIZE];
}t_delay_obj;

int64_t iclock64();
uint32_t iclock();
t_delay_obj *delay_new();
void delay_set_seqno(t_delay_obj *obj, uint16_t seqno);
void delay_set_seqno_send_time(t_delay_obj *obj, uint16_t seqno);
void delay_set_send_time(t_delay_obj *obj);
void delay_set_recv_time(t_delay_obj *obj);
void delay_print_rtt_time(t_delay_obj *objs, int num);

