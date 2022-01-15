# 网络io与select poll epoll
=====

## 简介
   一个对话框实验，主要用于理解网络io，使用了阻塞，多线程，select，poll，epoll等多种方式 
## 三次握手四次挥手

首先还是需要搞清楚三次握手和四次挥手的过程

### 三次握手

```mermaid

sequenceDiagram
Note left of 客户端: 连接请求
客户端->>服务器:SYN=1，seq=client_isn
Note right of 服务器: 允许连接
服务器->>客户端:SYN=1，seq=server_isn，
<br/>ack = client_isn+1
Note left of 客户端: ack
客户端->>服务器:SYN=0，seq=client_isn+1,
<br/>ack = client_isn+1
```
## 五种网络IO模型与其实现

### 1.阻塞
    * accept位于whlie循环之前：只能连接一个客户端
    * accept位于whlie循环之中：能连接多个客户端，但只能接收一条消息
### 2.thread 多线程
    * 优点：结构简单
    * 缺点：无法支持大量客户端
### 3.
    
### 4.
    
### 5.
    
### 6.
    




