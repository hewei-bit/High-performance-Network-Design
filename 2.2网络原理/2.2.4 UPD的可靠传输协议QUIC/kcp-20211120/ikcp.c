//=====================================================================
//
// KCP - A Better ARQ Protocol Implementation
// skywind3000 (at) gmail.com, 2010-2011
//  
// Features:
// + Average RTT reduce 30% - 40% vs traditional ARQ like tcp.
// + Maximum RTT reduce three times vs tcp.
// + Lightweight, distributed as a single source file.
//
//=====================================================================
#include "ikcp.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>



//=====================================================================
// KCP BASIC
//=====================================================================
const IUINT32 IKCP_RTO_NDL = 30;		// rto: no delay min rto 无延迟的最小重传超时时间
const IUINT32 IKCP_RTO_MIN = 100;		// rto: normal min rto 正常模式最小超时重传
const IUINT32 IKCP_RTO_DEF = 200;       // rto: 默认超时重传
const IUINT32 IKCP_RTO_MAX = 60000;     // rto: 最大超时超时
const IUINT32 IKCP_CMD_PUSH = 81;		// cmd: push data   协议类型 [正常接收数据]
const IUINT32 IKCP_CMD_ACK  = 82;		// cmd: ack         协议类型 [收到ack回复]
const IUINT32 IKCP_CMD_WASK = 83;		// cmd: window probe (ask)  协议类型 [询问对方窗口size]
const IUINT32 IKCP_CMD_WINS = 84;		// cmd: window size (tell)  协议类型 [告知对方我的窗口size]
const IUINT32 IKCP_ASK_SEND = 1;		// need to send IKCP_CMD_WASK   是否需要发送 IKCP_CMD_WASK
const IUINT32 IKCP_ASK_TELL = 2;		// need to send IKCP_CMD_WINS   是否需要发送 IKCP_CMD_WINS
const IUINT32 IKCP_WND_SND = 32;        // 发送队列滑动窗口最大值 
const IUINT32 IKCP_WND_RCV = 128;       // 接收队列滑动窗口最大值 must >= max fragment size
const IUINT32 IKCP_MTU_DEF = 1400;      // segment: 报文默认大小 [mtu 网络最小传输单元]
const IUINT32 IKCP_ACK_FAST	= 3;        // null: 没有被用使用
const IUINT32 IKCP_INTERVAL	= 100;      // flush: 控制刷新时间间隔
const IUINT32 IKCP_OVERHEAD = 24;       // segment: 报文默认大小 [mtu 网络最小传输单元]
const IUINT32 IKCP_DEADLINK = 20;
const IUINT32 IKCP_THRESH_INIT = 2;     // ssthresh: 慢热启动 初始窗口大小
const IUINT32 IKCP_THRESH_MIN = 2;      // ssthresh: 慢热启动 最小窗口大小
const IUINT32 IKCP_PROBE_INIT = 7000;		// 7 secs to probe window size  请求询问远端窗口大小的初始时间
const IUINT32 IKCP_PROBE_LIMIT = 120000;	// up to 120 secs to probe window   请求询问远端窗口大小的最大时间
const IUINT32 IKCP_FASTACK_LIMIT = 5;		// max times to trigger fastack


//---------------------------------------------------------------------
// encode / decode
//---------------------------------------------------------------------

/* encode 8 bits unsigned int */
static inline char *ikcp_encode8u(char *p, unsigned char c)
{
	*(unsigned char*)p++ = c;
	return p;
}

/* decode 8 bits unsigned int */
static inline const char *ikcp_decode8u(const char *p, unsigned char *c)
{
	*c = *(unsigned char*)p++;
	return p;
}

/* encode 16 bits unsigned int (lsb) */
static inline char *ikcp_encode16u(char *p, unsigned short w)
{
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
	*(unsigned char*)(p + 0) = (w & 255);
	*(unsigned char*)(p + 1) = (w >> 8);
#else
	memcpy(p, &w, 2);
#endif
	p += 2;
	return p;
}

/* decode 16 bits unsigned int (lsb) */
static inline const char *ikcp_decode16u(const char *p, unsigned short *w)
{
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
	*w = *(const unsigned char*)(p + 1);
	*w = *(const unsigned char*)(p + 0) + (*w << 8);
#else
	memcpy(w, p, 2);
#endif
	p += 2;
	return p;
}

/* encode 32 bits unsigned int (lsb) */
static inline char *ikcp_encode32u(char *p, IUINT32 l)
{
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
	*(unsigned char*)(p + 0) = (unsigned char)((l >>  0) & 0xff);
	*(unsigned char*)(p + 1) = (unsigned char)((l >>  8) & 0xff);
	*(unsigned char*)(p + 2) = (unsigned char)((l >> 16) & 0xff);
	*(unsigned char*)(p + 3) = (unsigned char)((l >> 24) & 0xff);
#else
	memcpy(p, &l, 4);
#endif
	p += 4;
	return p;
}

/* decode 32 bits unsigned int (lsb) */
static inline const char *ikcp_decode32u(const char *p, IUINT32 *l)
{
#if IWORDS_BIG_ENDIAN || IWORDS_MUST_ALIGN
	*l = *(const unsigned char*)(p + 3);
	*l = *(const unsigned char*)(p + 2) + (*l << 8);
	*l = *(const unsigned char*)(p + 1) + (*l << 8);
	*l = *(const unsigned char*)(p + 0) + (*l << 8);
#else 
	memcpy(l, p, 4);
#endif
	p += 4;
	return p;
}

static inline IUINT32 _imin_(IUINT32 a, IUINT32 b) {
	return a <= b ? a : b;
}

static inline IUINT32 _imax_(IUINT32 a, IUINT32 b) {
	return a >= b ? a : b;
}

static inline IUINT32 _ibound_(IUINT32 lower, IUINT32 middle, IUINT32 upper) 
{
	return _imin_(_imax_(lower, middle), upper);
}

static inline long _itimediff(IUINT32 later, IUINT32 earlier) 
{
	return ((IINT32)(later - earlier));
}

//---------------------------------------------------------------------
// manage segment
//---------------------------------------------------------------------
typedef struct IKCPSEG IKCPSEG;

static void* (*ikcp_malloc_hook)(size_t) = NULL;
static void (*ikcp_free_hook)(void *) = NULL;

// internal malloc
static void* ikcp_malloc(size_t size) {
	if (ikcp_malloc_hook) 
		return ikcp_malloc_hook(size);
	return malloc(size);
}

// internal free
static void ikcp_free(void *ptr) {
	if (ikcp_free_hook) {
		ikcp_free_hook(ptr);
	}	else {
		free(ptr);
	}
}

// redefine allocator
void ikcp_allocator(void* (*new_malloc)(size_t), void (*new_free)(void*))
{
	ikcp_malloc_hook = new_malloc;
	ikcp_free_hook = new_free;
}

// allocate a new kcp segment
static IKCPSEG* ikcp_segment_new(ikcpcb *kcp, int size)
{
	return (IKCPSEG*)ikcp_malloc(sizeof(IKCPSEG) + size);
}

// delete a segment
static void ikcp_segment_delete(ikcpcb *kcp, IKCPSEG *seg)
{
	ikcp_free(seg);
}

// write log
void ikcp_log(ikcpcb *kcp, int mask, const char *fmt, ...)
{
	char buffer[1024];
	va_list argptr;
	if ((mask & kcp->logmask) == 0 || kcp->writelog == 0) return;
	va_start(argptr, fmt);
	vsprintf(buffer, fmt, argptr);
	va_end(argptr);
	kcp->writelog(buffer, (struct IKCPCB  *)kcp, kcp->user);
}

// check log mask
static int ikcp_canlog(const ikcpcb *kcp, int mask)
{
	if ((mask & kcp->logmask) == 0 || kcp->writelog == NULL) return 0;
	return 1;
}
// output segment
static int ikcp_output(ikcpcb *kcp, const void *data, int size)
{
	assert(kcp);
	assert(kcp->output);
	if (ikcp_canlog(kcp, IKCP_LOG_OUTPUT)) {
		ikcp_log(kcp, IKCP_LOG_OUTPUT, "[RO] %ld bytes", (long)size);
	}
	if (size == 0) return 0;
	return kcp->output((const char*)data, size, kcp, kcp->user);
}

// output queue
void ikcp_qprint(const char *name, const struct IQUEUEHEAD *head)
{
#if 0
	const struct IQUEUEHEAD *p;
	printf("<%s>: [", name);
	for (p = head->next; p != head; p = p->next) {
		const IKCPSEG *seg = iqueue_entry(p, const IKCPSEG, node);
		printf("(%lu %d)", (unsigned long)seg->sn, (int)(seg->ts % 10000));
		if (p->next != head) printf(",");
	}
	printf("]\n");
#endif
}
//---------------------------------------------------------------------
// create a new kcpcb 创建kcp对象，然后进行初始化
//---------------------------------------------------------------------
ikcpcb* ikcp_create(IUINT32 conv, void *user)
{
	ikcpcb *kcp = (ikcpcb*)ikcp_malloc(sizeof(struct IKCPCB));
	if (kcp == NULL) return NULL;
	kcp->conv = conv;
	kcp->user = user;
	kcp->snd_una = 0;
	kcp->snd_nxt = 0;
	kcp->rcv_nxt = 0;
	kcp->ts_recent = 0;
	kcp->ts_lastack = 0;
	kcp->ts_probe = 0;
	kcp->probe_wait = 0;
	kcp->snd_wnd = IKCP_WND_SND;	// 默认发送的窗口
	kcp->rcv_wnd = IKCP_WND_RCV;	// 默认接收的窗口
	kcp->rmt_wnd = IKCP_WND_RCV;	// 默认远端的接收窗口 
	kcp->cwnd = 0;		// 拥塞动态窗口
	kcp->incr = 0;
	kcp->probe = 0;		// 当出现拥塞的时候，我们就可能要探测窗口
	kcp->mtu = IKCP_MTU_DEF;	// 缺省的mtu大小
	kcp->mss = kcp->mtu - IKCP_OVERHEAD;	// mtu - 分片 header头部占用字节
	kcp->stream = 0;

	kcp->buffer = (char*)ikcp_malloc((kcp->mtu + IKCP_OVERHEAD) * 3);	// 消息字节流, 类似tcp方式使用
	if (kcp->buffer == NULL) {
		ikcp_free(kcp);
		return NULL;
	}

	iqueue_init(&kcp->snd_queue);
	iqueue_init(&kcp->rcv_queue);
	iqueue_init(&kcp->snd_buf);
	iqueue_init(&kcp->rcv_buf);
	kcp->nrcv_buf = 0;	// 队列 缓存的数量
	kcp->nsnd_buf = 0;
	kcp->nrcv_que = 0;
	kcp->nsnd_que = 0;
	kcp->state = 0;	
	kcp->acklist = NULL; // 需要应答的序号
	kcp->ackblock = 0;
	kcp->ackcount = 0;		
	kcp->rx_srtt = 0;
	kcp->rx_rttval = 0;
	kcp->rx_rto = IKCP_RTO_DEF;		// 缺省的超时重传时间
	kcp->rx_minrto = IKCP_RTO_MIN;	// 最小重传时间
	kcp->current = 0;		// 当前时间
	kcp->interval = IKCP_INTERVAL;	// 缺省刷新循环的时间间隔
	kcp->ts_flush = IKCP_INTERVAL;
	kcp->nodelay = 0;		// 无延迟是否开启
	kcp->updated = 0;		// 是否调用过ikcp_update
	kcp->logmask = 0;
	kcp->ssthresh = IKCP_THRESH_INIT;	// 窗口启动初始化
	kcp->fastresend = 0;		// 触发ack的数量, 本质是跳过了多少个ack就重传
	kcp->fastlimit = IKCP_FASTACK_LIMIT;
	kcp->nocwnd = 0;		// 是否开启拥塞控制
	kcp->xmit = 0;		// 只是计数器
	kcp->dead_link = IKCP_DEADLINK;	// 重传的最大次数, 如果达到则认为链路断开
	kcp->output = NULL;		// udp输出函数
	kcp->writelog = NULL;		// 可以设置外部的log

	return kcp;
}

//---------------------------------------------------------------------
// release a new kcpcb 释放一个kcp对象
//---------------------------------------------------------------------
void ikcp_release(ikcpcb *kcp)
{
	assert(kcp);
	if (kcp) {
		IKCPSEG *seg;
		while (!iqueue_is_empty(&kcp->snd_buf)) {
			seg = iqueue_entry(kcp->snd_buf.next, IKCPSEG, node);
			iqueue_del(&seg->node);
			ikcp_segment_delete(kcp, seg);
		}
		while (!iqueue_is_empty(&kcp->rcv_buf)) {
			seg = iqueue_entry(kcp->rcv_buf.next, IKCPSEG, node);
			iqueue_del(&seg->node);
			ikcp_segment_delete(kcp, seg);
		}
		while (!iqueue_is_empty(&kcp->snd_queue)) {
			seg = iqueue_entry(kcp->snd_queue.next, IKCPSEG, node);
			iqueue_del(&seg->node);
			ikcp_segment_delete(kcp, seg);
		}
		while (!iqueue_is_empty(&kcp->rcv_queue)) {
			seg = iqueue_entry(kcp->rcv_queue.next, IKCPSEG, node);
			iqueue_del(&seg->node);
			ikcp_segment_delete(kcp, seg);
		}
		if (kcp->buffer) {
			ikcp_free(kcp->buffer);
		}
		if (kcp->acklist) {
			ikcp_free(kcp->acklist);
		}

		kcp->nrcv_buf = 0;
		kcp->nsnd_buf = 0;
		kcp->nrcv_que = 0;
		kcp->nsnd_que = 0;
		kcp->ackcount = 0;
		kcp->buffer = NULL;
		kcp->acklist = NULL;
		ikcp_free(kcp);
	}
}

//---------------------------------------------------------------------
// set output callback, which will be invoked by kcp 设置输出函数回调
//---------------------------------------------------------------------
void ikcp_setoutput(ikcpcb *kcp, int (*output)(const char *buf, int len,
	ikcpcb *kcp, void *user))
{
	kcp->output = output;
}

//---------------------------------------------------------------------
// user/upper level recv: returns size, returns below zero for EAGAIN
// 读取组好的数据
//---------------------------------------------------------------------
/*
主要做三件事情:
1. 读取组好包的数据
2. 将接收缓存rcv_buf的分片转移到接收队列rcv_queue
3. 如果有接收空间则将kcp->probe |= IKCP_ASK_TELL; 以在update的时候告知对方可以发送数据了。
*/
int ikcp_recv(ikcpcb *kcp, char *buffer, int len)
{
	struct IQUEUEHEAD *p;
	int ispeek = (len < 0)? 1 : 0;	// peek:窥视; 偷看. 如果是 ispeek 说明只是为了拿数据看看，则不用将数据从queue删除
	int peeksize;
	int recover = 0;
	IKCPSEG *seg;
	assert(kcp);
	// 排序好的数据存放在rcv_queue, 反过来待发送的数据则存放在snd_queue
	if (iqueue_is_empty(&kcp->rcv_queue))   // 如果为空则没有数据可读
		return -1;

	if (len < 0) len = -len;
    //计算当前接收队列中的属于同一个消息的数据总长度(不是所有消息的总长度)，注意这里的同一个消息是seg->frg进行标记
	peeksize = ikcp_peeksize(kcp); // 当我们没有采用流式传输的时候, 我们接收的则是类似 udp一样的报文传输方式

	if (peeksize < 0)  // 没有数据可读
		return -2;

	if (peeksize > len)     // 可读数据大于 用户传入的长度，每次读取需要一次性读取完毕，类似udp报文的读取
		return -3;
    // KCP 协议在远端窗口为0的时候将会停止发送数据
	if (kcp->nrcv_que >= kcp->rcv_wnd)  // 接收队列segment数量大于等于接收窗口，标记窗口可以恢复
		recover = 1;                     // 标记可以开始窗口恢复

	// merge fragment   将属于同一个消息的各分片重组完整数据，并删除rcv_queue中segment，nrcv_que减少 
    // 经过 ikcp_send 发送的数据会进行分片，分片编号为倒序序号，因此 frg 为 0 的数据包标记着完整接收到了一次 send 发送过来的数据
	for (len = 0, p = kcp->rcv_queue.next; p != &kcp->rcv_queue; ) {
		int fragment;
		seg = iqueue_entry(p, IKCPSEG, node);
		p = p->next;

		if (buffer) {
			memcpy(buffer, seg->data, seg->len);	// 把queue的数据就放入用户buffer
			buffer += seg->len;
		}

		len += seg->len;
		fragment = seg->frg;

		if (ikcp_canlog(kcp, IKCP_LOG_RECV)) {
			ikcp_log(kcp, IKCP_LOG_RECV, "recv sn=%lu", (unsigned long)seg->sn);
		}

		if (ispeek == 0) {
			iqueue_del(&seg->node);
			ikcp_segment_delete(kcp, seg);  // 删除节点
			kcp->nrcv_que--;    // nrcv_que接收队列-1
		}

		if (fragment == 0) 	// 完整的数据接收到,  send的时候如果数据被分片，比如分成 n个片，则0 .. n-2的fragment为1，n-1的为0
			break;
	}

	assert(len == peeksize);

	// move available data from rcv_buf -> rcv_queue
    // 将合适的数据从接收缓存rcv_buf 转移到接收队列rev_queue, 已经确认的以及接收窗口未满
	while (! iqueue_is_empty(&kcp->rcv_buf)) {
		seg = iqueue_entry(kcp->rcv_buf.next, IKCPSEG, node);
        // 条件1 序号是该接收的数据
        // 条件2 接收队列nrcv_que < 接收窗口rcv_wnd; 
		if (seg->sn == kcp->rcv_nxt && kcp->nrcv_que < kcp->rcv_wnd) {
			iqueue_del(&seg->node);
			kcp->nrcv_buf--;
			iqueue_add_tail(&seg->node, &kcp->rcv_queue);
			kcp->nrcv_que++;    // 接收队列 有多少个分片 + 1
			kcp->rcv_nxt++;     // 接收序号 + 1
		}	else {
			break;
		}
	}

	// fast recover ，nrcv_que小于rcv_wnd, 说明接收端有空间继续接收数据了
	if (kcp->nrcv_que < kcp->rcv_wnd && recover) {
		// ready to send back IKCP_CMD_WINS in ikcp_flush
		// tell remote my window size
		kcp->probe |= IKCP_ASK_TELL;
	}

	return len;
}

//---------------------------------------------------------------------
// peek data size 计算当前一帧数据的总大小(一个或多个分片组成的数据帧)
//---------------------------------------------------------------------
int ikcp_peeksize(const ikcpcb *kcp)
{
	struct IQUEUEHEAD *p;
	IKCPSEG *seg;
	int length = 0;

	assert(kcp);

	if (iqueue_is_empty(&kcp->rcv_queue)) return -1;

	seg = iqueue_entry(kcp->rcv_queue.next, IKCPSEG, node);
	if (seg->frg == 0) return seg->len;

	if (kcp->nrcv_que < seg->frg + 1) return -1;

	for (p = kcp->rcv_queue.next; p != &kcp->rcv_queue; p = p->next) {
		seg = iqueue_entry(p, IKCPSEG, node);
		length += seg->len; // 
		if (seg->frg == 0) break;
	}

	return length;
}
//---------------------------------------------------------------------
// user/upper level send, returns below zero for error
//---------------------------------------------------------------------
/*
把用户发送的数据根据MSS(max segment size)分片成KCP的数据分片格式，插入待发送队列中。当用户的数据超过一个MSS(最大分片大小)
的时候，会对发送的数据进行分片处理。通过frg进行排序区分，frg即message中的segment分片ID，在message中的索引，由大到小，
0表示最后一个分片。分成4片时，frg为3,2,1,0。
如用户发送2900字节的数据，MSS为1400byte。因此，该函数会把1900byte的用户数据分成两个分片，一个数据大小为1400，头frg设置为2，
len设置为1400；第二个分片，头frg设置为1，len设置为1400; 第三个分片, 头frg设置为0，len设置为100。
切好KCP分片之后，放入到名为snd_queue的待发送队列中。
*/

/*分片方式共有两种。
(1) 流模式情况下，检测每个发送队列里的分片是否达到最大MSS，如果没有达到就会用新的数据填充分片。
接收端会把多片发送的数据重组为一个完整的KCP帧。
(2)消息模式下，将用户数据分片，为每个分片设置sn和frag，将分片后的数据一个一个地存入发送队列，
接收方通过sn和frag解析原来的包，消息方式一个分片的数据量可能不能达到MSS，也会作为一个包发送出去。*/
/*
不能一下send太长的数据, 当数据长度/mss大于对方接收窗口的时候则返回错误
*/
int ikcp_send(ikcpcb *kcp, const char *buffer, int len)
{
	IKCPSEG *seg;
	int count, i;

	assert(kcp->mss > 0);       // 从mtu
	if (len < 0) return -1;

	// append to previous segment in streaming mode (if possible)
    // 1 如果当前的 KCP 开启流模式，取出 `snd_queue` 中的最后一个报文 将其填充到 mss 的长度，并设置其 frg 为 0.
	if (kcp->stream != 0) {		// 先不考虑流式的
		if (!iqueue_is_empty(&kcp->snd_queue)) {
			IKCPSEG *old = iqueue_entry(kcp->snd_queue.prev, IKCPSEG, node);
            /*节点内数据长度小于mss，计算还可容纳的数据大小，以及本次占用的空间大小，
             以此新建segment，将新建segment附加到发送队列尾，将old节点内数据拷贝过去，
             然后将buffer中也拷贝其中，如果buffer中的数据没有拷贝完，extend为拷贝数据，
             开始frg计数。更新len为剩余数据，删除old */ 
			if (old->len < kcp->mss) {
				int capacity = kcp->mss - old->len;
				int extend = (len < capacity)? len : capacity;
				seg = ikcp_segment_new(kcp, old->len + extend);
				assert(seg);
				if (seg == NULL) {
					return -2;
				}
				iqueue_add_tail(&seg->node, &kcp->snd_queue);   // 重新一个新segment加入
				memcpy(seg->data, old->data, old->len);
				if (buffer) {
					memcpy(seg->data + old->len, buffer, extend);
					buffer += extend;
				}
				seg->len = old->len + extend;
				seg->frg = 0;
				len -= extend;
				iqueue_del_init(&old->node);
				ikcp_segment_delete(kcp, old);
			}
		}
		if (len <= 0) {
			return 0;
		}
	}
    // 2 计算数据可以被最多分成多少个frag  
	if (len <= (int)kcp->mss) count = 1;		// 分片(头部24) + user data(mss 1376) = mtu （1400）
	else count = (len + kcp->mss - 1) / kcp->mss;

	if (count >= (int)IKCP_WND_RCV) return -2;  // 超过对方的初始接收窗口
	// 这里的设计有个疑问，如果一致send则snd_queue一致增长, 如果去也去判断主机的发送窗口大小可能更优些
	if (count == 0) count = 1;  // ?

	// fragment
    // 3 将数据全部新建segment插入发送队列尾部，队列计数递增, frag递减
	for (i = 0; i < count; i++) {
		int size = len > (int)kcp->mss ? (int)kcp->mss : len;
		seg = ikcp_segment_new(kcp, size);
		assert(seg);
		if (seg == NULL) {
			return -2;
		}
		if (buffer && len > 0) {
			memcpy(seg->data, buffer, size);	// 拷贝数据
		}
		seg->len = size;                                    // 每seg 数据大小
		seg->frg = (kcp->stream == 0)? (count - i - 1) : 0; // frg编号 , 流模式情况下分片编号不用填写
		iqueue_init(&seg->node);
		iqueue_add_tail(&seg->node, &kcp->snd_queue);	// 发送队列
		kcp->nsnd_que++;           	 // 发送队列++
		if (buffer) {
			buffer += size;
		}
		len -= size;
	}

	return 0;
}

//---------------------------------------------------------------------
// parse ack
//---------------------------------------------------------------------
/*
RTT: Round Trip Time，也就是一个数据包从发出去到回来的时间.
    这样发送端就大约知道需要多少的时间，从而可以方便地设置Timeout——RTO（Retransmission TimeOut）
RTO: (Retransmission TimeOut)即重传超时时间
rx_srtt: smoothed round trip time，平滑后的RTT
rx_rttval：RTT的变化量，代表连接的抖动情况
interval：内部flush刷新间隔，对系统循环效率有非常重要影响

作用：更新RTT和RTO等参数，该算法与TCP保持一致:
1. 第一次测量，rtt 是我们测量的结果，rx_srtt = rtt，rx_rttval = rtt / 2
2. 以后每次测量：
    rx_srtt =(1-a) * rx_srtt + a * rtt，a取值1/8
    rx_rttval= (1-b) * rx_rttval + b * |rtt - rx_srtt|，b取值1/4
    rto = rx_srtt + 4 * rx_rttval
    rx_rto = MIN(MAX(rx_minrto, rto), IKCP_RTO_MAX)
*/
static void ikcp_update_ack(ikcpcb *kcp, IINT32 rtt)
{
	IINT32 rto = 0;
	if (kcp->rx_srtt == 0) {    //rx_srtt初始为0时
		kcp->rx_srtt = rtt;     
		kcp->rx_rttval = rtt / 2;
	}	else {
		long delta = rtt - kcp->rx_srtt;    //计算这次和之前的差值
		if (delta < 0) delta = -delta;
		kcp->rx_rttval = (3 * kcp->rx_rttval + delta) / 4;  //权重计算 rtt的变化量
		kcp->rx_srtt = (7 * kcp->rx_srtt + rtt) / 8;    // 计算平滑后的rtt
		if (kcp->rx_srtt < 1) kcp->rx_srtt = 1;
	}
	/*! 通过抖动情况与内部调度间隔计算出RTO时间 */
	rto = kcp->rx_srtt + _imax_(kcp->interval, 4 * kcp->rx_rttval);
	/*! 使得最后结果在minrto <= x <=  IKCP_RTO_MAX 之间 */
	kcp->rx_rto = _ibound_(kcp->rx_minrto, rto, IKCP_RTO_MAX);
}

// 更新下一个需要对方应答的序号 snd_una
static void ikcp_shrink_buf(ikcpcb *kcp)
{
    // 更新未确认的segment的序号
	struct IQUEUEHEAD *p = kcp->snd_buf.next; 
	if (p != &kcp->snd_buf) {/*! 判断发送队列不为空，与iqueue_is_empty一个意思 */
		IKCPSEG *seg = iqueue_entry(p, IKCPSEG, node);
		kcp->snd_una = seg->sn;
	}	else {
		kcp->snd_una = kcp->snd_nxt;
	}
}
// 该函数主要工作从发送buf中删除相应编号的分片
static void ikcp_parse_ack(ikcpcb *kcp, IUINT32 sn)
{
	struct IQUEUEHEAD *p, *next;
	// 当前确认数据包ack的编号小于已经接收到的编号(una)或数据包的ack编号大于待分配的编号则不合法
	if (_itimediff(sn, kcp->snd_una) < 0 || _itimediff(sn, kcp->snd_nxt) >= 0)
		return;
	// 遍历发送队列释放该编号分片，已经确认就可以删除了
	for (p = kcp->snd_buf.next; p != &kcp->snd_buf; p = next) {
		IKCPSEG *seg = iqueue_entry(p, IKCPSEG, node);
		next = p->next;
		if (sn == seg->sn) {
			iqueue_del(p);
			ikcp_segment_delete(kcp, seg);
			kcp->nsnd_buf--;
			break;
		}
		// 如果该编号小于则表明不需要继续遍历下去，因为队列里面的序号是递增的
		if (_itimediff(sn, seg->sn) < 0) {
			break;
		}
	}
}

// una前的序号都已经收到，所以将对方已经确认收到的数据从发送缓存删除
// 确定已经发送的数据包有哪些被对方接收到
static void ikcp_parse_una(ikcpcb *kcp, IUINT32 una)
{
	struct IQUEUEHEAD *p, *next;
	for (p = kcp->snd_buf.next; p != &kcp->snd_buf; p = next) {
		IKCPSEG *seg = iqueue_entry(p, IKCPSEG, node);
		next = p->next;
		/*! 发送队列满足sn由大到小的顺序规则 */
		if (_itimediff(una, seg->sn) > 0) {
			iqueue_del(p);
			ikcp_segment_delete(kcp, seg);
			kcp->nsnd_buf--;
		}	else {
			break;
		}
	}
}
// 解析fastack
static void ikcp_parse_fastack(ikcpcb *kcp, IUINT32 sn, IUINT32 ts)
{
	struct IQUEUEHEAD *p, *next;
	// 当前确认数据包ack的编号小于已经接收到的编号(una)或数据包的ack编号大于待分配的编号则不合法
	// 说明已经发过了
	if (_itimediff(sn, kcp->snd_una) < 0 || _itimediff(sn, kcp->snd_nxt) >= 0)
		return;
	// 遍历发送buf，进行快速确认(ack)
	for (p = kcp->snd_buf.next; p != &kcp->snd_buf; p = next) {
		IKCPSEG *seg = iqueue_entry(p, IKCPSEG, node);
		next = p->next;
		if (_itimediff(sn, seg->sn) < 0) {
			break;
		}
		else if (sn != seg->sn) {
		#ifndef IKCP_FASTACK_CONSERVE
			seg->fastack++;
		#else
			if (_itimediff(ts, seg->ts) >= 0)
				seg->fastack++;
		#endif
		}
	}
}
/**
 * ts: message发送时刻的时间戳
 * sn: 分片编号
 * ackcount: acklist中ack的数量，每个ack在acklist中存储ts，sn两个量
 * ackblock: 2的倍数，标识acklist最大可容纳的ack数量
 * 该函数主要用与添加ack确认数据包信息
 */
//---------------------------------------------------------------------
// ack append 注意这里的append，意思是先把要sn的序号收集起来
//---------------------------------------------------------------------
static void ikcp_ack_push(ikcpcb *kcp, IUINT32 sn, IUINT32 ts)
{
	IUINT32 newsize = kcp->ackcount + 1;
	IUINT32 *ptr;

	if (newsize > kcp->ackblock) {//判断是否超出acklist可容纳的数量
		IUINT32 *acklist;
		IUINT32 newblock;
		//进行扩容，以8的N次方扩充
		for (newblock = 8; newblock < newsize; newblock <<= 1);
		//分配数组
		acklist = (IUINT32*)ikcp_malloc(newblock * sizeof(IUINT32) * 2);

		if (acklist == NULL) {
			assert(acklist != NULL);
			abort();
		}
		/*! 不为空则需要copy */
		if (kcp->acklist != NULL) {
			IUINT32 x;
			for (x = 0; x < kcp->ackcount; x++) {
				acklist[x * 2 + 0] = kcp->acklist[x * 2 + 0];
				acklist[x * 2 + 1] = kcp->acklist[x * 2 + 1];
			}
			ikcp_free(kcp->acklist);	//释放旧数据
		}

		kcp->acklist = acklist;	//数组赋值
		kcp->ackblock = newblock;	// --> 容量赋值
	}

	ptr = &kcp->acklist[kcp->ackcount * 2];	//进行数组下标偏移
	ptr[0] = sn;
	ptr[1] = ts;
	kcp->ackcount++;	 //增加数量
}
//  sn:message分片segment的序号,ts:message发送时刻的时间戳
// 通过下标偏移获取相应位置p上的ack确认包数据信息
static void ikcp_ack_get(const ikcpcb *kcp, int p, IUINT32 *sn, IUINT32 *ts)
{
	if (sn) sn[0] = kcp->acklist[p * 2 + 0];
	if (ts) ts[0] = kcp->acklist[p * 2 + 1];
}


//---------------------------------------------------------------------
// 该函数主要将数据分片追加到buf中，并将buf中数据有序的移置接收队列中
// 1. 先检测该分片是否需要进行接收处理
// 2.将分片放入rcv_buf
// 3.将已经确认的rcv_buf里面的分片放入rcv_queue
//---------------------------------------------------------------------
void ikcp_parse_data(ikcpcb *kcp, IKCPSEG *newseg)
{
	struct IQUEUEHEAD *p, *prev;
	IUINT32 sn = newseg->sn;
	int repeat = 0;             // 接收也有窗口大小 rcv_nxt =4, rcv_wnd =30, sn > 34
	// 判断该数据分片的编号是否超出接收窗口可接收的范围，或者该编号小于需要的则直接丢弃
	if (_itimediff(sn, kcp->rcv_nxt + kcp->rcv_wnd) >= 0 || //  检测序号是否超过 缓存可以容纳的
		_itimediff(sn, kcp->rcv_nxt) < 0) {   				// 检测是否是过期的数据 rcv_wnd = 4 
		ikcp_segment_delete(kcp, newseg);                   // 不在窗口内的都删除
		return;
	}
	// 在接收buf中寻找编号为sn的分片用来判断是否重复
	for (p = kcp->rcv_buf.prev; p != &kcp->rcv_buf; p = prev) {
		IKCPSEG *seg = iqueue_entry(p, IKCPSEG, node);
		prev = p->prev;
		if (seg->sn == sn) {        // 重复的包 检测序列化
			repeat = 1;
			break;
		}
		if (_itimediff(sn, seg->sn) > 0) {//由于分片编号是递增的
			break;
		}
	}

	if (repeat == 0) {
		iqueue_init(&newseg->node);
		iqueue_add(&newseg->node, p);   // 加入到队列 按着顺序插入的
		kcp->nrcv_buf++;
	}	else {
		ikcp_segment_delete(kcp, newseg);
	}

#if 0
	ikcp_qprint("rcvbuf", &kcp->rcv_buf);
	printf("rcv_nxt=%lu\n", kcp->rcv_nxt);
#endif

	// move available data from rcv_buf -> rcv_queue
	while (! iqueue_is_empty(&kcp->rcv_buf)) {
		IKCPSEG *seg = iqueue_entry(kcp->rcv_buf.next, IKCPSEG, node);
		if (seg->sn == kcp->rcv_nxt && kcp->nrcv_que < kcp->rcv_wnd) {  // 将顺序正确的加入到队列
			iqueue_del(&seg->node);
			kcp->nrcv_buf--;
			iqueue_add_tail(&seg->node, &kcp->rcv_queue);
			kcp->nrcv_que++;
			kcp->rcv_nxt++; // rcv_nxt = 4, seg->sn = 4;  rcv_nxt = 5, seg->sn = 5
		}	else {
			break;
		}
	}

#if 0
	ikcp_qprint("queue", &kcp->rcv_queue);
	printf("rcv_nxt=%lu\n", kcp->rcv_nxt);
#endif

#if 1
//	printf("snd(buf=%d, queue=%d)\n", kcp->nsnd_buf, kcp->nsnd_que);
//	printf("rcv(buf=%d, queue=%d)\n", kcp->nrcv_buf, kcp->nrcv_que);
#endif
}

/**
 * 接收对方的数据输入
 * 该函数主要是处理接收到的数据
 * 校验数据=》解析数据=》处理数据（将合法的数据分片添加到接收buf中）=》拥塞窗口处理
 * 
 * 1.检测una，将una之前的分片从snd_buf清除(批量)
 * 2. 检测ack，对应ack sn分片从snd_buf清除（单个）
 */
//---------------------------------------------------------------------
// input data
//---------------------------------------------------------------------
int ikcp_input(ikcpcb *kcp, const char *data, long size)
{
	IUINT32 prev_una = kcp->snd_una;	// 最新应答的序号
	IUINT32 maxack = 0, latest_ts = 0;
	int flag = 0;

	if (ikcp_canlog(kcp, IKCP_LOG_INPUT)) {
		ikcp_log(kcp, IKCP_LOG_INPUT, "[RI] %d bytes", (int)size);
	}
    // 据和长度的初步校验 数据太小时异常，因为kcp头部都占用了24字节了，即时sendto最小的大小为 IKCP_OVERHEAD
	if (data == NULL || (int)size < (int)IKCP_OVERHEAD) return -1;

	while (1) {                             
		IUINT32 ts, sn, len, una, conv;
		IUINT16 wnd;
		IUINT8 cmd, frg;
		IKCPSEG *seg;

		if (size < (int)IKCP_OVERHEAD) break;
		// 校验数据分片 
		data = ikcp_decode32u(data, &conv);     // 获取segment头部信息
		if (conv != kcp->conv) return -1;		// 特别需要注意会话id的匹配

		data = ikcp_decode8u(data, &cmd);
		data = ikcp_decode8u(data, &frg);
		data = ikcp_decode16u(data, &wnd);
		data = ikcp_decode32u(data, &ts);
		data = ikcp_decode32u(data, &sn);
		data = ikcp_decode32u(data, &una);
		data = ikcp_decode32u(data, &len);

		size -= IKCP_OVERHEAD;	//剔除固定的包头信息长度

		if ((long)size < (long)len || (int)len < 0) return -2;      // 数据不足或者, 没有真正的数据存在
		
		//只支持{IKCP_CMD_PUSH, IKCP_CMD_ACK, IKCP_CMD_WASK, IKCP_CMD_WINS}指令
		//其他不合法
		if (cmd != IKCP_CMD_PUSH && cmd != IKCP_CMD_ACK &&
			cmd != IKCP_CMD_WASK && cmd != IKCP_CMD_WINS) 
			return -3;

		kcp->rmt_wnd = wnd;             // 携带了远端的接收窗口
		ikcp_parse_una(kcp, una);       // 删除小于snd_buf中小于una的segment, 意思是una之前的都已经收到了
		ikcp_shrink_buf(kcp);     // 更新snd_una为snd_buf中seg->sn或kcp->snd_nxt ，更新下一个待应答的序号

		if (cmd == IKCP_CMD_ACK) {
			if (_itimediff(kcp->current, ts) >= 0) {		// 根据应答判断rtt
                //更新rx_srtt，rx_rttval，计算kcp->rx_rto
				ikcp_update_ack(kcp, _itimediff(kcp->current, ts));
			}
             //遍历snd_buf中（snd_una, snd_nxt），将sn相等的删除，直到大于sn  
			ikcp_parse_ack(kcp, sn);    // 将已经ack的分片删除
			ikcp_shrink_buf(kcp);       // 更新控制块的 snd_una
			if (flag == 0) {
				flag = 1;       //快速重传标记
				maxack = sn;    // 记录最大的 ACK 编号
				latest_ts = ts;
			}	else {
				if (_itimediff(sn, maxack) > 0) {
				#ifndef IKCP_FASTACK_CONSERVE
					maxack = sn;        // 记录最大的 ACK 编号
					latest_ts = ts;
				#else
					if (_itimediff(ts, latest_ts) > 0) {
						maxack = sn;
						latest_ts = ts;
					}
				#endif
				}
			}
			if (ikcp_canlog(kcp, IKCP_LOG_IN_ACK)) {
				ikcp_log(kcp, IKCP_LOG_IN_ACK, 
					"input ack: sn=%lu rtt=%ld rto=%ld", (unsigned long)sn, 
					(long)_itimediff(kcp->current, ts),
					(long)kcp->rx_rto);
			}
		}
		else if (cmd == IKCP_CMD_PUSH) {	//接收到具体的数据包
			if (ikcp_canlog(kcp, IKCP_LOG_IN_DATA)) {
				ikcp_log(kcp, IKCP_LOG_IN_DATA, 
					"input psh: sn=%lu ts=%lu", (unsigned long)sn, (unsigned long)ts);
			}
			if (_itimediff(sn, kcp->rcv_nxt + kcp->rcv_wnd) < 0) {
				ikcp_ack_push(kcp, sn, ts); // 对该报文的确认 ACK 报文放入 ACK 列表中
				//  判断接收的数据分片编号是否符合要求，即：在接收窗口（滑动窗口）范围之内
				if (_itimediff(sn, kcp->rcv_nxt) >= 0) {    // 是要接受起始的序号
					seg = ikcp_segment_new(kcp, len);
					seg->conv = conv;
					seg->cmd = cmd;
					seg->frg = frg;
					seg->wnd = wnd;
					seg->ts = ts;
					seg->sn = sn;
					seg->una = una;
					seg->len = len;

					if (len > 0) {
						memcpy(seg->data, data, len);
					}

					//1. 丢弃sn > kcp->rcv_nxt + kcp->rcv_wnd的segment;
                    //2. 逐一比较rcv_buf中的segment，若重复丢弃，非重复，新建segment加入;
                    //3. 检查rcv_buf的包序号sn，如果是待接收的序号rcv_nxt，且可以接收（接收队列小 于接收窗口），
                    //    转移segment到rcv_buf，nrcv_buf减少，nrcv_que增加，rcv_nxt增加;
                    ikcp_parse_data(kcp, seg);  // 将该报文插入到 rcv_buf 链表中
				}
			}
		}
		else if (cmd == IKCP_CMD_WASK) {		
			// ready to send back IKCP_CMD_WINS in ikcp_flush // 如果是探测包
			// tell remote my window size	添加相应的标识位
			kcp->probe |= IKCP_ASK_TELL;        // 收到对方请求后标记自己要告诉对方自己的窗口
			if (ikcp_canlog(kcp, IKCP_LOG_IN_PROBE)) {
				ikcp_log(kcp, IKCP_LOG_IN_PROBE, "input probe");
			}
		}
		else if (cmd == IKCP_CMD_WINS) {
			// do nothing 如果是tell me 远端窗口大小，什么都不做
			if (ikcp_canlog(kcp, IKCP_LOG_IN_WINS)) {
				ikcp_log(kcp, IKCP_LOG_IN_WINS,
					"input wins: %lu", (unsigned long)(wnd));
			}
		}
		else {
			return -3;
		}

		data += len;
		size -= len;
	}

	if (flag != 0) {
		ikcp_parse_fastack(kcp, maxack, latest_ts);
	}
    // 如果snd_una增加了那么就说明对端正常收到且回应了发送方发送缓冲区第一个待确认的包，
    // 此时需要更新cwnd（拥塞窗口）
	if (_itimediff(kcp->snd_una, prev_una) > 0) {
		//如何拥塞窗口小于远端窗口
		if (kcp->cwnd < kcp->rmt_wnd) {
			IUINT32 mss = kcp->mss;	//最大分片大小
			if (kcp->cwnd < kcp->ssthresh) {  //拥塞窗口小于阈值
				kcp->cwnd++;			// 扩大窗口?	
				kcp->incr += mss;
			}	else {
				if (kcp->incr < mss) kcp->incr = mss;
				kcp->incr += (mss * mss) / kcp->incr + (mss / 16);
				if ((kcp->cwnd + 1) * mss <= kcp->incr) {
				#if 1
					kcp->cwnd = (kcp->incr + mss - 1) / ((mss > 0)? mss : 1);
				#else
					kcp->cwnd++;
				#endif
				}
			}
			//如果拥塞窗口大于远端窗口
			if (kcp->cwnd > kcp->rmt_wnd) {
				kcp->cwnd = kcp->rmt_wnd;	//则使用远端窗口
				kcp->incr = kcp->rmt_wnd * mss;	//并设置相应数据量，该数据量以字节数
			}
		}
	}

	return 0;
}


//---------------------------------------------------------------------
// ikcp_encode_seg 打包分片
//---------------------------------------------------------------------
static char *ikcp_encode_seg(char *ptr, const IKCPSEG *seg)
{
	ptr = ikcp_encode32u(ptr, seg->conv);
	ptr = ikcp_encode8u(ptr, (IUINT8)seg->cmd);
	ptr = ikcp_encode8u(ptr, (IUINT8)seg->frg);
	ptr = ikcp_encode16u(ptr, (IUINT16)seg->wnd);
	ptr = ikcp_encode32u(ptr, seg->ts);
	ptr = ikcp_encode32u(ptr, seg->sn);
	ptr = ikcp_encode32u(ptr, seg->una);
	ptr = ikcp_encode32u(ptr, seg->len);
	return ptr;
}
// 计算可接收长度，以f分片为单位
static int ikcp_wnd_unused(const ikcpcb *kcp)
{
	if (kcp->nrcv_que < kcp->rcv_wnd) {
		return kcp->rcv_wnd - kcp->nrcv_que;
	}
	return 0;
}


//---------------------------------------------------------------------
// ikcp_flush 检查 kcp->update 是否更新，未更新直接返回。kcp->update 由 ikcp_update 更新，
// 上层应用需要每隔一段时间（10-100ms）调用 ikcp_update 来驱动 KCP 发送数据；
//---------------------------------------------------------------------
/*
准备将 acklist 中记录的 ACK 报文发送出去，即从 acklist 中填充 ACK 报文的 sn 和 ts 字段；
检查当前是否需要对远端窗口进行探测。由于 KCP 流量控制依赖于远端通知其可接受窗口的大小，
一旦远端接受窗口 kcp->rmt_wnd 为0，那么本地将不会再向远端发送数据，因此就没有机会从远端接受 ACK 报文，
从而没有机会更新远端窗口大小。在这种情况下，KCP 需要发送窗口探测报文到远端，待远端回复窗口大小后，后续传输可以继续。
在发送数据之前，先设置快重传的次数和重传间隔；KCP 允许设置快重传的次数，即 fastresend 参数。
例如设置 fastresend 为2，并且发送端发送了1,2,3,4,5几个分片，收到远端的ACK: 1, 3, 4, 5，
当收到ACK3时，KCP知道2被跳过1次，收到ACK4时，知道2被“跳过”了2次，此时可以认为2号丢失，
不用等超时，直接重传2号包；每个报文的 fastack 记录了该报文被跳过了几次，由函数 ikcp_parse_fastack 更新。
于此同时，KCP 也允许设置 nodelay 参数，当激活该参数时，每个报文的超时重传时间将由 x2 变为 x1.5，即加快报文重传：

 * 1. ack确认包
 * 2. 探测远端窗口
 * 3. 发送snd_buf数据分片
 * 4. 更新拥塞窗口
*/
void ikcp_flush(ikcpcb *kcp)
{
	IUINT32 current = kcp->current;
	char *buffer = kcp->buffer;
	char *ptr = buffer;
	int count, size, i;
	IUINT32 resent, cwnd;
	IUINT32 rtomin;
	struct IQUEUEHEAD *p;
	int change = 0;
	int lost = 0;
	IKCPSEG seg;

	// 'ikcp_update' haven't been called. 
	if (kcp->updated == 0) return;

	seg.conv = kcp->conv;
	seg.cmd = IKCP_CMD_ACK;        // 这里的ack后
	seg.frg = 0;
	seg.wnd = ikcp_wnd_unused(kcp);     // 应答的时候携带了剩余的接收窗口大小
	seg.una = kcp->rcv_nxt;    // 已经处理到具体的分片
	seg.len = 0;
	seg.sn = 0;
	seg.ts = 0;

    // 逐一获取acklist中的sn和ts，编码成segment
	// flush acknowledges
	count = kcp->ackcount;		// 需要应答的分片数量
	for (i = 0; i < count; i++) {
		size = (int)(ptr - buffer);
		if (size + (int)IKCP_OVERHEAD > (int)kcp->mtu) {
			ikcp_output(kcp, buffer, size);
			ptr = buffer;
		}
		ikcp_ack_get(kcp, i, &seg.sn, &seg.ts); // 应答包 把时间戳发回去是为了能够计算RTT
		ptr = ikcp_encode_seg(ptr, &seg);       // 编码segment协议头
	}

	kcp->ackcount = 0;

	// probe window size (if remote window size equals zero)
	if (kcp->rmt_wnd == 0) {
		if (kcp->probe_wait == 0) { // 初始化探测间隔和下一次探测时间
			kcp->probe_wait = IKCP_PROBE_INIT;  // 默认7秒探测
			kcp->ts_probe = kcp->current + kcp->probe_wait; // 下一次探测时间
		}	
		else {
            //远端窗口为0，发送过探测请求，但是已经超过下次探测的时间  
            //更新probe_wait，增加为IKCP_PROBE_INIT+  probe_wait /2,但满足KCP_PROBE_LIMIT 
            //更新下次探测时间 ts_probe与 探测变量 为 IKCP_ASK_SEND，立即发送探测消息 
			if (_itimediff(kcp->current, kcp->ts_probe) >= 0) { // 检测是否到了探测时间
				if (kcp->probe_wait < IKCP_PROBE_INIT) 
					kcp->probe_wait = IKCP_PROBE_INIT;
				kcp->probe_wait += kcp->probe_wait / 2;
				if (kcp->probe_wait > IKCP_PROBE_LIMIT)
					kcp->probe_wait = IKCP_PROBE_LIMIT;
				kcp->ts_probe = kcp->current + kcp->probe_wait;
				kcp->probe |= IKCP_ASK_SEND;
			}
		}
	}	else {
        // 远端窗口正常，则不需要探测 远端窗口不等于0，更新下次探测时间与探测窗口等待时间为0，不发送窗口探测  
		kcp->ts_probe = 0;      
		kcp->probe_wait = 0;
	}

	// flush window probing commands
	if (kcp->probe & IKCP_ASK_SEND) {
		seg.cmd = IKCP_CMD_WASK;        // 窗口探测  [询问对方窗口size]
		size = (int)(ptr - buffer);
		if (size + (int)IKCP_OVERHEAD > (int)kcp->mtu) {
			ikcp_output(kcp, buffer, size);
			ptr = buffer;
		}
		ptr = ikcp_encode_seg(ptr, &seg);
	}

	// flush window probing commands
	if (kcp->probe & IKCP_ASK_TELL) {
		seg.cmd = IKCP_CMD_WINS;    // [告诉对方我方窗口size], 如果不为0，可以往我方发送数据
		size = (int)(ptr - buffer);
		if (size + (int)IKCP_OVERHEAD > (int)kcp->mtu) {
			ikcp_output(kcp, buffer, size);
			ptr = buffer;
		}
		ptr = ikcp_encode_seg(ptr, &seg);
	}

	kcp->probe = 0;	//清空标识

	// calculate window size 取发送窗口和远端窗口最小值得到拥塞窗口小
	cwnd = _imin_(kcp->snd_wnd, kcp->rmt_wnd);      // 当rmt_wnd为0的时候, 
	// 如果做了流控制则取配置拥塞窗口、发送窗口和远端窗口三者最小值
	if (kcp->nocwnd == 0) cwnd = _imin_(kcp->cwnd, cwnd);   // 进一步控制cwnd大小

	// move data from snd_queue to snd_buf
    // 从snd_queue移动到snd_buf的数量不能超出对方的接收能力   此时如果
	//  发送那些符合拥塞范围的数据分片
	while (_itimediff(kcp->snd_nxt, kcp->snd_una + cwnd) < 0) {
		IKCPSEG *newseg;
		if (iqueue_is_empty(&kcp->snd_queue)) break;

		newseg = iqueue_entry(kcp->snd_queue.next, IKCPSEG, node);

		iqueue_del(&newseg->node);
		iqueue_add_tail(&newseg->node, &kcp->snd_buf);  // 从发送队列添加到发送缓存
		kcp->nsnd_que--;
		kcp->nsnd_buf++;
		//设置数据分片的属性
		newseg->conv = kcp->conv;
		newseg->cmd = IKCP_CMD_PUSH;
		newseg->wnd = seg.wnd;		// 告知对方当前的接收窗口
		newseg->ts = current;
		newseg->sn = kcp->snd_nxt++;        // 序号
		newseg->una = kcp->rcv_nxt;
		newseg->resendts = current;
		newseg->rto = kcp->rx_rto;
		newseg->fastack = 0;
		newseg->xmit = 0;
	}

	// calculate resent (1)使用快速重传时 resent = fastresend; (2)不使用时resent = 0xffffffff
	resent = (kcp->fastresend > 0)? (IUINT32)kcp->fastresend : 0xffffffff;  // 使用快速重传时
	rtomin = (kcp->nodelay == 0)? (kcp->rx_rto >> 3) : 0;       // 最小超时时间

	// flush data segments
    // 只要还在snd_buf 说明对方还没有应答
	// 发送snd buf的分片
	for (p = kcp->snd_buf.next; p != &kcp->snd_buf; p = p->next) {
		IKCPSEG *segment = iqueue_entry(p, IKCPSEG, node);
		int needsend = 0;
		if (segment->xmit == 0) {   //1 如果该报文是第一次传输，那么直接发送
			needsend = 1;
			segment->xmit++;    // 发送次数技术
			segment->rto = kcp->rx_rto; // 超时时间
			segment->resendts = current + segment->rto + rtomin;    // 下一次要发送的时间
		}
		else if (_itimediff(current, segment->resendts) >= 0) { //2 当前时间达到了重发时间，但并没有新的ack到达，出现丢包, 重传  
			needsend = 1;
			segment->xmit++;
			kcp->xmit++;
			if (kcp->nodelay == 0) {
				segment->rto += _imax_(segment->rto, (IUINT32)kcp->rx_rto);
			}	else {
				IINT32 step = (kcp->nodelay < 2)? 
					((IINT32)(segment->rto)) : kcp->rx_rto;
				segment->rto += step / 2;
			}
			segment->resendts = current + segment->rto;
			lost = 1;       // 丢包，反应到拥塞控制策略去了
		}
		else if (segment->fastack >= resent) {  //3 segment的累计被跳过次数大于快速重传设定，需要重传  
			if ((int)segment->xmit <= kcp->fastlimit || 
				kcp->fastlimit <= 0) {
				needsend = 1;
				segment->xmit++;
				segment->fastack = 0;
				segment->resendts = current + segment->rto;
				change++;
			}
		}

		if (needsend) {
			int need;
			segment->ts = current;
			segment->wnd = seg.wnd; // 剩余接收窗口大小(接收窗口大小-接收队列大小), 告诉对方目前自己的接收能力
			segment->una = kcp->rcv_nxt;   // 待接收的下一个包序号, 即是告诉对方una之前的包都收到了, 你不用再发送发送缓存了

			size = (int)(ptr - buffer);
			need = IKCP_OVERHEAD + segment->len;

			if (size + need > (int)kcp->mtu) {		// 小包封装成大包取发送 500 500 ， 按1000发
				ikcp_output(kcp, buffer, size);
				ptr = buffer;
			}

			ptr = ikcp_encode_seg(ptr, segment);    // 把segment封装成线性buffer发送 头部+数据

			if (segment->len > 0) {
				memcpy(ptr, segment->data, segment->len);
				ptr += segment->len;
			}

			if (segment->xmit >= kcp->dead_link) {
				kcp->state = (IUINT32)-1;
			}
		}
	}

	// flash remain segments
	size = (int)(ptr - buffer);     // 剩余的数据
	if (size > 0) {
		ikcp_output(kcp, buffer, size);  // 最终只要有数据要发送，一定发出去
	}

	// update ssthresh  看完 用户态协议栈再来看这里的拥塞控制
	if (change) {   //如果发生了快速重传，拥塞窗口阈值降低为当前未确认包数量的一半或最小值 
		IUINT32 inflight = kcp->snd_nxt - kcp->snd_una;
		kcp->ssthresh = inflight / 2;
		if (kcp->ssthresh < IKCP_THRESH_MIN)
			kcp->ssthresh = IKCP_THRESH_MIN;
		kcp->cwnd = kcp->ssthresh + resent;     // 动态调整拥塞控制窗口
		kcp->incr = kcp->cwnd * kcp->mss;
	}

	if (lost) {     
		kcp->ssthresh = cwnd / 2;
		if (kcp->ssthresh < IKCP_THRESH_MIN)
			kcp->ssthresh = IKCP_THRESH_MIN;
		kcp->cwnd = 1;  //丢失则阈值减半, cwd 窗口保留为 1   动态调整拥塞控制窗口 
		kcp->incr = kcp->mss;
	}

	if (kcp->cwnd < 1) {
		kcp->cwnd = 1;  
		kcp->incr = kcp->mss;
	}
}


//---------------------------------------------------------------------
// update state (call it repeatedly, every 10ms-100ms), or you can ask 
// ikcp_check when to call it again (without ikcp_input/_send calling).
// 'current' - current timestamp in millisec. 时钟来源
//---------------------------------------------------------------------
void ikcp_update(ikcpcb *kcp, IUINT32 current)
{
	IINT32 slap;

	kcp->current = current;         // 超时重传要不要？

	if (kcp->updated == 0) {
		kcp->updated = 1;
		kcp->ts_flush = kcp->current;
	}

	slap = _itimediff(kcp->current, kcp->ts_flush);

	if (slap >= 10000 || slap < -10000) {   // 至少10ms的间隔触发一次
		kcp->ts_flush = kcp->current;
		slap = 0;
	}

	if (slap >= 0) {
		kcp->ts_flush += kcp->interval;     // 先按interval叠加下一次要刷新的时间
		if (_itimediff(kcp->current, kcp->ts_flush) >= 0) { // 如果下一次要刷新的时间已经落后则需要做校正
			kcp->ts_flush = kcp->current + kcp->interval;   // 使用当前时间 + interval进行校正
		}
		ikcp_flush(kcp);
	}
}


//---------------------------------------------------------------------
// Determine when should you invoke ikcp_update:
// returns when you should invoke ikcp_update in millisec, if there 
// is no ikcp_input/_send calling. you can call ikcp_update in that
// time, instead of call update repeatly.
// Important to reduce unnacessary ikcp_update invoking. use it to 
// schedule ikcp_update (eg. implementing an epoll-like mechanism, 
// or optimize ikcp_update when handling massive kcp connections)
//---------------------------------------------------------------------
IUINT32 ikcp_check(const ikcpcb *kcp, IUINT32 current)
{
	IUINT32 ts_flush = kcp->ts_flush;
	IINT32 tm_flush = 0x7fffffff;
	IINT32 tm_packet = 0x7fffffff;
	IUINT32 minimal = 0;
	struct IQUEUEHEAD *p;

	if (kcp->updated == 0) {
		return current;
	}

	if (_itimediff(current, ts_flush) >= 10000 ||
		_itimediff(current, ts_flush) < -10000) {
		ts_flush = current;
	}

	if (_itimediff(current, ts_flush) >= 0) {
		return current;
	}

	tm_flush = _itimediff(ts_flush, current);

	for (p = kcp->snd_buf.next; p != &kcp->snd_buf; p = p->next) {
		const IKCPSEG *seg = iqueue_entry(p, const IKCPSEG, node);
		IINT32 diff = _itimediff(seg->resendts, current);
		if (diff <= 0) {
			return current;
		}
		if (diff < tm_packet) tm_packet = diff;
	}

	minimal = (IUINT32)(tm_packet < tm_flush ? tm_packet : tm_flush);
	if (minimal >= kcp->interval) minimal = kcp->interval;

	return current + minimal;
}



int ikcp_setmtu(ikcpcb *kcp, int mtu)
{
	char *buffer;
	if (mtu < 50 || mtu < (int)IKCP_OVERHEAD) 
		return -1;
	buffer = (char*)ikcp_malloc((mtu + IKCP_OVERHEAD) * 3);
	if (buffer == NULL) 
		return -2;
	kcp->mtu = mtu;	// 本质是sendto的最大size值
	kcp->mss = kcp->mtu - IKCP_OVERHEAD;    // 默认mtu 1400, - IKCP_OVERHEAD = 1400-24= 1376
	ikcp_free(kcp->buffer);
	kcp->buffer = buffer;
	return 0;
}
// 设置调度间隔
int ikcp_interval(ikcpcb *kcp, int interval)
{
	if (interval > 5000) interval = 5000;
	else if (interval < 10) interval = 10;
	kcp->interval = interval;
	return 0;
}
// 设置无延迟机制
int ikcp_nodelay(ikcpcb *kcp, int nodelay, int interval, int resend, int nc)
{
	if (nodelay >= 0) {
		kcp->nodelay = nodelay;
		if (nodelay) {
			kcp->rx_minrto = IKCP_RTO_NDL;		//设置无延迟，设置相应的最小重传时间
		}	
		else {
			kcp->rx_minrto = IKCP_RTO_MIN;	//设置正常的最小重传时间
		}
	}
	if (interval >= 0) {	//设置调度间隔
		if (interval > 5000) interval = 5000;
		else if (interval < 10) interval = 10;
		kcp->interval = interval;
	}
	if (resend >= 0) {	//设置快速重传数
		kcp->fastresend = resend;
	}
	if (nc >= 0) {      // 0 不关闭流控， 1关闭流控
		kcp->nocwnd = nc;
	}
	return 0;
}

// 设置发送和接收窗口
int ikcp_wndsize(ikcpcb *kcp, int sndwnd, int rcvwnd)
{
	if (kcp) {
		if (sndwnd > 0) {
			kcp->snd_wnd = sndwnd;
		}
		if (rcvwnd > 0) {   // must >= max fragment size
			kcp->rcv_wnd = _imax_(rcvwnd, IKCP_WND_RCV);
		}
	}
	return 0;
}
// 我们可以获取等待发送的,  如果有就flush?
int ikcp_waitsnd(const ikcpcb *kcp)
{
	return kcp->nsnd_buf + kcp->nsnd_que;
}


// read conv 获取会话id
IUINT32 ikcp_getconv(const void *ptr)
{
	IUINT32 conv;
	ikcp_decode32u((const char*)ptr, &conv);
	return conv;
}