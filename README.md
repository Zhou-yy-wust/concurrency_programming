# 并发编程
* 无锁单生产者单消费者环形队列：实现了一个无锁环形队列，专为单生产者单消费者(SPSC)模型优化，通过指针连接构建环形链表，提升数据
处理效率。队列具备动态扩展功能，能在容量不足时自动分配新块并链接至链表。实施缓存行对齐策略，减少伪共享问题。
* 栈：锁栈、基于双引用计数的无锁栈。
* 单例模式：包括无锁、静态实例、有锁等。

```shell
cmake -B build && cmake --build build

```

运行测试
```shell
./build/test_singleton
./build/test_stack
./build/test_sq
```
