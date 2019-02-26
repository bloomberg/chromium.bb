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

#ifndef DAWNNATIVE_D3D12_RENDERPASSDESCRIPTORD3D12_H_
#define DAWNNATIVE_D3D12_RENDERPASSDESCRIPTORD3D12_H_

#include "dawn_native/RenderPassDescriptor.h"

#include "common/Constants.h"
#include "dawn_native/d3d12/DescriptorHeapAllocator.h"
#include "dawn_native/d3d12/d3d12_platform.h"

#include <array>
#include <vector>

namespace dawn_native { namespace d3d12 {

    class Device;

    class RenderPassDescriptor : public RenderPassDescriptorBase {
      public:
        struct OMSetRenderTargetArgs {
            unsigned int numRTVs = 0;
            std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kMaxColorAttachments> RTVs = {};
            D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};
        };

        RenderPassDescriptor(RenderPassDescriptorBuilder* builder);
        OMSetRenderTargetArgs GetSubpassOMSetRenderTargetArgs();
        D3D12_CPU_DESCRIPTOR_HANDLE GetRTVDescriptor(uint32_t attachmentSlot);
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSVDescriptor();

      private:
        DescriptorHeapHandle mRtvHeap = {};
        DescriptorHeapHandle mDsvHeap = {};
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_RENDERPASSDESCRIPTORD3D12_H_
