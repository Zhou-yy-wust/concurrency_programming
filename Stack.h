//
// Created by blair on 2024/9/4.
//

#ifndef STACK_H
#define STACK_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <atomic>
#include <exception>
#include <stack>



class EmptyStackError final: public std::exception
{
public:
    [[nodiscard]] const char* what() const noexcept override
    {
        return "Stack is empty.";
    }
};


// 暂时不考虑拷贝控制成员
// 用户在使用时，需要不断的检查empty() 并且 pop();
template <typename T>
class Stack1
{
public:
    Stack1() = default;
    ~Stack1() = default;
    Stack1(const Stack1&) = delete;
    Stack1& operator=(const Stack1&) = delete;
    Stack1(Stack1&& other) noexcept{
        std::scoped_lock lock(_mtx, other._mtx);
        _stack = std::move(other._stack);
    }

    Stack1& operator=(Stack1&& other) noexcept
    {
        if(this != &other)
        {
            std::scoped_lock lock(_mtx, other._mtx);
            _stack = std::move(other._stack);
        }
        return *this;
    }


    void push(const T&v)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        _stack.emplace(v);
    }

    void push(T &&v)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        _stack.emplace(std::move(v));
    }

    void pop(T &v)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if(_stack.empty()) throw EmptyStackError();
        v = std::move(_stack.top());
        _stack.pop();
    }

    std::shared_ptr<T> pop()
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if(_stack.empty()) throw EmptyStackError();
        auto res = std::make_shared<T>(std::move(_stack.top()));
        _stack.pop();
        return res;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(_mtx);
        return _stack.empty();
    }
private:
    std::stack<T> _stack;
    mutable std::mutex _mtx;
};


// 通过条件变量，提供wait_pop，当无数据时直接阻塞，来数据时唤醒
// 缺陷：在wait_pop中，如果在获得了cv的通知之后抛出异常，导致其他的线程无法获得改notify，因此全部阻塞
template<typename T>
class Stack2
{
public:
    Stack2() = default;
    ~Stack2() = default;
    Stack2(const Stack2&) = delete;
    Stack2& operator=(const Stack2&) = delete;
    Stack2(Stack2&& other) noexcept
    {
        std::scoped_lock lock(_mtx, other._mtx);
        _stack = std::move(other._stack);
    }

    Stack2& operator=(Stack2&& other) noexcept
    {
        if(this != &other)
        {
            std::scoped_lock lock(_mtx, other._mtx);
            _stack = std::move(other._stack);
        }
        return *this;
    }

    void push(const T& v)   // 入栈，唤醒阻塞的线程
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _stack.emplace(v);
        _cv.notify_one();
    }

    void push(T&& v)   // 入栈，唤醒阻塞的线程
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _stack.emplace(std::move(v));
        _cv.notify_one();
    }

    void wait_pop(T& v)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _cv.wait(lock, [this]()->bool{return !_stack.empty();});
        v = std::move(_stack.top());
        _stack.pop();
    }

    std::shared_ptr<T> wait_pop()
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _cv.wait(lock, [this]()->bool{return !_stack.empty();});
        auto res = std::make_shared<T>(std::move(_stack.top()));  // 当此处抛出异常，由于本线程获得了cv的通知，导致其他的线程无法获得改notify，因此全部阻塞
        _stack.pop();
        return res;
    }

    void pop(T &v)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if(_stack.empty()) throw EmptyStackError();
        v = std::move(_stack.top());
        _stack.pop();
    }

    std::shared_ptr<T> pop()
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if(_stack.empty()) throw EmptyStackError();
        auto res = std::make_shared<T>(std::move(_stack.top()));
        _stack.pop();
        return res;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(_mtx);
        return _stack.empty();
    }

private:
    mutable std::mutex _mtx;
    std::condition_variable _cv;
    std::stack<T> _stack;
};


// 将数据分配内存的时间提前到push的过程中，这样wait_pop就不会因为无法分配内存抛出异常了
template<typename T>
class Stack3
{
public:
    Stack3() = default;
    ~Stack3() = default;
    Stack3(const Stack3&) = delete;
    Stack3& operator=(const Stack3&) = delete;
    Stack3(Stack3&& other) noexcept
    {
        std::scoped_lock lock(_mtx, other._mtx);
        _stack = std::move(other._stack);
    }

    Stack3& operator=(Stack3&& other) noexcept
    {
        if(this != &other)
        {
            std::scoped_lock lock(_mtx, other._mtx);
            _stack = std::move(other._stack);
        }
        return *this;
    }

    void push(const T& v)   // 入栈，唤醒阻塞的线程
    {
        auto t = std::make_shared<T>(v);
        std::unique_lock<std::mutex> lock(_mtx);
        _stack.push(t);
        _cv.notify_one();
    }

    void push(T&& v)   // 入栈，唤醒阻塞的线程
    {
        auto t = std::make_shared<T>(std::move(v));
        std::unique_lock<std::mutex> lock(_mtx);
        _stack.push(t);
        _cv.notify_one();
    }

    void wait_pop(T& v)
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _cv.wait(lock, [this]()->bool{return !_stack.empty();});
        v = std::move(*_stack.top());
        _stack.pop();
    }

    std::shared_ptr<T> wait_pop()
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _cv.wait(lock, [this]()->bool{return !_stack.empty();});
        auto res = _stack.top();  // 当此处抛出异常，由于本线程获得了cv的通知，导致其他的线程无法获得改notify，因此全部阻塞
        _stack.pop();
        return res;
    }

    void pop(T &v)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if(_stack.empty()) throw EmptyStackError();
        v = std::move(*_stack.top());
        _stack.pop();
    }

    std::shared_ptr<T> pop()
    {
        std::lock_guard<std::mutex> lock(_mtx);
        if(_stack.empty()) throw EmptyStackError();
        auto res = _stack.top();
        _stack.pop();
        return res;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(_mtx);
        return _stack.empty();
    }

private:
    mutable std::mutex _mtx;
    std::condition_variable _cv;
    std::stack<std::shared_ptr<T>> _stack;
};


/********************************无锁栈***************************************/

template <typename T>
class LockFreeStack1
{
private:
    struct Node
    {
        std::shared_ptr<T> data;
        Node* next;
    };

public:
    LockFreeStack1()= default;
    ~LockFreeStack1() = default;

    void push(const T& t)
    {
        auto new_node = new Node{std::make_shared<T>(t), nullptr};
        new_node->next = _head.load();
        while(!_head.compare_exchange_weak(new_node->next, new_node));
    }

    std::shared_ptr<T> pop()
    {
        // 无锁编程最大的问题，内存回收
        // 当获取到old_head后，可能其他的线程在pop中也持有这个old_head，怎么安全的delete?
        auto old_head = _head.load();
        while(old_head && !_head.compare_exchange_weak(old_head, old_head->next)){}
        return old_head? old_head->data : std::make_shared<T>();
    }

private:
    std::atomic<Node*> _head;
};


// 通过一个引用计数，统计进入pop函数的线程数量
// 缺陷：在高并发是，不会出现只有一个线程进入pop的时候，那么内存就会一直得不到释放；
template <typename T>
class LockFreeStack2
{
private:
    struct Node
    {
        std::shared_ptr<T> data;
        Node* next;
    };

public:
    LockFreeStack2()= default;
    ~LockFreeStack2() = default;
    LockFreeStack2(const LockFreeStack2&) = delete;
    LockFreeStack2(LockFreeStack2&& ) = delete;
    LockFreeStack2& operator=(const LockFreeStack2&) = delete;
    LockFreeStack2& operator=(LockFreeStack2&&) = delete;

    void push(const T& t)
    {
        auto new_node = new Node{std::make_shared<T>(t), nullptr};
        new_node->next = _head.load();
        while(!_head.compare_exchange_weak(new_node->next, new_node));
    }

    void push(T&& t)
    {
        auto new_node = new Node{std::make_shared<T>(std::move(t)), nullptr};
        new_node->next = _head.load();
        while(!_head.compare_exchange_weak(new_node->next, new_node));
    }

    std::shared_ptr<T> pop()
    {
        _threads_in_pop.fetch_add(1);

        auto old_head = _head.load();
        while(old_head && !_head.compare_exchange_weak(old_head, old_head->next)){}

        auto res = std::make_shared<T>();
        if(old_head)
        {
            res.swap(old_head->data);
            try_reclaim(old_head);
        }
        return res;
    }


private:
    void try_reclaim(Node* old_head)
    {
        if(_threads_in_pop == 1)  // 仅仅一个线程正在执行pop，那么就可以删除节点，以及待节点列表
        {
            // 获取待删除节点列表
            Node* need_to_be_deleted = _to_be_deleted.exchange(nullptr);
            if(!--_threads_in_pop)  // 再次确保只有一个线程
            {
                while(need_to_be_deleted)
                {
                    auto tmp = need_to_be_deleted;
                    need_to_be_deleted = need_to_be_deleted->next;
                    delete tmp;
                }
            }
            else  // 有多个线程，那么就将两个待删除列表合并
            {
                if(need_to_be_deleted)
                {
                    auto last = need_to_be_deleted;
                    while(last && last->next) last = last->next;
                    last->next = _to_be_deleted.load();
                    while(!_to_be_deleted.compare_exchange_weak(last->next, need_to_be_deleted));
                }


            }
            delete old_head;  // 后续进入pop的线程必然不会持有old_head;
        }
        else // 有多个线程，将待删除节点添加到列表后面
        {
            old_head->next = _to_be_deleted.load();
            while(!_to_be_deleted.compare_exchange_weak(old_head->next, old_head)){}
            --_threads_in_pop;
        }
    }

private:
    std::atomic<Node*> _head;
    std::atomic<int> _threads_in_pop{};
    std::atomic<Node*> _to_be_deleted;
};


// 通过对每个节点上访问的线程数量进行统计，解决问题。
// 此种依赖于shared_ptr是否std::atomic_is_lock_free是无锁的。如果有锁，那么便不是无锁的了
template <typename T>
class LockFreeStack3
{
private:
    struct Node
    {
        std::shared_ptr<T> data;
        std::shared_ptr<Node> next;
        explicit Node(const T& t):data(std::make_shared<T>(t)),next(nullptr){}
    };

public:
    LockFreeStack3()= default;
    ~LockFreeStack3() = default;
    LockFreeStack3(const LockFreeStack3&) = delete;
    LockFreeStack3(LockFreeStack3&& ) = delete;
    LockFreeStack3& operator=(const LockFreeStack3&) = delete;
    LockFreeStack3& operator=(LockFreeStack3&&) = delete;

    void push(const T& t)
    {
        auto new_node = std::make_shared<Node>(t);
        new_node->next = std::atomic_load(&_head);
        while(!std::atomic_compare_exchange_weak(&_head, &new_node->next, new_node));
    }

    void push(T&& t)
    {
        auto new_node = std::make_shared<Node>(std::move(t));
        new_node->next = std::atomic_load(&_head);
        while(!std::atomic_compare_exchange_weak(&_head, &new_node->next, new_node));
    }

    std::shared_ptr<T> pop()
    {
        // 无锁编程最大的问题，内存回收
        // 当获取到old_head后，可能其他的线程在pop中也持有这个old_head，怎么安全的delete?
        auto old_head = std::atomic_load(&_head);
        while(old_head && !std::atomic_compare_exchange_weak(&_head, &old_head, old_head->next)){}
        return old_head? old_head->data : std::make_shared<T>();
    }

private:
    std::shared_ptr<Node> _head;
};


// 手动实现计数，计数包括两个，内部计数和外部计数
// 两个值的总和就是对这个节点的引用数。外部计数标明多少线程引用节点。
// 此双计数的最大作用：标记待删除节点和栈中节点有不同状态，当已经pop出节点后将外部计数同步到内部计数中。
// 如果使用一个计数，那么待删除节点_head和_head_next的计数状态是一样的；
template <typename T>
class LockFreeStack4
{
private:
    struct Node;
    struct CountedNodePtr
    {
        int external_count{1};
        Node* ptr{nullptr};
    };

    struct Node
    {
        std::shared_ptr<T> data{nullptr};
        std::atomic<int> internal_count{0};
        CountedNodePtr next;

        explicit Node(const T& d)
        :data(std::make_shared<T>(d)),
         internal_count(0){}

        explicit Node(T&& d)
        :data(std::make_shared<T>(std::move(d))),
        internal_count(0){}
    };

public:
    LockFreeStack4()= default;
    ~LockFreeStack4() = default;
    LockFreeStack4(const LockFreeStack4&) = delete;
    LockFreeStack4(LockFreeStack4&& ) = delete;
    LockFreeStack4& operator=(const LockFreeStack4&) = delete;
    LockFreeStack4& operator=(LockFreeStack4&&) = delete;

    void push(const T& t)
    {
        CountedNodePtr new_node;
        new_node.ptr = new Node(t);
        new_node.external_count = 1;

        new_node.ptr->next = _head.load(std::memory_order_relaxed);
        while(!_head.compare_exchange_weak(
            new_node.ptr->next,
            new_node,
            std::memory_order_release,
            std::memory_order_relaxed)){}
    }

    void push(T&& t)
    {
        CountedNodePtr new_node;
        new_node.ptr = new Node(std::move(t));
        new_node.external_count = 1;

        new_node.ptr->next = _head.load(std::memory_order_relaxed);
        while(!_head.compare_exchange_weak(
            new_node.ptr->next,
            new_node,
            std::memory_order_release,
            std::memory_order_relaxed)){}
    }

    std::shared_ptr<T> pop()
    {
        CountedNodePtr old_node = _head.load(std::memory_order_relaxed);
        while(true)
        {
            increase_head_count(old_node);  // external_count+ 1，表示当前线程引用，并且保证读取到最新的head
            Node* ptr = old_node.ptr; // 当计数增加，就能安全的解引用ptr，并读取head指针的值，就能访问指向的节点
            if(!ptr) return std::shared_ptr<T>();  // 如果指针是空指针，那么将会访问到链表的最后。

            if (_head.compare_exchange_strong(old_node, ptr->next), std::memory_order_relaxed) // 为什么只需要next，而不需要一直循环？ 因为外层有个while(true)
                // 当compare_exchange_strong()成功时，就拥有对应节点的所有权，并且可以和data进行交换；
                // 不可能多个线程同时进入这个if分支
            {
                std::shared_ptr<T> res;
                res.swap(ptr->data);

                int count_increase = old_node.external_count - 2;  // 因为increase_head_count已经加了1次
                if(ptr->internal_count.fetch_add(count_increase, std::memory_order_release) == -count_increase) delete ptr; // 将所有外部引用更新到内部引用中，让其他线程删除或者本线程删除
                return  res; // 7
            }
            // 当“比较/交换”③失败，就说明其他线程在之前把对应节点删除了，或者其他线程添加了一个新的节点到栈中。
            // 无论是哪种原因，需要通过“比较/交换”的调用，对具有新值的head重新进行操作。
            // 不过，首先需要减少节点(要删除的节点)上的引用计数。这个线程将再也没有办法访问这个节点了。
            // 如果当前线程是最后一个持有引用(因为其他线程已经将这个节点从栈上删除了)的线程，那么内部引用计数将会为1，
            // 所以减一的操作将会让计数器为0。这样，你就能在循环⑧进行之前将对应节点删除了。
            if(ptr->internal_count.fetch_sub(1, std::memory_order_relaxed) == 1)
            {
                ptr->internal_count.load(std::memory_order_acquire);
                delete ptr; // 其他线程读到internal_count=1时，表明本线程是最后一个引用该节点的，负责删除。
            }
        }
    }

private:
    void increase_head_count(CountedNodePtr& old_header)
    {
        // 这里是因为CountedNodePtr的external_count不是原子的，改变他需要不断的重试
        // 其次，读取到的head可能不是最新的。
        CountedNodePtr new_counter;
        do
        {
            new_counter = old_header;
            ++new_counter.external_count;
        }while (!_head.compare_exchange_strong(
            old_header, new_counter,
            std::memory_order_acquire,
            std::memory_order_relaxed)); //1 通过增加外部引用计数，保证指针在访问期间的合法性。

        old_header.external_count = new_counter.external_count;
    }

    std::atomic<CountedNodePtr> _head;

};

#endif //STACK_H
