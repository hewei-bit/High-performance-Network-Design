# High-performance-Network-Design

# 网络编程实践

## 0.简介
    、根据陈硕的网络编程实践课程，通过十几个例子，加深对网络编程的理解

## 1.课程亮点 Highlight of the course

  1. Focus on server-side TCP neteork programming
  2. Measurable performance
  3. No hypothetic optimizations

## 2.铺垫 Layered NetWork

  * Ethernet frame 以太网帧
  * IP packet IP包
  * TCP segment TCP分节
  * Application message 应用消息 


## 3.课程大纲

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