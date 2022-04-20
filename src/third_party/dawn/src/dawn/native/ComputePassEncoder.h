// Copyright 2018 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_COMPUTEPASSENCODER_H_
#define SRC_DAWN_NATIVE_COMPUTEPASSENCODER_H_

#include "dawn/native/CommandBufferStateTracker.h"
#include "dawn/native/Error.h"
#include "dawn/native/Forward.h"
#include "dawn/native/PassResourceUsageTracker.h"
#include "dawn/native/ProgrammableEncoder.h"

namespace dawn::native {

    class SyncScopeUsageTracker;

    class ComputePassEncoder final : public ProgrammableEncoder {
      public:
        static Ref<ComputePassEncoder> Create(DeviceBase* device,
                                              const ComputePassDescriptor* descriptor,
                                              CommandEncoder* commandEncoder,
                                              EncodingContext* encodingContext,
                                              std::vector<TimestampWrite> timestampWritesAtEnd);
        static Ref<ComputePassEncoder> MakeError(DeviceBase* device,
                                                 CommandEncoder* commandEncoder,
                                                 EncodingContext* encodingContext);

        ObjectType GetType() const override;

        void APIEnd();
        void APIEndPass();  // TODO(dawn:1286): Remove after deprecation period.

        void APIDispatch(uint32_t workgroupCountX,
                         uint32_t workgroupCountY = 1,
                         uint32_t workgroupCountZ = 1);
        void APIDispatchIndirect(BufferBase* indirectBuffer, uint64_t indirectOffset);
        void APISetPipeline(ComputePipelineBase* pipeline);

        void APISetBindGroup(uint32_t groupIndex,
                             BindGroupBase* group,
                             uint32_t dynamicOffsetCount = 0,
                             const uint32_t* dynamicOffsets = nullptr);

        void APIWriteTimestamp(QuerySetBase* querySet, uint32_t queryIndex);

        CommandBufferStateTracker* GetCommandBufferStateTrackerForTesting();
        void RestoreCommandBufferStateForTesting(CommandBufferStateTracker state) {
            RestoreCommandBufferState(std::move(state));
        }

      protected:
        ComputePassEncoder(DeviceBase* device,
                           const ComputePassDescriptor* descriptor,
                           CommandEncoder* commandEncoder,
                           EncodingContext* encodingContext,
                           std::vector<TimestampWrite> timestampWritesAtEnd);
        ComputePassEncoder(DeviceBase* device,
                           CommandEncoder* commandEncoder,
                           EncodingContext* encodingContext,
                           ErrorTag errorTag);

      private:
        void DestroyImpl() override;

        ResultOrError<std::pair<Ref<BufferBase>, uint64_t>> TransformIndirectDispatchBuffer(
            Ref<BufferBase> indirectBuffer,
            uint64_t indirectOffset);

        void RestoreCommandBufferState(CommandBufferStateTracker state);

        CommandBufferStateTracker mCommandBufferState;

        // Adds the bindgroups used for the current dispatch to the SyncScopeResourceUsage and
        // records it in mUsageTracker.
        void AddDispatchSyncScope(SyncScopeUsageTracker scope = {});
        ComputePassResourceUsageTracker mUsageTracker;

        // For render and compute passes, the encoding context is borrowed from the command encoder.
        // Keep a reference to the encoder to make sure the context isn't freed.
        Ref<CommandEncoder> mCommandEncoder;

        std::vector<TimestampWrite> mTimestampWritesAtEnd;
    };

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_COMPUTEPASSENCODER_H_
