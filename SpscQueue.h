#ifndef SPSC_QUEUE_H
#define SPSC_QUEUE_H

#include <new>
#include <type_traits>
#include <utility>
#include <cassert>
#include <stdexcept>
#include <new>
#include <memory>
#include <atomic>
#include "SpscQueueUtils.h"

namespace sq{

template<typename T, size_t MAX_BLOCK_SIZE = 512>
class alignas(CACHE_LINE_SIZE) ReaderWriterQueue
    {
        public:
            using Block = sq::Block<T>;
            explicit ReaderWriterQueue(size_t size = 15);

            ReaderWriterQueue(const ReaderWriterQueue& ) = delete;
            ReaderWriterQueue(ReaderWriterQueue&& other) noexcept;
            ReaderWriterQueue& operator=(const ReaderWriterQueue&) = delete;
            ReaderWriterQueue& operator=(ReaderWriterQueue&& other) noexcept;

            ~ReaderWriterQueue();

            template<typename U>
            bool try_dequeue(U& result);

            template<typename U>
            bool inner_enqueue(U&& element);



        private:
            static constexpr size_t ceilToPow2(size_t x)
        {
                --x;
                x |= x >> 1;
                x |= x >> 2;
                x |= x >> 4;
                for (size_t i = 1; i < sizeof(size_t); i <<= 1)
                    {
                    x |= x >> (i << 3);
                    }
                ++x;
                return x;
        }

        private:
            std::atomic<Block*> frontBlock{};  // （原子）元素从此块中出列
            std::atomic<Block*> tailBlock{};  //（原子）元素排队到该块中
            size_t largestBlockSize;

            std::atomic<bool> enqueuing{false};
            std::atomic<bool> dequeuing{false};
    };



    template<typename T, size_t MAX_BLOCK_SIZE>
    ReaderWriterQueue<T, MAX_BLOCK_SIZE>::ReaderWriterQueue(size_t size)
    {   // 单向环形链表，头尾指向相同节点的空结点
        assert(MAX_BLOCK_SIZE == ceilToPow2(MAX_BLOCK_SIZE));
        assert(MAX_BLOCK_SIZE >= 2);

        Block* firstBlock = nullptr;
        largestBlockSize = ceilToPow2(size + 1);

        if (largestBlockSize > MAX_BLOCK_SIZE * 2) {
            size_t initialBlockCount = (size + MAX_BLOCK_SIZE * 2 - 3) / (MAX_BLOCK_SIZE - 1);
            largestBlockSize = MAX_BLOCK_SIZE;
            Block* lastBlock = nullptr;
            for (size_t i = 0; i != initialBlockCount; ++i) {
                auto block = Block::make_block(largestBlockSize);
                if (firstBlock == nullptr) {
                    firstBlock = block;
                } else {
                    lastBlock->store_next(block);
                }
                lastBlock = block;
                block->store_next(firstBlock);
            }
        } else {
            firstBlock = Block::make_block(largestBlockSize);
            firstBlock->store_next(firstBlock);
        }
        frontBlock.store(firstBlock);
        tailBlock.store(firstBlock);

        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    template<typename T, size_t MAX_BLOCK_SIZE>
    ReaderWriterQueue<T, MAX_BLOCK_SIZE>::ReaderWriterQueue(ReaderWriterQueue &&other) noexcept
    : frontBlock(other.frontBlock.load())
    , tailBlock(other.tailBlock.load())
    , largestBlockSize(other.largestBlockSize)
    {
        other.largestBlockSize = 32;
        Block* b = Block::make_block(other.largestBlockSize);

        b->store_next(b);
        other.frontBlock = b;
        other.tailBlock = b;
    }

    template<typename T, size_t MAX_BLOCK_SIZE>
    ReaderWriterQueue<T, MAX_BLOCK_SIZE>& ReaderWriterQueue<T, MAX_BLOCK_SIZE>::operator=(ReaderWriterQueue&& other) noexcept {  // 相当于交换彼此
        Block* b = frontBlock.load();
        frontBlock = other.frontBlock.load();
        other.frontBlock = b;

        b = tailBlock.load();
        tailBlock = other.tailBlock.load();
        other.tailBlock = b;

        std::swap(largestBlockSize, other.largestBlockSize);
        return *this;
    }

    template<typename T, size_t MAX_BLOCK_SIZE>
    ReaderWriterQueue<T, MAX_BLOCK_SIZE>::~ReaderWriterQueue()
    { // 一个一个块进行析构
        std::atomic_thread_fence(std::memory_order_seq_cst);
        Block* frontBlock_ = frontBlock.load();
        Block* block = frontBlock_;
        do {
            Block* nextBlock = block->next_block();
            block->~Block();
            block = nextBlock;
        } while (block != frontBlock_);
    }

    template<typename T, size_t MAX_BLOCK_SIZE>
    template<typename U>
    bool ReaderWriterQueue<T, MAX_BLOCK_SIZE>::try_dequeue(U& result) {
        ReentrantGuard guard(this->dequeuing);

        // 获取当前前端块和尾部状态
        Block* frontBlock_ = frontBlock.load();
        size_t blockTail = frontBlock_->get_local_tail();
        size_t blockFront = frontBlock_->get_front();

        // 检查当前前端块是否有元素,如果当前块的前端不等于尾部（即块中有元素），或者前端不等于尾部块的尾部（此检查确保块在出队前没有被生产者填充）
        if (blockFront != blockTail || blockFront != frontBlock_->get_local_tail_from_tail())
            {
            std::atomic_thread_fence(std::memory_order_acquire);
            // 从非空的前端块中出队
            auto element = frontBlock_->get_element_at_idx(blockFront);
            result = std::move(*element);
            element->~T();

            blockFront = frontBlock_->forward(blockFront);
            std::atomic_thread_fence(std::memory_order_release);
            frontBlock_->store_front(blockFront);
            return true;
            }

        if (frontBlock_ != tailBlock.load())
            {  // 当前前端块为空但存在下一个块：
            std::atomic_thread_fence(std::memory_order_acquire);

            frontBlock_ = frontBlock.load();
            blockTail = frontBlock_->get_local_tail_from_tail();
            blockFront = frontBlock_->get_front();
            std::atomic_thread_fence(std::memory_order_acquire);

            if (blockFront != blockTail)
                {
                auto element = frontBlock_->get_element_at_idx(blockFront);
                result = std::move(*element);
                element->~T();

                blockFront = frontBlock_->forward(blockFront);

                std::atomic_thread_fence(std::memory_order_release);
                frontBlock_->store_front(blockFront);
                return true;
                }

            Block* nextBlock = frontBlock_->next_block();
            size_t nextBlockFront = nextBlock->get_front();
            size_t nextBlockTail = nextBlock->get_local_tail_from_tail();

            std::atomic_thread_fence(std::memory_order_acquire);
            assert(nextBlockFront != nextBlockTail);
            std::atomic_thread_fence(std::memory_order_release);

            frontBlock = frontBlock_ = nextBlock;
            auto element = frontBlock_->get_element_at_idx(nextBlockFront);
            result = std::move(*element);
            element->~T();

            nextBlockFront = frontBlock_->forward(nextBlockFront);

            std::atomic_thread_fence(std::memory_order_release);
            frontBlock_->store_front(nextBlockFront);
        } else { // 无可用块：
            return false;
        }

        return true;
    }

    template<typename T, size_t MAX_BLOCK_SIZE>
    template<typename U>
    bool ReaderWriterQueue<T, MAX_BLOCK_SIZE>::inner_enqueue(U&& element)
    {
        ReentrantGuard guard(this->enqueuing);

        // 获取当前尾部块的状态：
        Block* tailBlock_ = tailBlock.load();
        std::size_t blockFront = tailBlock_->get_local_front();
        std::size_t blockTail = tailBlock_->get_tail();
        std::size_t nextBlockTail = tailBlock_->forward(blockTail);

        if(nextBlockTail != blockFront || nextBlockTail != tailBlock_->get_local_front_from_front())
        { // 检查尾部块是否有足够空间：
            std::atomic_thread_fence(std::memory_order_acquire);
            tailBlock_->construct_element_at_idx(blockTail, std::forward<U>(element));

            std:std::atomic_thread_fence(std::memory_order_release);
            tailBlock_->store_tail(nextBlockTail);
        }
        else
        {
            std::atomic_thread_fence(std::memory_order_acquire);
            if(tailBlock_->next_block() != frontBlock)
            {
                // 尾部块已满但存在下一个块：
                std::atomic_thread_fence(std::memory_order_acquire);
                Block* tailBlockNext = tailBlock_->next_block();
                std::size_t nextBlockFront = tailBlockNext->get_local_front_from_front();
                nextBlockTail = tailBlockNext->get_tail();
                std::atomic_thread_fence(std::memory_order_acquire);

                assert(nextBlockTail != nextBlockFront);
                tailBlockNext->construct_element_at_idx(nextBlockTail, std::forward<U>(element));

                tailBlockNext->store_tail(tailBlockNext->forward(nextBlockTail));

                std::atomic_thread_fence(std::memory_order_release);
                tailBlock = tailBlockNext;
            }
            else //  尾部块已满且没有可用块：
            {
                auto newBlockSize = largestBlockSize >= MAX_BLOCK_SIZE ? largestBlockSize : largestBlockSize * 2;


                Block* newBlock;
                try
                {
                    newBlock = Block::make_block(newBlockSize);
                }
                catch (const std::bad_alloc& e)
                {
                    return false;
                }

                largestBlockSize = newBlockSize;

                newBlock->construct_element_at_idx(0, std::forward<U>(element));
                assert(newBlock->get_front() == 0);
                newBlock->store_tail(1);
                auto tmp = newBlock->get_local_tail_from_tail();

                newBlock->store_next(tailBlock_->next_block());
                tailBlock_->store_next(newBlock);

                std::atomic_thread_fence(std::memory_order_release);
                tailBlock = newBlock;
            }
        }
        return true;
    }

//
// template<typename T, size_t MAX_BLOCK_SIZE = 512>
// class BlockingReaderWriterQueue {
// private:
//     typedef ReaderWriterQueue<T, MAX_BLOCK_SIZE> ReaderWriterQueue;
//
// public:
//     explicit BlockingReaderWriterQueue(size_t size = 15)
//         : inner(size), sema(new spsc_sema::LightweightSemaphore()) {}
//
//     BlockingReaderWriterQueue(BlockingReaderWriterQueue&& other)
//         : inner(std::move(other.inner)), sema(std::move(other.sema)) {}
//
//     BlockingReaderWriterQueue& operator=(BlockingReaderWriterQueue&& other) {
//         std::swap(sema, other.sema);
//         std::swap(inner, other.inner);
//         return *this;
//     }
//
//     bool try_enqueue(T const& element) {
//         if (inner.try_enqueue(element)) {
//             sema->signal();
//             return true;
//         }
//         return false;
//     }
//
//     bool try_enqueue(T&& element) {
//         if (inner.try_enqueue(std::forward<T>(element))) {
//             sema->signal();
//             return true;
//         }
//         return false;
//     }
//
//     template<typename... Args>
//     bool try_emplace(Args&&... args) {
//         if (inner.try_emplace(std::forward<Args>(args)...)) {
//             sema->signal();
//             return true;
//         }
//         return false;
//     }
//
//     bool enqueue(T const& element) {
//         if (inner.enqueue(element)) {
//             sema->signal();
//             return true;
//         }
//         return false;
//     }
//
//     bool enqueue(T&& element) {
//         if (inner.enqueue(std::forward<T>(element))) {
//             sema->signal();
//             return true;
//         }
//         return false;
//     }
//
//     template<typename... Args>
//     bool emplace(Args&&... args) {
//         if (inner.emplace(std::forward<Args>(args)...)) {
//             sema->signal();
//             return true;
//         }
//         return false;
//     }
//
//     template<typename U>
//     bool try_dequeue(U& result) {
//         if (sema->tryWait()) {
//             bool success = inner.try_dequeue(result);
//             assert(success);
//             return true;
//         }
//         return false;
//     }
//
//     template<typename U>
//     void wait_dequeue(U& result) {
//         while (!sema->wait());
//         bool success = inner.try_dequeue(result);
//         assert(success);
//     }
//
//     template<typename U, typename Rep, typename Period>
//     bool wait_dequeue_timed(U& result, std::chrono::duration<Rep, Period> const& timeout) {
//         return wait_dequeue_timed(result, std::chrono::duration_cast<std::chrono::microseconds>(timeout).count());
//     }
//
//     bool wait_dequeue_timed(U& result, std::int64_t timeout_usecs) {
//         if (!sema->wait(timeout_usecs)) {
//             return false;
//         }
//         bool success = inner.try_dequeue(result);
//         assert(success);
//         return success;
//     }
//
//     T* peek() const {
//         return inner.peek();
//     }
//
//     bool pop() {
//         if (sema->tryWait()) {
//             bool result = inner.pop();
//             assert(result);
//             return true;
//         }
//         return false;
//     }
//
//     size_t size_approx() const {
//         return sema->availableApprox();
//     }
//
//     size_t max_capacity() const {
//         return inner.max_capacity();
//     }
//
// private:
//     BlockingReaderWriterQueue(BlockingReaderWriterQueue const&) = delete;
//     BlockingReaderWriterQueue& operator=(BlockingReaderWriterQueue const&) = delete;
//
//     ReaderWriterQueue inner;
//     std::unique_ptr<spsc_sema::LightweightSemaphore> sema;
// };

}

#endif
