# Reactor原理与实现 
====
## 简介
实现一个Reactor框架，Reactor是一种==事件驱动框架==，该框架下改变了select、poll和epoll对fd进行管理的思路转变成对读写事件进行管理，重点变成了事件。Reator逆置了事件处理流程，应用程序需要提供相应的接口并注册到Reactor上，在相应的时间发生，Reator将主动调用这些接口函数，即==回调函数==

* reactor模型
	reactor以epoll为底层，主要是对事件为基本单元进行处理，多个事件并发地交给服务器进行处理，服务器将传入的事件同步地分派给相关地处理函数，reactor处理事件模型如下所示
	![reactor处理模型](https://img-blog.csdnimg.cn/ba74dd1eda0844458ff56ca533dbe780.png?x-oss-process=image/watermark,type_d3F5LXplbmhlaQ,shadow_50,text_Q1NETiBA5L2V6JSa,size_20,color_FFFFFF,t_70,g_se,x_16#pic_center)
* 定义结构
	首先分别定义了两个结构体，一个是事件 结构体==ntyevent==，一个是reactor结构体==ntyreactor==
	* ==ntyevent==
		* 该事件的==fd==
		* 对应的事件类型==events==比如输入输出
		* 需要传给回调函数的参数,一般传的是该事件所属reactor的指针==void* arg==
		* 对应的回调函数指针 ==int (*callback)==
		* 事件是否创建的状态 ==status==
		* 事件消息的==buffer==
		* 消息长度==length==
		* 最后的运行时间==last_active==
	* ==ntyreactor==（通过reactor来管理所有的事件）
		* 该reactor的==fd==
		* 该reactor内部的ntyevent数组==*events==（运行时通过epoll_wait取出所有存在内核的事件）
```cpp
//事务结构体
//缓冲区长度
#define BUFFER_LENGTH 4096
// epoll中的事物数量
#define MAX_EPOLL_EVENT 1024
#define SERVER_PORT 8888
struct ntyevent
{
    int fd;     //事务的fd
    int events; // epoll events类型
    void *arg;  //需要传给回调函数的参数,一般传的是reactor的指针
    int (*callback)(int fd, int events, void *arg); //对应的回调函数
    int status; // 0:新建 1：已存在
    char buffer[BUFFER_LENGTH];
    int length;
    long last_active;
};
// reator使用的结构体
struct ntyreactor
{
    int epfd;                // reatctor的fd
    struct ntyevent *events; // reactor管理的基础单元
};
```


* 编译环境 linux Ubuntu 
* 编译方式
	* gcc -o reactor reactor.c
* 使用方法
	* 服务器运行编译好的./reactor
	* 客户端使用网络调试助手进行访问
