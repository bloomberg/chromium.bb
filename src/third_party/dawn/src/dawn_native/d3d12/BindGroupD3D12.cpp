// Copyright 2017 The Dawn Authors
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

#include "dawn_native/d3d12/BindGroupD3D12.h"

#include "common/BitSetIterator.h"
#include "dawn_native/d3d12/BindGroupLayoutD3D12.h"
#include "dawn_native/d3d12/BufferD3D12.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/SamplerD3D12.h"
#include "dawn_native/d3d12/ShaderVisibleDescriptorAllocatorD3D12.h"
#include "dawn_native/d3d12/TextureD3D12.h"

namespace dawn_native { namespace d3d12 {

    // static
    ResultOrError<BindGroup*> BindGroup::Create(Device* device,
                                                const BindGroupDescriptor* descriptor) {
        return ToBackend(descriptor->layout)->AllocateBindGroup(device, descriptor);
    }

    BindGroup::BindGroup(Device* device,
                         const BindGroupDescriptor* descriptor,
                         uint32_t viewSizeIncrement,
                         const CPUDescriptorHeapAllocation& viewAllocation,
                         uint32_t samplerSizeIncrement,
                         const CPUDescriptorHeapAllocation& samplerAllocation)
        : BindGroupBase(this, device, descriptor) {
        BindGroupLayout* bgl = ToBackend(GetLayout());

        mCPUViewAllocation = viewAllocation;
        mCPUSamplerAllocation = samplerAllocation;

        const auto& bindingOffsets = bgl->GetBindingOffsets();

        ID3D12Device* d3d12Device = device->GetD3D12Device();

        // It's not necessary to create descriptors in the descriptor heap for dynamic resources.
        // This is because they are created as root descriptors which are never heap allocated.
        // Since dynamic buffers are packed in the front, we can skip over these bindings by
        // starting from the dynamic buffer count.
        for (BindingIndex bindingIndex = bgl->GetDynamicBufferCount();
             bindingIndex < bgl->GetBindingCount(); ++bindingIndex) {
            const BindingInfo& bindingInfo = bgl->GetBindingInfo(bindingIndex);

            // Increment size does not need to be stored and is only used to get a handle
            // local to the allocation with OffsetFrom().
            switch (bindingInfo.type) {
                case wgpu::BindingType::UniformBuffer: {
                    BufferBinding binding = GetBindingAsBufferBinding(bindingIndex);

                    D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
                    // TODO(enga@google.com): investigate if this needs to be a constraint at
                    // the API level
                    desc.SizeInBytes = Align(binding.size, 256);
                    desc.BufferLocation = ToBackend(binding.buffer)->GetVA() + binding.offset;

                    d3d12Device->CreateConstantBufferView(
                        &desc,
                        viewAllocation.OffsetFrom(viewSizeIncrement, bindingOffsets[bindingIndex]));
                    break;
                }
                case wgpu::BindingType::StorageBuffer: {
                    BufferBinding binding = GetBindingAsBufferBinding(bindingIndex);

                    // Since SPIRV-Cross outputs HLSL shaders with RWByteAddressBuffer,
                    // we must use D3D12_BUFFER_UAV_FLAG_RAW when making the
                    // UNORDERED_ACCESS_VIEW_DESC. Using D3D12_BUFFER_UAV_FLAG_RAW requires
                    // that we use DXGI_FORMAT_R32_TYPELESS as the format of the view.
                    // DXGI_FORMAT_R32_TYPELESS requires that the element size be 4
                    // byte aligned. Since binding.size and binding.offset are in bytes,
                    // we need to divide by 4 to obtain the element size.
                    D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
                    desc.Buffer.NumElements = binding.size / 4;
                    desc.Format = DXGI_FORMAT_R32_TYPELESS;
                    desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                    desc.Buffer.FirstElement = binding.offset / 4;
                    desc.Buffer.StructureByteStride = 0;
                    desc.Buffer.CounterOffsetInBytes = 0;
                    desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

                    d3d12Device->CreateUnorderedAccessView(
                        ToBackend(binding.buffer)->GetD3D12Resource().Get(), nullptr, &desc,
                        viewAllocation.OffsetFrom(viewSizeIncrement, bindingOffsets[bindingIndex]));
                    break;
                }
                case wgpu::BindingType::ReadonlyStorageBuffer: {
                    BufferBinding binding = GetBindingAsBufferBinding(bindingIndex);

                    // Like StorageBuffer, SPIRV-Cross outputs HLSL shaders for readonly storage
                    // buffer with ByteAddressBuffer. So we must use D3D12_BUFFER_SRV_FLAG_RAW
                    // when making the SRV descriptor. And it has similar requirement for
                    // format, element size, etc.
                    D3D12_SHADER_RESOURCE_VIEW_DESC desc;
                    desc.Format = DXGI_FORMAT_R32_TYPELESS;
                    desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    desc.Buffer.FirstElement = binding.offset / 4;
                    desc.Buffer.NumElements = binding.size / 4;
                    desc.Buffer.StructureByteStride = 0;
                    desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
                    d3d12Device->CreateShaderResourceView(
                        ToBackend(binding.buffer)->GetD3D12Resource().Get(), &desc,
                        viewAllocation.OffsetFrom(viewSizeIncrement, bindingOffsets[bindingIndex]));
                    break;
                }

                // Readonly storage is implemented as SRV so it can be used at the same time as a
                // sampled texture.
                case wgpu::BindingType::SampledTexture:
                case wgpu::BindingType::ReadonlyStorageTexture: {
                    auto* view = ToBackend(GetBindingAsTextureView(bindingIndex));
                    auto& srv = view->GetSRVDescriptor();
                    d3d12Device->CreateShaderResourceView(
                        ToBackend(view->GetTexture())->GetD3D12Resource(), &srv,
                        viewAllocation.OffsetFrom(viewSizeIncrement, bindingOffsets[bindingIndex]));
                    break;
                }
                case wgpu::BindingType::Sampler:
                case wgpu::BindingType::ComparisonSampler: {
                    auto* sampler = ToBackend(GetBindingAsSampler(bindingIndex));
                    auto& samplerDesc = sampler->GetSamplerDescriptor();
                    d3d12Device->CreateSampler(
                        &samplerDesc, samplerAllocation.OffsetFrom(samplerSizeIncrement,
                                                                   bindingOffsets[bindingIndex]));
                    break;
                }

                case wgpu::BindingType::WriteonlyStorageTexture: {
                    TextureView* view = ToBackend(GetBindingAsTextureView(bindingIndex));
                    D3D12_UNORDERED_ACCESS_VIEW_DESC uav = view->GetUAVDescriptor();
                    d3d12Device->CreateUnorderedAccessView(
                        ToBackend(view->GetTexture())->GetD3D12Resource(), nullptr, &uav,
                        viewAllocation.OffsetFrom(viewSizeIncrement, bindingOffsets[bindingIndex]));
                    break;
                }

                case wgpu::BindingType::StorageTexture:
                    UNREACHABLE();
                    break;

                    // TODO(shaobo.yan@intel.com): Implement dynamic buffer offset.
            }
        }
    }

    BindGroup::~BindGroup() {
        ToBackend(GetLayout())
            ->DeallocateBindGroup(this, &mCPUViewAllocation, &mCPUSamplerAllocation);
        ASSERT(!mCPUViewAllocation.IsValid());
        ASSERT(!mCPUSamplerAllocation.IsValid());
    }

    bool BindGroup::PopulateViews(ShaderVisibleDescriptorAllocator* viewAllocator) {
        const BindGroupLayout* bgl = ToBackend(GetLayout());
        return Populate(viewAllocator, bgl->GetCbvUavSrvDescriptorCount(),
                        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, mCPUViewAllocation,
                        &mGPUViewAllocation);
    }

    bool BindGroup::PopulateSamplers(ShaderVisibleDescriptorAllocator* samplerAllocator) {
        const BindGroupLayout* bgl = ToBackend(GetLayout());
        return Populate(samplerAllocator, bgl->GetSamplerDescriptorCount(),
                        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, mCPUSamplerAllocation,
                        &mGPUSamplerAllocation);
    }

    bool BindGroup::Populate(ShaderVisibleDescriptorAllocator* allocator,
                             uint32_t descriptorCount,
                             D3D12_DESCRIPTOR_HEAP_TYPE heapType,
                             const CPUDescriptorHeapAllocation& stagingAllocation,
                             GPUDescriptorHeapAllocation* allocation) {
        if (descriptorCount == 0 || allocator->IsAllocationStillValid(*allocation)) {
            return true;
        }

        // Attempt to allocate descriptors for the currently bound shader-visible heaps.
        // If either failed, return early to re-allocate and switch the heaps.
        Device* device = ToBackend(GetDevice());

        D3D12_CPU_DESCRIPTOR_HANDLE baseCPUDescriptor;
        if (!allocator->AllocateGPUDescriptors(descriptorCount, device->GetPendingCommandSerial(),
                                               &baseCPUDescriptor, allocation)) {
            return false;
        }

        // CPU bindgroups are sparsely allocated across CPU heaps. Instead of doing
        // simple copies per bindgroup, a single non-simple copy could be issued.
        // TODO(dawn:155): Consider doing this optimization.
        device->GetD3D12Device()->CopyDescriptorsSimple(
            descriptorCount, baseCPUDescriptor, stagingAllocation.GetBaseDescriptor(), heapType);

        return true;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE BindGroup::GetBaseViewDescriptor() const {
        return mGPUViewAllocation.GetBaseDescriptor();
    }

    D3D12_GPU_DESCRIPTOR_HANDLE BindGroup::GetBaseSamplerDescriptor() const {
        return mGPUSamplerAllocation.GetBaseDescriptor();
    }
}}  // namespace dawn_native::d3d12
