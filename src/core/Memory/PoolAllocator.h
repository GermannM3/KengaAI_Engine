/**
 * @file PoolAllocator.h
 * @brief Фиксированный пул аллокатор для уменьшения фрагментации
 *
 * PROJECT_RULES.md. Фаза 2.3: Memory — базовый allocator.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

namespace kenga {

/**
 * @brief Пул фиксированных блоков BlockSize, NumBlocks штук
 *
 * allocate(n) возвращает память для n байт (округление до BlockSize).
 * deallocate возвращает блок в free list.
 */
template <size_t BlockSize, size_t NumBlocks>
class PoolAllocator {
public:
    PoolAllocator();
    ~PoolAllocator();

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    void* allocate(size_t n);
    void deallocate(void* p, size_t n) noexcept;

    static constexpr size_t block_size = BlockSize;
    static constexpr size_t num_blocks = NumBlocks;
    static constexpr size_t capacity = BlockSize * NumBlocks;

private:
    union Block {
        Block* next;
        std::byte storage[BlockSize];
    };

    std::byte* m_storage = nullptr;
    Block* m_free_list = nullptr;
};

template <size_t BlockSize, size_t NumBlocks>
PoolAllocator<BlockSize, NumBlocks>::PoolAllocator()
{
    static_assert(BlockSize >= sizeof(Block*), "BlockSize must fit a pointer");
    m_storage = static_cast<std::byte*>(::operator new(capacity));
    m_free_list = reinterpret_cast<Block*>(m_storage);
    Block* current = m_free_list;
    for (size_t i = 0; i < NumBlocks - 1; ++i) {
        current->next = reinterpret_cast<Block*>(reinterpret_cast<std::byte*>(current) + BlockSize);
        current = current->next;
    }
    current->next = nullptr;
}

template <size_t BlockSize, size_t NumBlocks>
PoolAllocator<BlockSize, NumBlocks>::~PoolAllocator()
{
    ::operator delete(m_storage);
    m_storage = nullptr;
    m_free_list = nullptr;
}

template <size_t BlockSize, size_t NumBlocks>
void* PoolAllocator<BlockSize, NumBlocks>::allocate(size_t n)
{
    if (n == 0 || n > BlockSize) {
        return nullptr;
    }
    if (m_free_list == nullptr) {
        return nullptr;
    }
    Block* block = m_free_list;
    m_free_list = block->next;
    return block;
}

template <size_t BlockSize, size_t NumBlocks>
void PoolAllocator<BlockSize, NumBlocks>::deallocate(void* p, size_t n) noexcept
{
    (void)n;
    if (p == nullptr) {
        return;
    }
    Block* block = static_cast<Block*>(p);
    block->next = m_free_list;
    m_free_list = block;
}

} // namespace kenga
