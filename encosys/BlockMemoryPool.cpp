#include "BlockMemoryPool.h"

#include <cstdio>

namespace ECS {

BlockMemoryPool::~BlockMemoryPool () {
    for (char* block : m_blocks) {
        delete[] block;
    }
}

void BlockMemoryPool::Reserve (uint32_t newCapacity) {
    while (m_capacity < newCapacity) {
        m_blocks.push_back(new char[m_elementSize * m_blockSize]);
        m_capacity += m_blockSize;
    }
}

void* BlockMemoryPool::Get (uint32_t index) {
    ECS_ASSERT_(index < m_capacity);
    return m_blocks[index / m_blockSize] + (index % m_blockSize) * m_elementSize;
}

const void* BlockMemoryPool::Get (uint32_t index) const {
    return m_blocks[index / m_blockSize] + (index % m_blockSize) * m_elementSize;
}

void BlockMemoryPool::Destroy (uint32_t index) {
    ECS_ASSERT_(index < m_capacity);
    memset(Get(index), 0, m_elementSize);
}

}