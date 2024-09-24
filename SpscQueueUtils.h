//
// Created by blair on 2024/9/9.
//

#ifndef SPSCQUEUEUTILS_H
#define SPSCQUEUEUTILS_H

#include <atomic>

namespace sq
{

    constexpr size_t CACHE_LINE_SIZE = 64;

    template<typename T>
    class alignas(CACHE_LINE_SIZE) Block
    {
        public:
            Block(const std::size_t &_size, char* _raw_this, char* _data)
                :data(_data), size_mask(_size-1), raw_this(_raw_this)
            {}

            ~Block()
            {
                // 队列为空是front == tail;
                size_t block_front = front.load();
                size_t block_tail = tail.load();
                for(size_t i = block_front; i!=block_tail; i = (i+1) & size_mask)
                {
                    auto elememt = get_element_at_idx(i);
                    elememt->~T();
                }
                std::free(raw_this);
            }

            // 为块本身及其包含的所有元素分配足够的内存
            static Block* make_block(std::size_t capacity)
            {
                // 为块本身及其包含的所有元素分配足够的内存
                // 确保 Block 类型的内存地址对齐到正确的字节边界。
                auto size = sizeof(Block) + std::alignment_of_v<Block> - 1;
                size += sizeof(T) * capacity + std::alignment_of_v<T> - 1;

                auto newBlockRaw = static_cast<char*>(std::malloc(size));
                if(!newBlockRaw) throw std::bad_alloc();

                auto newBlockAligned = align_for<Block>(newBlockRaw);
                auto newBlockData = align_for<T>(newBlockAligned + sizeof(Block));
                return new (newBlockAligned) Block(capacity, newBlockRaw, newBlockData);
            }

            [[nodiscard]] char* get_raw_this() const {return raw_this;}
            T* get_element_at_idx(size_t idx) const {return reinterpret_cast<T*>(data + idx * sizeof(T));}

            template<typename U>
            void construct_element_at_idx(size_t idx, U&& t)
            {
                char *pos = data + idx * sizeof(T);
                new (pos) T(std::forward<U>(t));
            }
            [[nodiscard]] size_t get_front() const {return front.load();}
            [[nodiscard]] size_t get_tail() const {return tail.load();}
            [[nodiscard]] size_t get_local_front() const {return local_front;}
            [[nodiscard]] size_t get_local_tail() const {return local_tail;}
            [[nodiscard]] size_t get_local_front_from_front()
            {
                local_front = front.load();
                return local_front;
            }
            [[nodiscard]] size_t get_local_tail_from_tail()
            {
                local_tail = tail.load();
                return local_tail;
            }

            void store_front(size_t t) {front.store(t);}
            void store_tail(size_t t) {tail.store(t);}
            void store_next(Block * t){next.store(t);}

            [[nodiscard]] size_t forward(size_t i) const {return (i + 1) & size_mask;}

            Block* next_block() {return next.load();}

        private:
            template <typename U>
            static char* align_for(char* ptr)
            {
                const std::size_t alignment = alignof(U);
                char *res = ptr + (alignment - reinterpret_cast<uintptr_t>(ptr) % alignment) % alignment;
                return res;
            }

            Block& operator=(const Block &) = default;
            // 通过将高度竞争的变量放在自己的缓存行上来避免错误共享
            std::atomic<std::size_t> front{0UL};  // 元素从这里读取
            std::size_t local_tail{0};  // 一个无争议的 tail 副本，由消费者拥有
            char cacheline_filler0[
                CACHE_LINE_SIZE - sizeof(std::atomic<std::size_t>) - sizeof(std::size_t)]{};

            std::atomic<std::size_t> tail{0UL}; // （原子）元素在这里排队
            std::size_t local_front{0};
            // next 的竞争并不激烈，但我们不希望它与 tail （它是）位于同一缓存行上
            char cacheline_filler1[
                CACHE_LINE_SIZE - sizeof(std::atomic<std::size_t>) - sizeof(std::size_t)]{};

            std::atomic<Block*> next{nullptr};

            char* data;  // // 内容（在堆上）与 T 的对齐方式对齐
            const std::size_t size_mask;

            char* raw_this;

    };

    struct ReentrantGuard
    {
        explicit ReentrantGuard(std::atomic<bool>& _inSection)
        : inSection(_inSection)
        {
            inSection=true;
        }
        ~ReentrantGuard() {inSection=false;}
        ReentrantGuard& operator=(const ReentrantGuard&) = delete;
    private:
        std::atomic<bool> &inSection;
    };

}

#endif //SPSCQUEUEUTILS_H
