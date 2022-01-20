# Reactor百万并法连接实现 
=====
## 简介
  基于 [reactor原理与实现](https://blog.csdn.net/weixin_43687811/article/details/122573696)的基础上继续做出改进进而实现服务器的百万并发连接
   
## epoll触发方式   
 * epoll 的描述符事件有两种触发模式：LT（level trigger）和 ET（edge trigger）。
	 *  LT 模式（水平触发，默认方式）
		 * 描述：当 ==epoll_wait==检测到描述符事件到达时，将此事件通知进程，进程==可以不立即==处理该事件，下次调用 epoll_wait()会再次通知进程。是==默认==的一种模式，并且同时支持 Blocking 和 No-Blocking。即LT 模式下无论是否设置了EPOLLONESHOT,都是epoll_wait检测缓冲区有没有数据，有就返回，否则等待；
	 	* 适用范围:  ==小数据==（recv一次性能够读完）
	 * ET 模式（边沿触发）
	 	* 描述：和 LT 模式不同的是，通知之后进程==必须立即==处理事件，下次再调用 epoll_wait() 时不会再得到事件到达的通知。很大程度上减少了 epoll 事件被重复触发的次数，因此效率要比 LT 模式高。只支持 No-Blocking，以避免由于一个文件句柄的阻塞读/阻塞写操作把处理多个文件描述符的任务饿死。
	 	* 适用范围：==并发量大==


## 实现过程
这是之前的reactor模型，reactor结构体直接有个指向ntyevent头节点的指针*ntyevent
![之前的reactor的模型](https://img-blog.csdnimg.cn/a87462e786284ef88cb7b14488b30702.png?x-oss-process=image/watermark,type_d3F5LXplbmhlaQ,shadow_50,text_Q1NETiBA5L2V6JSa,size_20,color_FFFFFF,t_70,g_se,x_16#pic_center)
这是百万并发的reactor模型，reactor结构体中eventblock指针指向eventblock链表的头结点，eventblock结构体有指向下一一个eventblock的next指针，也有指向ntyevent头节点的指针
![百万并发连接模型](https://img-blog.csdnimg.cn/d4a6c4422f10488a84bf8774042759b0.png?x-oss-process=image/watermark,type_d3F5LXplbmhlaQ,shadow_50,text_Q1NETiBA5L2V6JSa,size_20,color_FFFFFF,t_70,g_se,x_16#pic_center)

```cpp
//事务结构体
struct ntyevent
{
    int fd;                                         //事务的fd
    int events;                                     // epoll events类型
    void *arg;                                      //需要传给回调函数的参数,一般传的是reactor的指针
    int (*callback)(int fd, int events, void *arg); //对应的回调函数
    int status;                                     // 0:新建 1：已存在
    char buffer[BUFFER_LENGTH];                     //接收到的消息
    int length;
    long last_active;

    // http 参数
    int method;
    char resource[BUFFER_LENGTH];
    int ret_code;
};

struct eventblock
{
    struct eventblock *next; //指向下一个block
    struct ntyevent *events; //当前block对应的event数组
};

// reator使用的结构体
struct ntyreactor
{
    int epfd; // reatctor的fd
    int blkcnt;
    struct eventblock *evblk; // reactor管理的基础单元，现在是block，实现C1000K
};
```
