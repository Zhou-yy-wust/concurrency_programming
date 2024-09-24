//
// Created by blair on 2024/9/4.
//

#ifndef QUEUE_H
#define QUEUE_H
#include <condition_variable>
#include <memory>
#include <mutex>
#include <atomic>

class QueueEmptyError final: public std::exception
{
    [[nodiscard]] const char* what() const noexcept override
    {
        return "Queue is Empty.";
    }
};

// 实现一个细粒度锁的FIFO队列， 头出、尾进
// 暂时定义为nocopyable，固定头节点为虚拟节点
template <typename T>
class Queue
{
private:
    struct Node
    {
        std::shared_ptr<T> data;
        std::unique_ptr<Node> next;
    };

public:
    Queue(): _head(new Node), _tail(_head.get()){}
    ~Queue() = default;
    void push(const T& v)
    {
        auto data = std::make_shared<T>(v);
        auto new_tail = std::make_unique<Node>();
        {
            std::lock_guard<std::mutex> lock_tail(_tail_mtx);
            _tail->data = data;
            _tail->next = std::move(new_tail);
            _tail = new_tail.get();
        }
        _cv.notify_one();
    }

    std::shared_ptr<T> pop()
    {
        // get_tail需要放在_head_mtx里，因为放在外部可能会被其他线程Pop掉
        std::lock_guard<std::mutex> lock_head(_head_mtx);
        if(_head.get() == get_tail()) return std::make_shared<T>();

        auto old_head = std::move(_head);
        _head = std::move(old_head->next);
        return old_head->data;
    }

    bool pop(T& t)
    {
        std::lock_guard<std::mutex> lock_head(_head_mtx);
        if(_head.get() == get_tail()) return false;
        auto old_head = std::move(_head);
        _head = std::move(old_head->next);
        t = *(old_head->data);
        return true;
    }

    std::shared_ptr<T> wait_pop()
    {
        std::unique_lock<std::mutex> lock_head(_head_mtx);
        _cv.wait(_head_mtx, [this]() {return _head.get() != get_tail();});
        auto old_head = std::move(_head);
        _head = std::move(old_head->next);
        return old_head->data;
    }

    void wait_pop(T &t)
    {
        std::unique_lock<std::mutex> lock_head(_head_mtx);
        _cv.wait(_head_mtx, [this]() {return _head.get() != get_tail();});
        auto old_head = std::move(_head);
        _head = std::move(old_head->next);
        t = std::move(*(old_head->data));
    }

private:
    Node* get_tail()
    {
        std::lock_guard<std::mutex> lock_head(_tail_mtx);
        return _tail;
    }

private:
    std::mutex _head_mtx;
    std::mutex _tail_mtx;
    std::condition_variable _cv;
    std::unique_ptr<Node> _head;
    Node* _tail;
};


// 无锁队列，虚拟尾节点
// 单生产者、单消费者
template <typename T>
class LockFreeQueue1
{
private:
    struct Node
    {
        std::shared_ptr<T> data;
        Node* next;
    };

public:
    LockFreeQueue1(): _head(Node()), _tail(_head){}

    void push(const T& t)
    {
        auto new_data = std::make_shared<T>(t);
        Node* new_node = new Node;
        Node* old_tail = _tail.load(std::memory_order_relaxed);
        old_tail->data = new_data;
        old_tail->next = new_node;
        _tail.store(new_node, std::memory_order_release);
    }

    std::shared_ptr<T> pop()
    {
        Node* old_head = _head.load(std::memory_order_relaxed);
        if(old_head == _tail.load(std::memory_order_acquire)) return std::shared_ptr<T>();
        auto res = old_head->data;
        _head.store(old_head->next, std::memory_order_relaxed);
        delete old_head;
        return res;
    }

    ~LockFreeQueue1()
    {
        while(Node* old_head = _head.load())
        {
            _head.store(old_head->next);
            delete old_head;
        }
    }

private:
    std::atomic<Node*> _head;
    std::atomic<Node*> _tail;
};




template <typename T>
class LockFreeQueue2
{
private:
    struct Node;
    struct CountedNodePtr
    {
        Node* ptr;
        int external_count;
    };

    struct NodeCounter
    {
        unsigned internal_count:30;
        unsigned external_count:2;
    };

    struct Node
    {
        std::atomic<T*> data;
        std::atomic<NodeCounter> count;
        std::atomic<CountedNodePtr> next;

        Node()
        {
            NodeCounter new_count;
            new_count.internal_count = 0;
            new_count.external_count = 2;
            count.store(new_count);

            next.ptr = nullptr;
            next.external_count = 0;
        }

        void release_ref()
        {
            NodeCounter old_counter = count.load(std::memory_order_relaxed);
            NodeCounter new_counter;
            do
            {
                new_counter = old_counter;
                --new_counter.internal_count;
            }while(!count.compare_exchange_strong(
                old_counter, new_counter),
                std::memory_order_release, std::memory_order_release
                );

            if(!new_counter.internal_count && !new_counter.external_count) delete this;
        }
    };

public:
    LockFreeQueue2(): _head(CountedNodePtr()), _tail(_head){}

    void push(const T& t)
    {
        auto new_data = std::make_unique<T>(t);
        CountedNodePtr new_next;
        new_next.ptr = new Node;
        new_next.external_count = 1;
        CountedNodePtr old_tail = _tail.load();

        while (true)
        {
            increase_external_count(_tail, old_tail);
            T* old_data = nullptr;

            if(old_tail.ptr->data.compare_exchange_strong(old_data, new_data.get()))
            {
                CountedNodePtr old_next = {0};
                if(!old_tail.ptr->next.compare_exchange_strong(old_next, new_next))
                {
                    delete new_next.ptr;
                    new_next = old_next;
                }

                set_new_tail(old_tail, new_next);
                new_data.release();
                break;
            }
            else
            {
                CountedNodePtr old_next = {0};
                if(old_tail.ptr->next.compare_exchange_strong(  // 11
           old_next,new_next))
                {
                    old_next=new_next;  // 12
                    new_next.ptr=new Node;  // 13
                }
                set_new_tail(old_tail, old_next);  // 14
            }
        }
    }

    std::unique_ptr<T> pop()
    {
        CountedNodePtr old_head = _head.load(std::memory_order_relaxed);
        while(true)
        {
            increase_external_count(_head, old_head);
            Node* ptr = old_head.ptr;
            if(ptr == _tail.load().ptr)
            {
                ptr->release_ref();
                return std::unique_ptr<T>();
            }
            CountedNodePtr next = ptr->next.load();
            if(_head.compare_exchange_strong(old_head, ptr->next))
            {
                T* res = ptr->data.exchange(nullptr);
                free_external_counter(old_head);
                return std::unique_ptr<T>(*res);
            }
            ptr->release_ref();
        }
    }


private:
    std::atomic<CountedNodePtr> _head;
    std::atomic<CountedNodePtr> _tail;

    static void increase_external_count(std::atomic<CountedNodePtr>& counter,  CountedNodePtr& old_counter)
    {
        CountedNodePtr new_counter;
        do
        {
            new_counter = old_counter;
            ++new_counter.external_count;
        }while(!counter.compare_exchange_strong(old_counter, new_counter), std::memory_order_acquire, std::memory_order_relaxed);

        old_counter.external_count = new_counter.external_count;
    }
    static void free_external_counter(CountedNodePtr& old_node_ptr)
    {
        Node* ptr = old_node_ptr.ptr;
        int count_increase = old_node_ptr.external_count - 2;
        NodeCounter old_counter = ptr->count.load(std::memory_order_relaxed);
        NodeCounter new_counter;
        do
        {
            new_counter = old_counter;
            --new_counter.external_count;
        }while(
            !ptr->count.compare_exchange_strong(
                old_counter, new_counter,
                std::memory_order_acquire, std::memory_order_relaxed)
                );
        if(!new_counter.internal_count && !new_counter.external_count) delete ptr;
    }

    void set_new_tail(CountedNodePtr &old_tail, CountedNodePtr &new_tail)
    {
        Node* current_tail_ptr = old_tail.ptr;
        while(!_tail.compare_exchange_weak(old_tail, new_tail) && old_tail.ptr==current_tail_ptr){}
        if(old_tail.ptr == current_tail_ptr) free_external_counter(old_tail);
        else current_tail_ptr->release_ref();
    }
};

#endif //QUEUE_H
