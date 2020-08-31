// Copyright 2020 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dawn_native/d3d12/ShaderVisibleDescriptorAllocatorD3D12.h"
#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/GPUDescriptorHeapAllocationD3D12.h"

namespace dawn_native { namespace d3d12 {

    // Thresholds should be adjusted (lower == faster) to avoid tests taking too long to complete.
    static constexpr const uint32_t kShaderVisibleSmallHeapSizes[] = {1024, 512};

    uint32_t GetD3D12ShaderVisibleHeapSize(D3D12_DESCRIPTOR_HEAP_TYPE heapType, bool useSmallSize) {
        if (useSmallSize) {
            return kShaderVisibleSmallHeapSizes[heapType];
        }

        switch (heapType) {
            case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
                return D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1;
            case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
                return D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
            default:
                UNREACHABLE();
        }
    }

    D3D12_DESCRIPTOR_HEAP_FLAGS GetD3D12HeapFlags(D3D12_DESCRIPTOR_HEAP_TYPE heapType) {
        switch (heapType) {
            case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
            case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
                return D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            default:
                UNREACHABLE();
        }
    }

    // static
    ResultOrError<std::unique_ptr<ShaderVisibleDescriptorAllocator>>
    ShaderVisibleDescriptorAllocator::Create(Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType) {
        std::unique_ptr<ShaderVisibleDescriptorAllocator> allocator =
            std::make_unique<ShaderVisibleDescriptorAllocator>(device, heapType);
        DAWN_TRY(allocator->AllocateAndSwitchShaderVisibleHeap());
        return std::move(allocator);
    }

    ShaderVisibleDescriptorAllocator::ShaderVisibleDescriptorAllocator(
        Device* device,
        D3D12_DESCRIPTOR_HEAP_TYPE heapType)
        : mHeapType(heapType),
          mDevice(device),
          mSizeIncrement(device->GetD3D12Device()->GetDescriptorHandleIncrementSize(heapType)) {
        ASSERT(heapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
               heapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }

    bool ShaderVisibleDescriptorAllocator::AllocateGPUDescriptors(
        uint32_t descriptorCount,
        Serial pendingSerial,
        D3D12_CPU_DESCRIPTOR_HANDLE* baseCPUDescriptor,
        GPUDescriptorHeapAllocation* allocation) {
        ASSERT(mHeap != nullptr);
        const uint64_t startOffset = mAllocator.Allocate(descriptorCount, pendingSerial);
        if (startOffset == RingBufferAllocator::kInvalidOffset) {
            return false;
        }

        ID3D12DescriptorHeap* descriptorHeap = mHeap.Get();

        const uint64_t heapOffset = mSizeIncrement * startOffset;

        // Check for 32-bit overflow since CPU heap start handle uses size_t.
        const size_t cpuHeapStartPtr = descriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr;

        ASSERT(heapOffset <= std::numeric_limits<size_t>::max() - cpuHeapStartPtr);

        *baseCPUDescriptor = {cpuHeapStartPtr + static_cast<size_t>(heapOffset)};

        const D3D12_GPU_DESCRIPTOR_HANDLE baseGPUDescriptor = {
            descriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + heapOffset};

        // Record both the device and heap serials to determine later if the allocations are
        // still valid.
        *allocation = GPUDescriptorHeapAllocation{baseGPUDescriptor, pendingSerial, mHeapSerial};

        return true;
    }

    ID3D12DescriptorHeap* ShaderVisibleDescriptorAllocator::GetShaderVisibleHeap() const {
        return mHeap.Get();
    }

    void ShaderVisibleDescriptorAllocator::Tick(uint64_t completedSerial) {
        mAllocator.Deallocate(completedSerial);
    }

    // Creates a GPU descriptor heap that manages descriptors in a FIFO queue.
    MaybeError ShaderVisibleDescriptorAllocator::AllocateAndSwitchShaderVisibleHeap() {
        ComPtr<ID3D12DescriptorHeap> heap;
        // Return the switched out heap to the pool and retrieve the oldest heap that is no longer
        // used by GPU. This maintains a heap buffer to avoid frequently re-creating heaps for heavy
        // users.
        // TODO(dawn:256): Consider periodically triming to avoid OOM.
        if (mHeap != nullptr) {
            mPool.push_back({mDevice->GetPendingCommandSerial(), std::move(mHeap)});
        }

        // Recycle existing heap if possible.
        if (!mPool.empty() && mPool.front().heapSerial <= mDevice->GetCompletedCommandSerial()) {
            heap = std::move(mPool.front().heap);
            mPool.pop_front();
        }

        // TODO(bryan.bernhart@intel.com): Allocating to max heap size wastes memory
        // should the developer not allocate any bindings for the heap type.
        // Consider dynamically re-sizing GPU heaps.
        const uint32_t descriptorCount = GetD3D12ShaderVisibleHeapSize(
            mHeapType, mDevice->IsToggleEnabled(Toggle::UseD3D12SmallShaderVisibleHeapForTesting));

        if (heap == nullptr) {
            D3D12_DESCRIPTOR_HEAP_DESC heapDescriptor;
            heapDescriptor.Type = mHeapType;
            heapDescriptor.NumDescriptors = descriptorCount;
            heapDescriptor.Flags = GetD3D12HeapFlags(mHeapType);
            heapDescriptor.NodeMask = 0;
            DAWN_TRY(CheckOutOfMemoryHRESULT(mDevice->GetD3D12Device()->CreateDescriptorHeap(
                                                 &heapDescriptor, IID_PPV_ARGS(&heap)),
                                             "ID3D12Device::CreateDescriptorHeap"));
        }

        // Create a FIFO buffer from the recently created heap.
        mHeap = std::move(heap);
        mAllocator = RingBufferAllocator(descriptorCount);

        // Invalidate all bindgroup allocations on previously bound heaps by incrementing the heap
        // serial. When a bindgroup attempts to re-populate, it will compare with its recorded
        // heap serial.
        mHeapSerial++;

        return {};
    }

    Serial ShaderVisibleDescriptorAllocator::GetShaderVisibleHeapSerialForTesting() const {
        return mHeapSerial;
    }

    uint64_t ShaderVisibleDescriptorAllocator::GetShaderVisibleHeapSizeForTesting() const {
        return mAllocator.GetSize();
    }

    uint64_t ShaderVisibleDescriptorAllocator::GetShaderVisiblePoolSizeForTesting() const {
        return mPool.size();
    }

    bool ShaderVisibleDescriptorAllocator::IsAllocationStillValid(
        const GPUDescriptorHeapAllocation& allocation) const {
        // Consider valid if allocated for the pending submit and the shader visible heaps
        // have not switched over.
        return (allocation.GetLastUsageSerial() > mDevice->GetCompletedCommandSerial() &&
                allocation.GetHeapSerial() == mHeapSerial);
    }
}}  // namespace dawn_native::d3d12