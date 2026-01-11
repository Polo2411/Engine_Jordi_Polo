#pragma once

#include <array>
#include <cstdint>
#include <cassert>

// 32-bit Handle = [8-bit generation | 24-bit index]
template <size_t Capacity>
class HandleManager
{
    static_assert(Capacity < (1u << 24), "Capacity must fit in 24 bits");
    using Handle = uint32_t;

    struct Node
    {
        uint32_t link = 0;  // free: next free index, allocated: self index
        uint8_t  gen = 0;  // generation for allocated nodes
    };

public:
    HandleManager()
    {
        // Slot 0 is reserved as invalid.
        freeHead = (Capacity > 1) ? 1u : uint32_t(Capacity);

        for (uint32_t i = 0; i < uint32_t(Capacity); ++i)
        {
            nodes[i].link = i + 1;
            nodes[i].gen = 0;
        }

        if (Capacity > 0)
            nodes[Capacity - 1].link = uint32_t(Capacity);

        if (Capacity > 0)
        {
            nodes[0].link = uint32_t(Capacity);
            nodes[0].gen = 0;
        }
    }

    Handle allocHandle()
    {
        assert(freeHead < Capacity && "Out of handles");

        if (freeHead >= Capacity)
            return 0;

        const uint32_t idx = freeHead;
        freeHead = nodes[idx].link;

        genCounter = uint8_t(genCounter + 1);
        if (genCounter == 0) genCounter = 1;

        nodes[idx].link = idx;
        nodes[idx].gen = genCounter;

        return pack(idx, genCounter);
    }

    void freeHandle(Handle handle)
    {
        assert(validHandle(handle) && "Invalid handle");

        const uint32_t idx = unpackIndex(handle);

        nodes[idx].link = freeHead;
        freeHead = idx;
    }

    bool validHandle(Handle handle) const
    {
        if (handle == 0)
            return false;

        const uint32_t idx = unpackIndex(handle);
        const uint8_t gen = unpackGen(handle);

        if (idx == 0 || idx >= Capacity)
            return false;

        return nodes[idx].link == idx && nodes[idx].gen == gen;
    }

    uint32_t indexFromHandle(Handle handle) const
    {
        assert(validHandle(handle));
        return unpackIndex(handle);
    }

    size_t getSize() const { return Capacity; }

    size_t getFreeCount() const
    {
        size_t count = 0;
        uint32_t it = freeHead;

        while (it < Capacity)
        {
            ++count;
            it = nodes[it].link;
        }

        return count;
    }

private:
    static Handle pack(uint32_t index, uint8_t gen)
    {
        return (uint32_t(gen) << 24) | (index & 0x00FFFFFFu);
    }

    static uint32_t unpackIndex(Handle handle)
    {
        return handle & 0x00FFFFFFu;
    }

    static uint8_t unpackGen(Handle handle)
    {
        return uint8_t((handle >> 24) & 0xFFu);
    }

private:
    std::array<Node, Capacity> nodes{};
    uint32_t freeHead = 0;
    uint8_t genCounter = 0;
};
