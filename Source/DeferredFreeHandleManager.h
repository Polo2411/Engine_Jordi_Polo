#pragma once

#include <array>
#include <cstdint>
#include <cassert>

#include "HandleManager.h"

template <size_t Capacity>
class DeferredFreeHandleManager
{
    using Handle = uint32_t;

    struct DeferredItem
    {
        uint64_t frame = 0;
        Handle handle = 0;
    };

public:
    Handle allocHandle() { return handles.allocHandle(); }
    void freeHandle(Handle h) { handles.freeHandle(h); }

    bool validHandle(Handle h) const { return handles.validHandle(h); }
    uint32_t indexFromHandle(Handle h) const { return handles.indexFromHandle(h); }

    size_t getSize() const { return handles.getSize(); }
    size_t getFreeCount() const { return handles.getFreeCount(); }

    void deferRelease(Handle handle, uint64_t currentFrame)
    {
        assert(handles.validHandle(handle) && "Invalid handle");

        for (size_t i = 0; i < pendingCount; ++i)
        {
            if (pending[i].handle == handle)
            {
                pending[i].frame = currentFrame;
                return;
            }
        }

        assert(pendingCount < Capacity && "Deferred list overflow");
        pending[pendingCount++] = { currentFrame, handle };
    }

    void collectGarbage(uint64_t lastCompletedFrame)
    {
        for (size_t i = 0; i < pendingCount; )
        {
            if (pending[i].frame <= lastCompletedFrame)
            {
                handles.freeHandle(pending[i].handle);
                --pendingCount;
                pending[i] = pending[pendingCount];
            }
            else
            {
                ++i;
            }
        }
    }

    void forceCollectGarbage()
    {
        for (size_t i = 0; i < pendingCount; ++i)
            handles.freeHandle(pending[i].handle);

        pendingCount = 0;
    }

private:
    HandleManager<Capacity> handles;

    std::array<DeferredItem, Capacity> pending{};
    size_t pendingCount = 0;
};
