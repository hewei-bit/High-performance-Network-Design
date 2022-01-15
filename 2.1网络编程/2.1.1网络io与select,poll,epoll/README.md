# 网络io与select poll epoll
=====

## 简介
    

## STL 六大部件 Component
  * 1.容器 Containers
    用于解决内存问题
  * 2.分配器 Allocators
    分配器用于支持容器
  * 3.算法 Algorithms
    算法处理容器内的数据
  * 4.迭代器 Iterators
    迭代器是泛化的指针，用作算法与容器的桥梁
  * 5.适配器 Adapters
    有容器适配器、仿函数适配器、迭代器适配器
  * 6.仿函数 Functors
    通过仿函数用来实现算法

### 1.容器分类
* 1.1序列容器
  * ps.缩进代表着“基层与衍生层的关系”，并非继承（inheritance）而是复合（composition）
  * array 连续空间
  * vector 连续空间
    * heap 以算法形式呈现 xxx_heap()
      * priority_queue
  * list  双向
  * forward-list 单向
  * deque 分段连续空间
    * stack
    * queue

* 1.2关联容器
  * rb_tree
  
    * set/multiset
        set/multiset 元素的value和key,value就是key
    * map/multimap
        map/mutimap 
        

* 1.3无序关联容器
  * hashtable
    * Unordered set/multset
    * Unordered map/multmap




