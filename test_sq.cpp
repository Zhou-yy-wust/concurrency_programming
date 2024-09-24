//
// Created by blair on 2024/9/10.
//
#include <iostream>
#include <thread>
#include <vector>
#include "SpscQueue.h"  // 你的SPSC队列头文件路径

void producer(sq::ReaderWriterQueue<int>& queue, int items_to_produce) {
    for (int i = 0; i < items_to_produce; ++i) {
        queue.inner_enqueue(i);
        std::cout << "Produced: " << i << std::endl;
    }
}

void consumer(sq::ReaderWriterQueue<int>& queue, int items_to_consume) {
    int item;
    for (int i = 0; i < items_to_consume; ++i) {
        while (!queue.try_dequeue(item)) {
            // 等待生产者生产
        }
        std::cout << "Consumed: " << item << std::endl;
    }
}

int main() {
    const int items_to_produce = 100;

    // 初始化队列
    sq::ReaderWriterQueue<int> queue(32);  // 初始化容量为32的队列

    // 启动生产者线程
    std::thread producer_thread(producer, std::ref(queue), items_to_produce);

    // 启动消费者线程
    std::thread consumer_thread(consumer, std::ref(queue), items_to_produce);

    // 等待生产者和消费者完成
    producer_thread.join();
    consumer_thread.join();

    std::cout << "Test completed successfully!" << std::endl;

    return 0;
}
