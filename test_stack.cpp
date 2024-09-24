//
// Created by blair on 2024/9/8.
//
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>  // 用于计时
#include "Stack.h"  // 假设你的代码保存在 stack.h 中

// 单线程测试性能
template<typename StackType>
void test_single_thread_performance()
{
    StackType stack{};
    int num_operations = 1000000;  // 测试操作的数量

    // 计时：push 操作
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        stack.push(i);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Single-thread push: " << duration << " ms" << std::endl;

    // 计时：pop 操作
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        auto res = stack.pop();
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Single-thread pop: " << duration << " ms" << std::endl;
}

// 多线程测试性能
template<typename StackType>
void test_multi_thread_performance()
{
    StackType stack{};
    int num_operations = 1000000;
    int num_threads = 4;  // 测试的线程数

    // 多线程 push 测试
    auto push_fn = [&stack, num_operations, num_threads](int offset) {
        for (int i = 0; i < num_operations / num_threads; ++i) {
            stack.push(i + offset);
        }
    };

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(push_fn, i * 1000);
    }

    for (auto& t : threads) {
        t.join();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Multi-thread push: " << duration << " ms" << std::endl;

    // 多线程 pop 测试
    auto pop_fn = [&stack, num_operations, num_threads]() {
        for (int i = 0; i < num_operations / num_threads; ++i) {
            auto res = stack.pop();
        }
    };

    start = std::chrono::high_resolution_clock::now();
    threads.clear();
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(pop_fn);
    }

    for (auto& t : threads) {
        t.join();
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Multi-thread pop: " << duration << " ms" << std::endl;
}

int main()
{
    // 测试 Stack1 性能
    std::cout << "Testing Stack1 performance..." << std::endl;
    test_single_thread_performance<Stack1<int>>();
    test_multi_thread_performance<Stack1<int>>();

    // 测试 Stack2 性能
    std::cout << "Testing Stack2 performance..." << std::endl;
    test_single_thread_performance<Stack2<int>>();
    test_multi_thread_performance<Stack2<int>>();

    // 测试 Stack3 性能
    std::cout << "Testing Stack3 performance..." << std::endl;
    test_single_thread_performance<Stack3<int>>();
    test_multi_thread_performance<Stack3<int>>();

    // // 测试 LockFreeStack1 性能
    // std::cout << "Testing LockFreeStack1 performance..." << std::endl;
    // test_single_thread_performance<LockFreeStack1<int>>();
    // test_multi_thread_performance<LockFreeStack1<int>>();

    // 测试 LockFreeStack2 性能
    std::cout << "Testing LockFreeStack2 performance..." << std::endl;
    test_single_thread_performance<LockFreeStack2<int>>();
    test_multi_thread_performance<LockFreeStack2<int>>();

    // 测试 LockFreeStack3 性能
    std::cout << "Testing LockFreeStack3 performance..." << std::endl;
    test_single_thread_performance<LockFreeStack3<int>>();
    test_multi_thread_performance<LockFreeStack3<int>>();

    // 测试 LockFreeStack4 性能
    std::cout << "Testing LockFreeStack4 performance..." << std::endl;
    test_single_thread_performance<LockFreeStack4<int>>();
    test_multi_thread_performance<LockFreeStack4<int>>();

    return 0;
}
