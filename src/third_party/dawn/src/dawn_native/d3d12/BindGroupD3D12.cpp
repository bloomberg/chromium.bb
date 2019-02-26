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
#include "dawn_native/d3d12/SamplerD3D12.h"
#include "dawn_native/d3d12/TextureD3D12.h"

#include "dawn_native/d3d12/DeviceD3D12.h"

namespace dawn_native { namespace d3d12 {

    BindGroup::BindGroup(BindGroupBuilder* builder) : BindGroupBase(builder) {
    }

    void BindGroup::RecordDescriptors(const DescriptorHeapHandle& cbvUavSrvHeapStart,
                                      uint32_t* cbvUavSrvHeapOffset,
                                      const DescriptorHeapHandle& samplerHeapStart,
                                      uint32_t* samplerHeapOffset,
                                      uint64_t serial,
                                      uint32_t indexInSubmit) {
        mHeapSerial = serial;
        mIndexInSubmit = indexInSubmit;

        const auto* bgl = ToBackend(GetLayout());
        const auto& layout = bgl->GetBindingInfo();

        // Save the offset to the start of the descriptor table in the heap
        mCbvUavSrvHeapOffset = *cbvUavSrvHeapOffset;
        mSamplerHeapOffset = *samplerHeapOffset;

        const auto& bindingOffsets = bgl->GetBindingOffsets();

        auto d3d12Device = ToBackend(GetDevice())->GetD3D12Device();
        for (uint32_t binding : IterateBitSet(layout.mask)) {
            switch (layout.types[binding]) {
                case dawn::BindingType::UniformBuffer: {
                    auto* view = ToBackend(GetBindingAsBufferView(binding));
                    auto& cbv = view->GetCBVDescriptor();
                    d3d12Device->CreateConstantBufferView(
                        &cbv, cbvUavSrvHeapStart.GetCPUHandle(*cbvUavSrvHeapOffset +
                                                              bindingOffsets[binding]));
                } break;
                case dawn::BindingType::StorageBuffer: {
                    auto* view = ToBackend(GetBindingAsBufferView(binding));
                    auto& uav = view->GetUAVDescriptor();
                    d3d12Device->CreateUnorderedAccessView(
                        ToBackend(view->GetBuffer())->GetD3D12Resource().Get(), nullptr, &uav,
                        cbvUavSrvHeapStart.GetCPUHandle(*cbvUavSrvHeapOffset +
                                                        bindingOffsets[binding]));
                } break;
                case dawn::BindingType::SampledTexture: {
                    auto* view = ToBackend(GetBindingAsTextureView(binding));
                    auto& srv = view->GetSRVDescriptor();
                    d3d12Device->CreateShaderResourceView(
                        ToBackend(view->GetTexture())->GetD3D12Resource(), &srv,
                        cbvUavSrvHeapStart.GetCPUHandle(*cbvUavSrvHeapOffset +
                                                        bindingOffsets[binding]));
                } break;
                case dawn::BindingType::Sampler: {
                    auto* sampler = ToBackend(GetBindingAsSampler(binding));
                    auto& samplerDesc = sampler->GetSamplerDescriptor();
                    d3d12Device->CreateSampler(
                        &samplerDesc, samplerHeapStart.GetCPUHandle(*samplerHeapOffset +
                                                                    bindingOffsets[binding]));
                } break;
            }
        }

        // Offset by the number of descriptors created
        *cbvUavSrvHeapOffset += bgl->GetCbvUavSrvDescriptorCount();
        *samplerHeapOffset += bgl->GetSamplerDescriptorCount();
    }

    uint32_t BindGroup::GetCbvUavSrvHeapOffset() const {
        return mCbvUavSrvHeapOffset;
    }

    uint32_t BindGroup::GetSamplerHeapOffset() const {
        return mSamplerHeapOffset;
    }

    uint64_t BindGroup::GetHeapSerial() const {
        return mHeapSerial;
    }
    uint32_t BindGroup::GetIndexInSubmit() const {
        return mIndexInSubmit;
    }

}}  // namespace dawn_native::d3d12
