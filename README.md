# High-performance-Network-Design

高性能网络设计

## 1. 网络编程

### 1.1 网络io与select、poll、epoll，reactor原理与实现、

### 1.2 http服务器实现、

### 1.3 websocket协议与服务器实现

## 2. 网络原理

### 2.1 服务器百万并发，网络协议栈，UDP可靠传输协议QUIC

### 2.2 协程框架NtyCo的实现

### 2.3 用户态协议栈NtyTcp的实现

## 3. 网络编程实践

### 3.1 Basic，non-concurrent examples（基础非并发实例 ）

  1. TTCP：
    * classic TCP performance testing tool
  2. Round-trip：
    * measue clock error between two hosts 测试两台机器的时间差
  3. Netcat：a swiss Knife
  4. slow sink/source

### 3.2  Concurrent examples（并发网络编程）

  1. SOCKs proxy server （socks代理服务器）
    * Relay two TCP connections
  2. Sudukou solver（数独求解，请求响应）
    * a lot services fit in this request-response model
  3. Simple memchached
  4. Broadcasting to multiple TCP peers（应用层广播）
    * How to deal with slow receiver

### 3.3 Data processing with multiple machines（多机数据处理）
  
  1. Parallel N-queues （并行N皇后问题求解）
  2. Median of numbers across machines（中位数求解）
  3. Frequent queries（）
  4. Distributed sorting （）

### 3.4 Advanced topics

  1. RPC：A basic building block for various servers
  2. Load balancing （负载均衡）
    * Better than round-robin
  3. Capacity of a serving system
    * How many machines do I need to support X QPS？
    * What will be the number of replicas of each component？
  4. Fight for （tail）latency
    * Mean and Percentiles：95%，99%
