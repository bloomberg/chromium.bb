// Copyright 2019 The Dawn Authors
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

#ifndef DAWNNATIVE_RENDERENCODERBASE_H_
#define DAWNNATIVE_RENDERENCODERBASE_H_

#include "dawn_native/AttachmentState.h"
#include "dawn_native/CommandBufferStateTracker.h"
#include "dawn_native/Error.h"
#include "dawn_native/IndirectDrawMetadata.h"
#include "dawn_native/PassResourceUsageTracker.h"
#include "dawn_native/ProgrammablePassEncoder.h"

namespace dawn_native {

    class RenderEncoderBase : public ProgrammablePassEncoder {
      public:
        RenderEncoderBase(DeviceBase* device,
                          EncodingContext* encodingContext,
                          Ref<AttachmentState> attachmentState);

        void APIDraw(uint32_t vertexCount,
                     uint32_t instanceCount = 1,
                     uint32_t firstVertex = 0,
                     uint32_t firstInstance = 0);
        void APIDrawIndexed(uint32_t vertexCount,
                            uint32_t instanceCount,
                            uint32_t firstIndex,
                            int32_t baseVertex,
                            uint32_t firstInstance);

        void APIDrawIndirect(BufferBase* indirectBuffer, uint64_t indirectOffset);
        void APIDrawIndexedIndirect(BufferBase* indirectBuffer, uint64_t indirectOffset);

        void APISetPipeline(RenderPipelineBase* pipeline);

        void APISetVertexBuffer(uint32_t slot, BufferBase* buffer, uint64_t offset, uint64_t size);
        void APISetIndexBuffer(BufferBase* buffer,
                               wgpu::IndexFormat format,
                               uint64_t offset,
                               uint64_t size);

        void APISetBindGroup(uint32_t groupIndex,
                             BindGroupBase* group,
                             uint32_t dynamicOffsetCount = 0,
                             const uint32_t* dynamicOffsets = nullptr);

        const AttachmentState* GetAttachmentState() const;
        Ref<AttachmentState> AcquireAttachmentState();

      protected:
        // Construct an "error" render encoder base.
        RenderEncoderBase(DeviceBase* device, EncodingContext* encodingContext, ErrorTag errorTag);

        CommandBufferStateTracker mCommandBufferState;
        RenderPassResourceUsageTracker mUsageTracker;
        IndirectDrawMetadata mIndirectDrawMetadata;

      private:
        Ref<AttachmentState> mAttachmentState;
        const bool mDisableBaseVertex;
        const bool mDisableBaseInstance;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_RENDERENCODERBASE_H_
