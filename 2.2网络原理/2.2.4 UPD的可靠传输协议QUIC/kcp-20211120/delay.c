#include "delay.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/time.h>
#include <sys/wait.h>

/* get system time */
void itimeofday(long *sec, long *usec)
{
	#if defined(__unix)
	struct timeval time;
	gettimeofday(&time, NULL);
	if (sec) *sec = time.tv_sec;
	if (usec) *usec = time.tv_usec;
	#else
	static long mode = 0, addsec = 0;
	int retval;
	static int64_t freq = 1;
	int64_t qpc;
	if (mode == 0) {
		retval = QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
		freq = (freq == 0)? 1 : freq;
		retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
		addsec = (long)time(NULL);
		addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
		mode = 1;
	}
	retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
	retval = retval * 2;
	if (sec) *sec = (long)(qpc / freq) + addsec;
	if (usec) *usec = (long)((qpc % freq) * 1000000 / freq);
	#endif
}

/* get clock in millisecond 64 */
int64_t iclock64(void)
{
	long s, u;
	int64_t value;
	itimeofday(&s, &u);
	value = ((int64_t)s) * 1000 + (u / 1000);
	return value;
}

uint32_t iclock()
{
	return (uint32_t)(iclock64() & 0xfffffffful);
}


inline t_delay_obj *delay_new() {
    t_delay_obj *obj =  (t_delay_obj *)malloc(sizeof(t_delay_obj));
    if(!obj) {
        return NULL;
    }
    obj->seqno = 0;
    obj->send_time = 0;
    obj->recv_time = 0;
}

inline void delay_set_seqno(t_delay_obj *obj, uint16_t seqno) {
    obj->seqno = seqno;
}

inline void delay_set_seqno_send_time(t_delay_obj *obj, uint16_t seqno) {
    obj->seqno = seqno;
    obj->send_time = iclock64();
}

inline void delay_set_send_time(t_delay_obj *obj) {
    obj->send_time = iclock64();
}

inline void delay_set_recv_time(t_delay_obj *obj) {
    obj->recv_time = iclock64();
}

inline void delay_print_rtt_time(t_delay_obj *objs, int num) {
    for(int i = 0; i < num; i++) {
        t_delay_obj *obj = &(objs[i]);
        printf("%04d seqno:%d rtt  :%ldms\n", i, obj->seqno, obj->recv_time - obj->send_time);
		// printf("%04d seqno:%d snd_t:%ldms\n", i, obj->seqno, obj->send_time);
		// printf("%04d seqno:%d rcv_t:%ldms\n", i, obj->seqno, obj->recv_time);
    }
}

