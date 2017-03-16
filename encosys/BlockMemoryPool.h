#pragma once

#include <vector>

namespace ECS {

class BlockMemoryPool {
public:
    explicit BlockMemoryPool (uint32_t elementSize, uint32_t blockSize)
        : m_elementSize{elementSize}, m_blockSize{blockSize} {}

    virtual ~BlockMemoryPool ();

    uint32_t GetElementSize () const { return m_elementSize; }
    uint32_t GetBlockSize () const { return m_blockSize;}
    uint32_t GetCapacity () const { return m_capacity; }

    void Reserve (uint32_t newCapacity);
    void* Get (uint32_t index);
    const void* Get (uint32_t index) const;
    virtual void Destroy (uint32_t index);

private:
    uint32_t m_elementSize{0};
    uint32_t m_blockSize{0};
    uint32_t m_capacity{0};
    std::vector<char*> m_blocks{};
};

}