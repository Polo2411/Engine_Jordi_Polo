#include "Globals.h"
#include "ModuleRingBuffer.h"

#include "Application.h"
#include "D3D12Module.h"
#include "d3dx12.h"

bool ModuleRingBuffer::init()
{
    totalMemorySize = alignUp(kDefaultTotalSizeBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    D3D12Module* d3d12 = app ? app->getD3D12Module() : nullptr;
    if (!d3d12 || !d3d12->getDevice())
        return false;

    ID3D12Device* device = d3d12->getDevice();

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(totalMemorySize);

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&buffer)
    );

    if (FAILED(hr) || !buffer)
        return false;

    buffer->SetName(L"RingBuffer (Upload Heap)");

    CD3DX12_RANGE readRange(0, 0); // we do not read from CPU
    hr = buffer->Map(0, &readRange, reinterpret_cast<void**>(&bufferData));
    if (FAILED(hr) || !bufferData)
        return false;

    head = 0;
    tail = 0;
    totalAllocated = 0;

    for (unsigned i = 0; i < kFramesInFlight; ++i)
        allocatedInFrame[i] = 0;

    currentFrameIdx = d3d12->getCurrentBackBufferIndex() % kFramesInFlight;

    return true;
}

void ModuleRingBuffer::preRender()
{
    D3D12Module* d3d12 = app ? app->getD3D12Module() : nullptr;
    if (!d3d12)
        return;

    currentFrameIdx = d3d12->getCurrentBackBufferIndex() % kFramesInFlight;

    const size_t reclaimed = allocatedInFrame[currentFrameIdx];
    if (reclaimed > 0)
    {
        tail = (tail + reclaimed) % totalMemorySize;
        totalAllocated = (totalAllocated >= reclaimed) ? (totalAllocated - reclaimed) : 0;
        allocatedInFrame[currentFrameIdx] = 0;
    }
}

D3D12_GPU_VIRTUAL_ADDRESS ModuleRingBuffer::allocBufferRaw(const void* data, size_t size)
{
    if (!buffer || !bufferData || !data || size == 0)
        return 0;

    if ((size & (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)) != 0)
        return 0;

    if (size > totalMemorySize)
        return 0;

    if (totalAllocated + size > totalMemorySize)
    {
        // Out of space this frame; caller should handle 0 (skip draw / increase pool size).
        return 0;
    }

    auto commit = [&](size_t offset) -> D3D12_GPU_VIRTUAL_ADDRESS
        {
            memcpy(bufferData + offset, data, size);
            allocatedInFrame[currentFrameIdx] += size;
            totalAllocated += size;
            return buffer->GetGPUVirtualAddress() + offset;
        };

    // Case A: wrapped free space (tail < head) -> try from head to end
    if (tail < head)
    {
        const size_t availableToEnd = totalMemorySize - head;
        if (availableToEnd >= size)
        {
            const D3D12_GPU_VIRTUAL_ADDRESS addr = commit(head);
            head += size;
            return addr;
        }

        // Not enough contiguous space at end, wrap head to 0
        head = 0;
    }

    // Now tail >= head (or buffer empty). Compute available contiguous space.
    const bool empty = (tail == head) && (totalAllocated == 0);
    const size_t available = empty ? totalMemorySize : (tail - head);

    if (available >= size)
    {
        const D3D12_GPU_VIRTUAL_ADDRESS addr = commit(head);
        head += size;
        return addr;
    }

    return 0;
}
