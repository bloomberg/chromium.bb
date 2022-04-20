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

#include "dawn/native/ComputePassEncoder.h"

#include "dawn/native/BindGroup.h"
#include "dawn/native/BindGroupLayout.h"
#include "dawn/native/Buffer.h"
#include "dawn/native/CommandEncoder.h"
#include "dawn/native/CommandValidation.h"
#include "dawn/native/Commands.h"
#include "dawn/native/ComputePipeline.h"
#include "dawn/native/Device.h"
#include "dawn/native/InternalPipelineStore.h"
#include "dawn/native/ObjectType_autogen.h"
#include "dawn/native/PassResourceUsageTracker.h"
#include "dawn/native/QuerySet.h"
#include "dawn/native/utils/WGPUHelpers.h"

namespace dawn::native {

    namespace {

        ResultOrError<ComputePipelineBase*> GetOrCreateIndirectDispatchValidationPipeline(
            DeviceBase* device) {
            InternalPipelineStore* store = device->GetInternalPipelineStore();

            if (store->dispatchIndirectValidationPipeline != nullptr) {
                return store->dispatchIndirectValidationPipeline.Get();
            }

            // TODO(https://crbug.com/dawn/1108): Propagate validation feedback from this
            // shader in various failure modes.
            // Type 'bool' cannot be used in storage class 'uniform' as it is non-host-shareable.
            Ref<ShaderModuleBase> shaderModule;
            DAWN_TRY_ASSIGN(shaderModule, utils::CreateShaderModule(device, R"(
                struct UniformParams {
                    maxComputeWorkgroupsPerDimension: u32;
                    clientOffsetInU32: u32;
                    enableValidation: u32;
                    duplicateNumWorkgroups: u32;
                };

                struct IndirectParams {
                    data: array<u32>;
                };

                struct ValidatedParams {
                    data: array<u32>;
                };

                @group(0) @binding(0) var<uniform> uniformParams: UniformParams;
                @group(0) @binding(1) var<storage, read_write> clientParams: IndirectParams;
                @group(0) @binding(2) var<storage, write> validatedParams: ValidatedParams;

                @stage(compute) @workgroup_size(1, 1, 1)
                fn main() {
                    for (var i = 0u; i < 3u; i = i + 1u) {
                        var numWorkgroups = clientParams.data[uniformParams.clientOffsetInU32 + i];
                        if (uniformParams.enableValidation > 0u &&
                            numWorkgroups > uniformParams.maxComputeWorkgroupsPerDimension) {
                            numWorkgroups = 0u;
                        }
                        validatedParams.data[i] = numWorkgroups;

                        if (uniformParams.duplicateNumWorkgroups > 0u) {
                             validatedParams.data[i + 3u] = numWorkgroups;
                        }
                    }
                }
            )"));

            Ref<BindGroupLayoutBase> bindGroupLayout;
            DAWN_TRY_ASSIGN(
                bindGroupLayout,
                utils::MakeBindGroupLayout(
                    device,
                    {
                        {0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},
                        {1, wgpu::ShaderStage::Compute, kInternalStorageBufferBinding},
                        {2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},
                    },
                    /* allowInternalBinding */ true));

            Ref<PipelineLayoutBase> pipelineLayout;
            DAWN_TRY_ASSIGN(pipelineLayout,
                            utils::MakeBasicPipelineLayout(device, bindGroupLayout));

            ComputePipelineDescriptor computePipelineDescriptor = {};
            computePipelineDescriptor.layout = pipelineLayout.Get();
            computePipelineDescriptor.compute.module = shaderModule.Get();
            computePipelineDescriptor.compute.entryPoint = "main";

            DAWN_TRY_ASSIGN(store->dispatchIndirectValidationPipeline,
                            device->CreateComputePipeline(&computePipelineDescriptor));

            return store->dispatchIndirectValidationPipeline.Get();
        }

    }  // namespace

    ComputePassEncoder::ComputePassEncoder(DeviceBase* device,
                                           const ComputePassDescriptor* descriptor,
                                           CommandEncoder* commandEncoder,
                                           EncodingContext* encodingContext,
                                           std::vector<TimestampWrite> timestampWritesAtEnd)
        : ProgrammableEncoder(device, descriptor->label, encodingContext),
          mCommandEncoder(commandEncoder),
          mTimestampWritesAtEnd(std::move(timestampWritesAtEnd)) {
        TrackInDevice();
    }

    // static
    Ref<ComputePassEncoder> ComputePassEncoder::Create(
        DeviceBase* device,
        const ComputePassDescriptor* descriptor,
        CommandEncoder* commandEncoder,
        EncodingContext* encodingContext,
        std::vector<TimestampWrite> timestampWritesAtEnd) {
        return AcquireRef(new ComputePassEncoder(device, descriptor, commandEncoder,
                                                 encodingContext, std::move(timestampWritesAtEnd)));
    }

    ComputePassEncoder::ComputePassEncoder(DeviceBase* device,
                                           CommandEncoder* commandEncoder,
                                           EncodingContext* encodingContext,
                                           ErrorTag errorTag)
        : ProgrammableEncoder(device, encodingContext, errorTag), mCommandEncoder(commandEncoder) {
    }

    // static
    Ref<ComputePassEncoder> ComputePassEncoder::MakeError(DeviceBase* device,
                                                          CommandEncoder* commandEncoder,
                                                          EncodingContext* encodingContext) {
        return AcquireRef(
            new ComputePassEncoder(device, commandEncoder, encodingContext, ObjectBase::kError));
    }

    void ComputePassEncoder::DestroyImpl() {
        // Ensure that the pass has exited. This is done for passes only since validation requires
        // they exit before destruction while bundles do not.
        mEncodingContext->EnsurePassExited(this);
    }

    ObjectType ComputePassEncoder::GetType() const {
        return ObjectType::ComputePassEncoder;
    }

    void ComputePassEncoder::APIEnd() {
        if (mEncodingContext->TryEncode(
                this,
                [&](CommandAllocator* allocator) -> MaybeError {
                    if (IsValidationEnabled()) {
                        DAWN_TRY(ValidateProgrammableEncoderEnd());
                    }

                    EndComputePassCmd* cmd =
                        allocator->Allocate<EndComputePassCmd>(Command::EndComputePass);
                    // The query availability has already been updated at the beginning of compute
                    // pass, and no need to do update here.
                    cmd->timestampWrites = std::move(mTimestampWritesAtEnd);

                    return {};
                },
                "encoding %s.End().", this)) {
            mEncodingContext->ExitComputePass(this, mUsageTracker.AcquireResourceUsage());
        }
    }

    void ComputePassEncoder::APIEndPass() {
        GetDevice()->EmitDeprecationWarning("endPass() has been deprecated. Use end() instead.");
        APIEnd();
    }

    void ComputePassEncoder::APIDispatch(uint32_t workgroupCountX,
                                         uint32_t workgroupCountY,
                                         uint32_t workgroupCountZ) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (IsValidationEnabled()) {
                    DAWN_TRY(mCommandBufferState.ValidateCanDispatch());

                    uint32_t workgroupsPerDimension =
                        GetDevice()->GetLimits().v1.maxComputeWorkgroupsPerDimension;

                    DAWN_INVALID_IF(workgroupCountX > workgroupsPerDimension,
                                    "Dispatch workgroup count X (%u) exceeds max compute "
                                    "workgroups per dimension (%u).",
                                    workgroupCountX, workgroupsPerDimension);

                    DAWN_INVALID_IF(workgroupCountY > workgroupsPerDimension,
                                    "Dispatch workgroup count Y (%u) exceeds max compute "
                                    "workgroups per dimension (%u).",
                                    workgroupCountY, workgroupsPerDimension);

                    DAWN_INVALID_IF(workgroupCountZ > workgroupsPerDimension,
                                    "Dispatch workgroup count Z (%u) exceeds max compute "
                                    "workgroups per dimension (%u).",
                                    workgroupCountZ, workgroupsPerDimension);
                }

                // Record the synchronization scope for Dispatch, which is just the current
                // bindgroups.
                AddDispatchSyncScope();

                DispatchCmd* dispatch = allocator->Allocate<DispatchCmd>(Command::Dispatch);
                dispatch->x = workgroupCountX;
                dispatch->y = workgroupCountY;
                dispatch->z = workgroupCountZ;

                return {};
            },
            "encoding %s.Dispatch(%u, %u, %u).", this, workgroupCountX, workgroupCountY,
            workgroupCountZ);
    }

    ResultOrError<std::pair<Ref<BufferBase>, uint64_t>>
    ComputePassEncoder::TransformIndirectDispatchBuffer(Ref<BufferBase> indirectBuffer,
                                                        uint64_t indirectOffset) {
        DeviceBase* device = GetDevice();

        const bool shouldDuplicateNumWorkgroups =
            device->ShouldDuplicateNumWorkgroupsForDispatchIndirect(
                mCommandBufferState.GetComputePipeline());
        if (!IsValidationEnabled() && !shouldDuplicateNumWorkgroups) {
            return std::make_pair(indirectBuffer, indirectOffset);
        }

        // Save the previous command buffer state so it can be restored after the
        // validation inserts additional commands.
        CommandBufferStateTracker previousState = mCommandBufferState;

        auto* const store = device->GetInternalPipelineStore();

        Ref<ComputePipelineBase> validationPipeline;
        DAWN_TRY_ASSIGN(validationPipeline, GetOrCreateIndirectDispatchValidationPipeline(device));

        Ref<BindGroupLayoutBase> layout;
        DAWN_TRY_ASSIGN(layout, validationPipeline->GetBindGroupLayout(0));

        uint32_t storageBufferOffsetAlignment =
            device->GetLimits().v1.minStorageBufferOffsetAlignment;

        // Let the offset be the indirectOffset, aligned down to |storageBufferOffsetAlignment|.
        const uint32_t clientOffsetFromAlignedBoundary =
            indirectOffset % storageBufferOffsetAlignment;
        const uint64_t clientOffsetAlignedDown = indirectOffset - clientOffsetFromAlignedBoundary;
        const uint64_t clientIndirectBindingOffset = clientOffsetAlignedDown;

        // Let the size of the binding be the additional offset, plus the size.
        const uint64_t clientIndirectBindingSize =
            kDispatchIndirectSize + clientOffsetFromAlignedBoundary;

        // Neither 'enableValidation' nor 'duplicateNumWorkgroups' can be declared as 'bool' as
        // currently in WGSL type 'bool' cannot be used in storage class 'uniform' as 'it is
        // non-host-shareable'.
        struct UniformParams {
            uint32_t maxComputeWorkgroupsPerDimension;
            uint32_t clientOffsetInU32;
            uint32_t enableValidation;
            uint32_t duplicateNumWorkgroups;
        };

        // Create a uniform buffer to hold parameters for the shader.
        Ref<BufferBase> uniformBuffer;
        {
            UniformParams params;
            params.maxComputeWorkgroupsPerDimension =
                device->GetLimits().v1.maxComputeWorkgroupsPerDimension;
            params.clientOffsetInU32 = clientOffsetFromAlignedBoundary / sizeof(uint32_t);
            params.enableValidation = static_cast<uint32_t>(IsValidationEnabled());
            params.duplicateNumWorkgroups = static_cast<uint32_t>(shouldDuplicateNumWorkgroups);

            DAWN_TRY_ASSIGN(uniformBuffer, utils::CreateBufferFromData(
                                               device, wgpu::BufferUsage::Uniform, {params}));
        }

        // Reserve space in the scratch buffer to hold the validated indirect params.
        ScratchBuffer& scratchBuffer = store->scratchIndirectStorage;
        const uint64_t scratchBufferSize =
            shouldDuplicateNumWorkgroups ? 2 * kDispatchIndirectSize : kDispatchIndirectSize;
        DAWN_TRY(scratchBuffer.EnsureCapacity(scratchBufferSize));
        Ref<BufferBase> validatedIndirectBuffer = scratchBuffer.GetBuffer();

        Ref<BindGroupBase> validationBindGroup;
        ASSERT(indirectBuffer->GetUsage() & kInternalStorageBuffer);
        DAWN_TRY_ASSIGN(validationBindGroup,
                        utils::MakeBindGroup(device, layout,
                                             {
                                                 {0, uniformBuffer},
                                                 {1, indirectBuffer, clientIndirectBindingOffset,
                                                  clientIndirectBindingSize},
                                                 {2, validatedIndirectBuffer, 0, scratchBufferSize},
                                             }));

        // Issue commands to validate the indirect buffer.
        APISetPipeline(validationPipeline.Get());
        APISetBindGroup(0, validationBindGroup.Get());
        APIDispatch(1);

        // Restore the state.
        RestoreCommandBufferState(std::move(previousState));

        // Return the new indirect buffer and indirect buffer offset.
        return std::make_pair(std::move(validatedIndirectBuffer), uint64_t(0));
    }

    void ComputePassEncoder::APIDispatchIndirect(BufferBase* indirectBuffer,
                                                 uint64_t indirectOffset) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (IsValidationEnabled()) {
                    DAWN_TRY(GetDevice()->ValidateObject(indirectBuffer));
                    DAWN_TRY(ValidateCanUseAs(indirectBuffer, wgpu::BufferUsage::Indirect));
                    DAWN_TRY(mCommandBufferState.ValidateCanDispatch());

                    DAWN_INVALID_IF(indirectOffset % 4 != 0,
                                    "Indirect offset (%u) is not a multiple of 4.", indirectOffset);

                    DAWN_INVALID_IF(
                        indirectOffset >= indirectBuffer->GetSize() ||
                            indirectOffset + kDispatchIndirectSize > indirectBuffer->GetSize(),
                        "Indirect offset (%u) and dispatch size (%u) exceeds the indirect buffer "
                        "size (%u).",
                        indirectOffset, kDispatchIndirectSize, indirectBuffer->GetSize());
                }

                SyncScopeUsageTracker scope;
                scope.BufferUsedAs(indirectBuffer, wgpu::BufferUsage::Indirect);
                mUsageTracker.AddReferencedBuffer(indirectBuffer);
                // TODO(crbug.com/dawn/1166): If validation is enabled, adding |indirectBuffer|
                // is needed for correct usage validation even though it will only be bound for
                // storage. This will unecessarily transition the |indirectBuffer| in
                // the backend.

                Ref<BufferBase> indirectBufferRef = indirectBuffer;

                // Get applied indirect buffer with necessary changes on the original indirect
                // buffer. For example,
                // - Validate each indirect dispatch with a single dispatch to copy the indirect
                //   buffer params into a scratch buffer if they're valid, and otherwise zero them
                //   out.
                // - Duplicate all the indirect dispatch parameters to support @num_workgroups on
                //   D3D12.
                // - Directly return the original indirect dispatch buffer if we don't need any
                //   transformations on it.
                // We could consider moving the validation earlier in the pass after the last
                // last point the indirect buffer was used with writable usage, as well as batch
                // validation for multiple dispatches into one, but inserting commands at
                // arbitrary points in the past is not possible right now.
                DAWN_TRY_ASSIGN(std::tie(indirectBufferRef, indirectOffset),
                                TransformIndirectDispatchBuffer(indirectBufferRef, indirectOffset));

                // If we have created a new scratch dispatch indirect buffer in
                // TransformIndirectDispatchBuffer(), we need to track it in mUsageTracker.
                if (indirectBufferRef.Get() != indirectBuffer) {
                    // |indirectBufferRef| was replaced with a scratch buffer. Add it to the
                    // synchronization scope.
                    scope.BufferUsedAs(indirectBufferRef.Get(), wgpu::BufferUsage::Indirect);
                    mUsageTracker.AddReferencedBuffer(indirectBufferRef.Get());
                }

                AddDispatchSyncScope(std::move(scope));

                DispatchIndirectCmd* dispatch =
                    allocator->Allocate<DispatchIndirectCmd>(Command::DispatchIndirect);
                dispatch->indirectBuffer = std::move(indirectBufferRef);
                dispatch->indirectOffset = indirectOffset;
                return {};
            },
            "encoding %s.DispatchIndirect(%s, %u).", this, indirectBuffer, indirectOffset);
    }

    void ComputePassEncoder::APISetPipeline(ComputePipelineBase* pipeline) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (IsValidationEnabled()) {
                    DAWN_TRY(GetDevice()->ValidateObject(pipeline));
                }

                mCommandBufferState.SetComputePipeline(pipeline);

                SetComputePipelineCmd* cmd =
                    allocator->Allocate<SetComputePipelineCmd>(Command::SetComputePipeline);
                cmd->pipeline = pipeline;

                return {};
            },
            "encoding %s.SetPipeline(%s).", this, pipeline);
    }

    void ComputePassEncoder::APISetBindGroup(uint32_t groupIndexIn,
                                             BindGroupBase* group,
                                             uint32_t dynamicOffsetCount,
                                             const uint32_t* dynamicOffsets) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                BindGroupIndex groupIndex(groupIndexIn);

                if (IsValidationEnabled()) {
                    DAWN_TRY(ValidateSetBindGroup(groupIndex, group, dynamicOffsetCount,
                                                  dynamicOffsets));
                }

                mUsageTracker.AddResourcesReferencedByBindGroup(group);
                RecordSetBindGroup(allocator, groupIndex, group, dynamicOffsetCount,
                                   dynamicOffsets);
                mCommandBufferState.SetBindGroup(groupIndex, group, dynamicOffsetCount,
                                                 dynamicOffsets);

                return {};
            },
            "encoding %s.SetBindGroup(%u, %s, %u, ...).", this, groupIndexIn, group,
            dynamicOffsetCount);
    }

    void ComputePassEncoder::APIWriteTimestamp(QuerySetBase* querySet, uint32_t queryIndex) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (IsValidationEnabled()) {
                    DAWN_TRY(ValidateTimestampQuery(GetDevice(), querySet, queryIndex));
                }

                mCommandEncoder->TrackQueryAvailability(querySet, queryIndex);

                WriteTimestampCmd* cmd =
                    allocator->Allocate<WriteTimestampCmd>(Command::WriteTimestamp);
                cmd->querySet = querySet;
                cmd->queryIndex = queryIndex;

                return {};
            },
            "encoding %s.WriteTimestamp(%s, %u).", this, querySet, queryIndex);
    }

    void ComputePassEncoder::AddDispatchSyncScope(SyncScopeUsageTracker scope) {
        PipelineLayoutBase* layout = mCommandBufferState.GetPipelineLayout();
        for (BindGroupIndex i : IterateBitSet(layout->GetBindGroupLayoutsMask())) {
            scope.AddBindGroup(mCommandBufferState.GetBindGroup(i));
        }
        mUsageTracker.AddDispatch(scope.AcquireSyncScopeUsage());
    }

    void ComputePassEncoder::RestoreCommandBufferState(CommandBufferStateTracker state) {
        // Encode commands for the backend to restore the pipeline and bind groups.
        if (state.HasPipeline()) {
            APISetPipeline(state.GetComputePipeline());
        }
        for (BindGroupIndex i(0); i < kMaxBindGroupsTyped; ++i) {
            BindGroupBase* bg = state.GetBindGroup(i);
            if (bg != nullptr) {
                const std::vector<uint32_t>& offsets = state.GetDynamicOffsets(i);
                if (offsets.empty()) {
                    APISetBindGroup(static_cast<uint32_t>(i), bg);
                } else {
                    APISetBindGroup(static_cast<uint32_t>(i), bg, offsets.size(), offsets.data());
                }
            }
        }

        // Restore the frontend state tracking information.
        mCommandBufferState = std::move(state);
    }

    CommandBufferStateTracker* ComputePassEncoder::GetCommandBufferStateTrackerForTesting() {
        return &mCommandBufferState;
    }

}  // namespace dawn::native
