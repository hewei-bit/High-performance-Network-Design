/**
 * @File Name: delay.h
 * @Brief :
 * @Author : hewei (hewei_1996@qq.com)
 * @Version : 1.0
 * @Creat Date : 2022-03-29
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define DELAY_BODY_SIZE 1300
typedef struct delay_obj
{
    uint16_t seqno;    // ���к�
    int64_t send_time; // ����ʱ��
    int64_t recv_time; // ����ʱ��
    uint8_t body[DELAY_BODY_SIZE];
} t_delay_obj;

int64_t iclock64();
uint32_t iclock();
t_delay_obj *delay_new();
void delay_set_seqno(t_delay_obj *obj, uint16_t seqno);
void delay_set_seqno_send_time(t_delay_obj *obj, uint16_t seqno);
void delay_set_send_time(t_delay_obj *obj);
void delay_set_recv_time(t_delay_obj *obj);
void delay_print_rtt_time(t_delay_obj *objs, int num);
