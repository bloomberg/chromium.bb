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

#include "dawn_native/d3d12/CommandBufferD3D12.h"

#include "dawn_native/BindGroupTracker.h"
#include "dawn_native/CommandValidation.h"
#include "dawn_native/DynamicUploader.h"
#include "dawn_native/Error.h"
#include "dawn_native/RenderBundle.h"
#include "dawn_native/d3d12/BindGroupD3D12.h"
#include "dawn_native/d3d12/BindGroupLayoutD3D12.h"
#include "dawn_native/d3d12/ComputePipelineD3D12.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/PipelineLayoutD3D12.h"
#include "dawn_native/d3d12/PlatformFunctions.h"
#include "dawn_native/d3d12/QuerySetD3D12.h"
#include "dawn_native/d3d12/RenderPassBuilderD3D12.h"
#include "dawn_native/d3d12/RenderPipelineD3D12.h"
#include "dawn_native/d3d12/ShaderVisibleDescriptorAllocatorD3D12.h"
#include "dawn_native/d3d12/StagingBufferD3D12.h"
#include "dawn_native/d3d12/StagingDescriptorAllocatorD3D12.h"
#include "dawn_native/d3d12/UtilsD3D12.h"

namespace dawn_native { namespace d3d12 {

    namespace {

        DXGI_FORMAT DXGIIndexFormat(wgpu::IndexFormat format) {
            switch (format) {
                case wgpu::IndexFormat::Undefined:
                    return DXGI_FORMAT_UNKNOWN;
                case wgpu::IndexFormat::Uint16:
                    return DXGI_FORMAT_R16_UINT;
                case wgpu::IndexFormat::Uint32:
                    return DXGI_FORMAT_R32_UINT;
            }
        }

        D3D12_QUERY_TYPE D3D12QueryType(wgpu::QueryType type) {
            switch (type) {
                case wgpu::QueryType::Occlusion:
                    return D3D12_QUERY_TYPE_BINARY_OCCLUSION;
                case wgpu::QueryType::PipelineStatistics:
                    return D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
                case wgpu::QueryType::Timestamp:
                    return D3D12_QUERY_TYPE_TIMESTAMP;
            }
        }

        bool CanUseCopyResource(const TextureCopy& src,
                                const TextureCopy& dst,
                                const Extent3D& copySize) {
            // Checked by validation
            ASSERT(src.texture->GetSampleCount() == dst.texture->GetSampleCount());
            ASSERT(src.texture->GetFormat().format == dst.texture->GetFormat().format);
            ASSERT(src.aspect == dst.aspect);

            const Extent3D& srcSize = src.texture->GetSize();
            const Extent3D& dstSize = dst.texture->GetSize();

            // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copyresource
            // In order to use D3D12's copy resource, the textures must be the same dimensions, and
            // the copy must be of the entire resource.
            // TODO(dawn:129): Support 1D textures.
            return src.aspect == src.texture->GetFormat().aspects &&
                   src.texture->GetDimension() == dst.texture->GetDimension() &&  //
                   dst.texture->GetNumMipLevels() == 1 &&                         //
                   src.texture->GetNumMipLevels() == 1 &&  // A copy command is of a single mip, so
                                                           // if a resource has more than one, we
                                                           // definitely cannot use CopyResource.
                   copySize.width == dstSize.width &&      //
                   copySize.width == srcSize.width &&      //
                   copySize.height == dstSize.height &&    //
                   copySize.height == srcSize.height &&    //
                   copySize.depthOrArrayLayers == dstSize.depthOrArrayLayers &&  //
                   copySize.depthOrArrayLayers == srcSize.depthOrArrayLayers;
        }

        void RecordWriteTimestampCmd(ID3D12GraphicsCommandList* commandList,
                                     WriteTimestampCmd* cmd) {
            QuerySet* querySet = ToBackend(cmd->querySet.Get());
            ASSERT(D3D12QueryType(querySet->GetQueryType()) == D3D12_QUERY_TYPE_TIMESTAMP);
            commandList->EndQuery(querySet->GetQueryHeap(), D3D12_QUERY_TYPE_TIMESTAMP,
                                  cmd->queryIndex);
        }

        void RecordResolveQuerySetCmd(ID3D12GraphicsCommandList* commandList,
                                      Device* device,
                                      QuerySet* querySet,
                                      uint32_t firstQuery,
                                      uint32_t queryCount,
                                      Buffer* destination,
                                      uint64_t destinationOffset) {
            const std::vector<bool>& availability = querySet->GetQueryAvailability();

            auto currentIt = availability.begin() + firstQuery;
            auto lastIt = availability.begin() + firstQuery + queryCount;

            // Traverse available queries in the range of [firstQuery, firstQuery +  queryCount - 1]
            while (currentIt != lastIt) {
                auto firstTrueIt = std::find(currentIt, lastIt, true);
                // No available query found for resolving
                if (firstTrueIt == lastIt) {
                    break;
                }
                auto nextFalseIt = std::find(firstTrueIt, lastIt, false);

                // The query index of firstTrueIt where the resolving starts
                uint32_t resolveQueryIndex = std::distance(availability.begin(), firstTrueIt);
                // The queries count between firstTrueIt and nextFalseIt need to be resolved
                uint32_t resolveQueryCount = std::distance(firstTrueIt, nextFalseIt);

                // Calculate destinationOffset based on the current resolveQueryIndex and firstQuery
                uint32_t resolveDestinationOffset =
                    destinationOffset + (resolveQueryIndex - firstQuery) * sizeof(uint64_t);

                // Resolve the queries between firstTrueIt and nextFalseIt (which is at most lastIt)
                commandList->ResolveQueryData(
                    querySet->GetQueryHeap(), D3D12QueryType(querySet->GetQueryType()),
                    resolveQueryIndex, resolveQueryCount, destination->GetD3D12Resource(),
                    resolveDestinationOffset);

                // Set current iterator to next false
                currentIt = nextFalseIt;
            }
        }

        MaybeError ClearBufferToZero(Device* device,
                                     Buffer* destination,
                                     uint64_t destinationOffset,
                                     uint64_t size) {
            DynamicUploader* uploader = device->GetDynamicUploader();
            UploadHandle uploadHandle;
            DAWN_TRY_ASSIGN(uploadHandle,
                            uploader->Allocate(size, device->GetPendingCommandSerial(),
                                               kCopyBufferToBufferOffsetAlignment));
            memset(uploadHandle.mappedBuffer, 0u, size);

            return device->CopyFromStagingToBuffer(uploadHandle.stagingBuffer,
                                                   uploadHandle.startOffset, destination,
                                                   destinationOffset, size);
        }

        void RecordFirstIndexOffset(ID3D12GraphicsCommandList* commandList,
                                    RenderPipeline* pipeline,
                                    uint32_t firstVertex,
                                    uint32_t firstInstance) {
            const FirstOffsetInfo& firstOffsetInfo = pipeline->GetFirstOffsetInfo();
            if (!firstOffsetInfo.usesVertexIndex && !firstOffsetInfo.usesInstanceIndex) {
                return;
            }
            std::array<uint32_t, 2> offsets{};
            uint32_t count = 0;
            if (firstOffsetInfo.usesVertexIndex) {
                offsets[firstOffsetInfo.vertexIndexOffset / sizeof(uint32_t)] = firstVertex;
                ++count;
            }
            if (firstOffsetInfo.usesInstanceIndex) {
                offsets[firstOffsetInfo.instanceIndexOffset / sizeof(uint32_t)] = firstInstance;
                ++count;
            }
            PipelineLayout* layout = ToBackend(pipeline->GetLayout());
            commandList->SetGraphicsRoot32BitConstants(layout->GetFirstIndexOffsetParameterIndex(),
                                                       count, offsets.data(), 0);
        }

        bool ShouldCopyUsingTemporaryBuffer(DeviceBase* device,
                                            const TextureCopy& srcCopy,
                                            const TextureCopy& dstCopy) {
            // Currently we only need the workaround for an Intel D3D12 driver issue.
            if (device->IsToggleEnabled(
                    Toggle::
                        UseTempBufferInSmallFormatTextureToTextureCopyFromGreaterToLessMipLevel)) {
                bool copyToLesserLevel = srcCopy.mipLevel > dstCopy.mipLevel;
                ASSERT(srcCopy.texture->GetFormat().format == dstCopy.texture->GetFormat().format);

                // GetAspectInfo(aspect) requires HasOneBit(aspect) == true, plus the texel block
                // sizes of depth stencil formats are always no less than 4 bytes.
                bool isSmallColorFormat =
                    HasOneBit(srcCopy.aspect) &&
                    srcCopy.texture->GetFormat().GetAspectInfo(srcCopy.aspect).block.byteSize < 4u;
                if (copyToLesserLevel && isSmallColorFormat) {
                    return true;
                }
            }

            return false;
        }

        MaybeError RecordCopyTextureWithTemporaryBuffer(CommandRecordingContext* recordingContext,
                                                        const TextureCopy& srcCopy,
                                                        const TextureCopy& dstCopy,
                                                        const Extent3D& copySize) {
            ASSERT(srcCopy.texture->GetFormat().format == dstCopy.texture->GetFormat().format);
            ASSERT(srcCopy.aspect == dstCopy.aspect);
            dawn_native::Format format = srcCopy.texture->GetFormat();
            const TexelBlockInfo& blockInfo = format.GetAspectInfo(srcCopy.aspect).block;
            ASSERT(copySize.width % blockInfo.width == 0);
            uint32_t widthInBlocks = copySize.width / blockInfo.width;
            ASSERT(copySize.height % blockInfo.height == 0);
            uint32_t heightInBlocks = copySize.height / blockInfo.height;

            // Create tempBuffer
            uint32_t bytesPerRow =
                Align(blockInfo.byteSize * widthInBlocks, kTextureBytesPerRowAlignment);
            uint32_t rowsPerImage = heightInBlocks;

            // The size of temporary buffer isn't needed to be a multiple of 4 because we don't
            // need to set mappedAtCreation to be true.
            auto tempBufferSize =
                ComputeRequiredBytesInCopy(blockInfo, copySize, bytesPerRow, rowsPerImage);

            BufferDescriptor tempBufferDescriptor;
            tempBufferDescriptor.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
            tempBufferDescriptor.size = tempBufferSize.AcquireSuccess();
            Device* device = ToBackend(srcCopy.texture->GetDevice());
            Ref<BufferBase> tempBufferBase;
            DAWN_TRY_ASSIGN(tempBufferBase, device->CreateBuffer(&tempBufferDescriptor));
            Ref<Buffer> tempBuffer = ToBackend(std::move(tempBufferBase));

            // Copy from source texture into tempBuffer
            Texture* srcTexture = ToBackend(srcCopy.texture).Get();
            tempBuffer->TrackUsageAndTransitionNow(recordingContext, wgpu::BufferUsage::CopyDst);
            BufferCopy bufferCopy;
            bufferCopy.buffer = tempBuffer;
            bufferCopy.offset = 0;
            bufferCopy.bytesPerRow = bytesPerRow;
            bufferCopy.rowsPerImage = rowsPerImage;
            RecordCopyTextureToBuffer(recordingContext->GetCommandList(), srcCopy, bufferCopy,
                                      srcTexture, tempBuffer.Get(), copySize);

            // Copy from tempBuffer into destination texture
            tempBuffer->TrackUsageAndTransitionNow(recordingContext, wgpu::BufferUsage::CopySrc);
            Texture* dstTexture = ToBackend(dstCopy.texture).Get();
            RecordCopyBufferToTexture(recordingContext, dstCopy, tempBuffer->GetD3D12Resource(), 0,
                                      bytesPerRow, rowsPerImage, copySize, dstTexture,
                                      dstCopy.aspect);

            // Save tempBuffer into recordingContext
            recordingContext->AddToTempBuffers(std::move(tempBuffer));

            return {};
        }

        // Records the necessary barriers for a synchronization scope using the resource usage
        // data pre-computed in the frontend. Also performs lazy initialization if required.
        // Returns whether any UAV are used in the synchronization scope.
        bool TransitionAndClearForSyncScope(CommandRecordingContext* commandContext,
                                            const SyncScopeResourceUsage& usages) {
            std::vector<D3D12_RESOURCE_BARRIER> barriers;

            ID3D12GraphicsCommandList* commandList = commandContext->GetCommandList();

            wgpu::BufferUsage bufferUsages = wgpu::BufferUsage::None;

            for (size_t i = 0; i < usages.buffers.size(); ++i) {
                Buffer* buffer = ToBackend(usages.buffers[i]);

                // TODO(crbug.com/dawn/852): clear storage buffers with
                // ClearUnorderedAccessView*().
                buffer->GetDevice()->ConsumedError(buffer->EnsureDataInitialized(commandContext));

                D3D12_RESOURCE_BARRIER barrier;
                if (buffer->TrackUsageAndGetResourceBarrier(commandContext, &barrier,
                                                            usages.bufferUsages[i])) {
                    barriers.push_back(barrier);
                }
                bufferUsages |= usages.bufferUsages[i];
            }

            wgpu::TextureUsage textureUsages = wgpu::TextureUsage::None;

            for (size_t i = 0; i < usages.textures.size(); ++i) {
                Texture* texture = ToBackend(usages.textures[i]);

                // Clear subresources that are not render attachments. Render attachments will be
                // cleared in RecordBeginRenderPass by setting the loadop to clear when the texture
                // subresource has not been initialized before the render pass.
                usages.textureUsages[i].Iterate(
                    [&](const SubresourceRange& range, wgpu::TextureUsage usage) {
                        if (usage & ~wgpu::TextureUsage::RenderAttachment) {
                            texture->EnsureSubresourceContentInitialized(commandContext, range);
                        }
                        textureUsages |= usage;
                    });

                ToBackend(usages.textures[i])
                    ->TrackUsageAndGetResourceBarrierForPass(commandContext, &barriers,
                                                             usages.textureUsages[i]);
            }

            if (barriers.size()) {
                commandList->ResourceBarrier(barriers.size(), barriers.data());
            }

            return (bufferUsages & wgpu::BufferUsage::Storage ||
                    textureUsages & wgpu::TextureUsage::StorageBinding);
        }

    }  // anonymous namespace

    class BindGroupStateTracker : public BindGroupTrackerBase<false, uint64_t> {
        using Base = BindGroupTrackerBase;

      public:
        BindGroupStateTracker(Device* device)
            : BindGroupTrackerBase(),
              mDevice(device),
              mViewAllocator(device->GetViewShaderVisibleDescriptorAllocator()),
              mSamplerAllocator(device->GetSamplerShaderVisibleDescriptorAllocator()) {
        }

        void SetInComputePass(bool inCompute_) {
            mInCompute = inCompute_;
        }

        MaybeError Apply(CommandRecordingContext* commandContext) {
            BeforeApply();

            ID3D12GraphicsCommandList* commandList = commandContext->GetCommandList();
            UpdateRootSignatureIfNecessary(commandList);

            // Bindgroups are allocated in shader-visible descriptor heaps which are managed by a
            // ringbuffer. There can be a single shader-visible descriptor heap of each type bound
            // at any given time. This means that when we switch heaps, all other currently bound
            // bindgroups must be re-populated. Bindgroups can fail allocation gracefully which is
            // the signal to change the bounded heaps.
            // Re-populating all bindgroups after the last one fails causes duplicated allocations
            // to occur on overflow.
            bool didCreateBindGroupViews = true;
            bool didCreateBindGroupSamplers = true;
            for (BindGroupIndex index : IterateBitSet(mDirtyBindGroups)) {
                BindGroup* group = ToBackend(mBindGroups[index]);
                didCreateBindGroupViews = group->PopulateViews(mViewAllocator);
                didCreateBindGroupSamplers = group->PopulateSamplers(mDevice, mSamplerAllocator);
                if (!didCreateBindGroupViews && !didCreateBindGroupSamplers) {
                    break;
                }
            }

            if (!didCreateBindGroupViews || !didCreateBindGroupSamplers) {
                if (!didCreateBindGroupViews) {
                    DAWN_TRY(mViewAllocator->AllocateAndSwitchShaderVisibleHeap());
                }

                if (!didCreateBindGroupSamplers) {
                    DAWN_TRY(mSamplerAllocator->AllocateAndSwitchShaderVisibleHeap());
                }

                mDirtyBindGroupsObjectChangedOrIsDynamic |= mBindGroupLayoutsMask;
                mDirtyBindGroups |= mBindGroupLayoutsMask;

                // Must be called before applying the bindgroups.
                SetID3D12DescriptorHeaps(commandList);

                for (BindGroupIndex index : IterateBitSet(mBindGroupLayoutsMask)) {
                    BindGroup* group = ToBackend(mBindGroups[index]);
                    didCreateBindGroupViews = group->PopulateViews(mViewAllocator);
                    didCreateBindGroupSamplers =
                        group->PopulateSamplers(mDevice, mSamplerAllocator);
                    ASSERT(didCreateBindGroupViews);
                    ASSERT(didCreateBindGroupSamplers);
                }
            }

            for (BindGroupIndex index : IterateBitSet(mDirtyBindGroupsObjectChangedOrIsDynamic)) {
                BindGroup* group = ToBackend(mBindGroups[index]);
                ApplyBindGroup(commandList, ToBackend(mPipelineLayout), index, group,
                               mDynamicOffsetCounts[index], mDynamicOffsets[index].data());
            }

            AfterApply();

            return {};
        }

        void SetID3D12DescriptorHeaps(ID3D12GraphicsCommandList* commandList) {
            ASSERT(commandList != nullptr);
            std::array<ID3D12DescriptorHeap*, 2> descriptorHeaps = {
                mViewAllocator->GetShaderVisibleHeap(), mSamplerAllocator->GetShaderVisibleHeap()};
            ASSERT(descriptorHeaps[0] != nullptr);
            ASSERT(descriptorHeaps[1] != nullptr);
            commandList->SetDescriptorHeaps(descriptorHeaps.size(), descriptorHeaps.data());
        }

      private:
        void UpdateRootSignatureIfNecessary(ID3D12GraphicsCommandList* commandList) {
            if (mLastAppliedPipelineLayout != mPipelineLayout) {
                if (mInCompute) {
                    commandList->SetComputeRootSignature(
                        ToBackend(mPipelineLayout)->GetRootSignature());
                } else {
                    commandList->SetGraphicsRootSignature(
                        ToBackend(mPipelineLayout)->GetRootSignature());
                }
                // Invalidate the root sampler tables previously set in the root signature.
                mBoundRootSamplerTables = {};
            }
        }

        void ApplyBindGroup(ID3D12GraphicsCommandList* commandList,
                            const PipelineLayout* pipelineLayout,
                            BindGroupIndex index,
                            BindGroup* group,
                            uint32_t dynamicOffsetCountIn,
                            const uint64_t* dynamicOffsetsIn) {
            ityp::span<BindingIndex, const uint64_t> dynamicOffsets(
                dynamicOffsetsIn, BindingIndex(dynamicOffsetCountIn));
            ASSERT(dynamicOffsets.size() == group->GetLayout()->GetDynamicBufferCount());

            // Usually, the application won't set the same offsets many times,
            // so always try to apply dynamic offsets even if the offsets stay the same
            if (dynamicOffsets.size() != BindingIndex(0)) {
                // Update dynamic offsets.
                // Dynamic buffer bindings are packed at the beginning of the layout.
                for (BindingIndex bindingIndex{0}; bindingIndex < dynamicOffsets.size();
                     ++bindingIndex) {
                    const BindingInfo& bindingInfo =
                        group->GetLayout()->GetBindingInfo(bindingIndex);
                    if (bindingInfo.visibility == wgpu::ShaderStage::None) {
                        // Skip dynamic buffers that are not visible. D3D12 does not have None
                        // visibility.
                        continue;
                    }

                    uint32_t parameterIndex =
                        pipelineLayout->GetDynamicRootParameterIndex(index, bindingIndex);
                    BufferBinding binding = group->GetBindingAsBufferBinding(bindingIndex);

                    // Calculate buffer locations that root descriptors links to. The location
                    // is (base buffer location + initial offset + dynamic offset)
                    uint64_t dynamicOffset = dynamicOffsets[bindingIndex];
                    uint64_t offset = binding.offset + dynamicOffset;
                    D3D12_GPU_VIRTUAL_ADDRESS bufferLocation =
                        ToBackend(binding.buffer)->GetVA() + offset;

                    ASSERT(bindingInfo.bindingType == BindingInfoType::Buffer);
                    switch (bindingInfo.buffer.type) {
                        case wgpu::BufferBindingType::Uniform:
                            if (mInCompute) {
                                commandList->SetComputeRootConstantBufferView(parameterIndex,
                                                                              bufferLocation);
                            } else {
                                commandList->SetGraphicsRootConstantBufferView(parameterIndex,
                                                                               bufferLocation);
                            }
                            break;
                        case wgpu::BufferBindingType::Storage:
                        case kInternalStorageBufferBinding:
                            if (mInCompute) {
                                commandList->SetComputeRootUnorderedAccessView(parameterIndex,
                                                                               bufferLocation);
                            } else {
                                commandList->SetGraphicsRootUnorderedAccessView(parameterIndex,
                                                                                bufferLocation);
                            }
                            break;
                        case wgpu::BufferBindingType::ReadOnlyStorage:
                            if (mInCompute) {
                                commandList->SetComputeRootShaderResourceView(parameterIndex,
                                                                              bufferLocation);
                            } else {
                                commandList->SetGraphicsRootShaderResourceView(parameterIndex,
                                                                               bufferLocation);
                            }
                            break;
                        case wgpu::BufferBindingType::Undefined:
                            UNREACHABLE();
                    }
                }
            }

            // It's not necessary to update descriptor tables if only the dynamic offset changed.
            if (!mDirtyBindGroups[index]) {
                return;
            }

            const uint32_t cbvUavSrvCount =
                ToBackend(group->GetLayout())->GetCbvUavSrvDescriptorCount();
            const uint32_t samplerCount =
                ToBackend(group->GetLayout())->GetSamplerDescriptorCount();

            if (cbvUavSrvCount > 0) {
                uint32_t parameterIndex = pipelineLayout->GetCbvUavSrvRootParameterIndex(index);
                const D3D12_GPU_DESCRIPTOR_HANDLE baseDescriptor = group->GetBaseViewDescriptor();
                if (mInCompute) {
                    commandList->SetComputeRootDescriptorTable(parameterIndex, baseDescriptor);
                } else {
                    commandList->SetGraphicsRootDescriptorTable(parameterIndex, baseDescriptor);
                }
            }

            if (samplerCount > 0) {
                uint32_t parameterIndex = pipelineLayout->GetSamplerRootParameterIndex(index);
                const D3D12_GPU_DESCRIPTOR_HANDLE baseDescriptor =
                    group->GetBaseSamplerDescriptor();
                // Check if the group requires its sampler table to be set in the pipeline.
                // This because sampler heap allocations could be cached and use the same table.
                if (mBoundRootSamplerTables[index].ptr != baseDescriptor.ptr) {
                    if (mInCompute) {
                        commandList->SetComputeRootDescriptorTable(parameterIndex, baseDescriptor);
                    } else {
                        commandList->SetGraphicsRootDescriptorTable(parameterIndex, baseDescriptor);
                    }

                    mBoundRootSamplerTables[index] = baseDescriptor;
                }
            }
        }

        Device* mDevice;

        bool mInCompute = false;

        ityp::array<BindGroupIndex, D3D12_GPU_DESCRIPTOR_HANDLE, kMaxBindGroups>
            mBoundRootSamplerTables = {};

        ShaderVisibleDescriptorAllocator* mViewAllocator;
        ShaderVisibleDescriptorAllocator* mSamplerAllocator;
    };

    namespace {
        class VertexBufferTracker {
          public:
            void OnSetVertexBuffer(VertexBufferSlot slot,
                                   Buffer* buffer,
                                   uint64_t offset,
                                   uint64_t size) {
                mStartSlot = std::min(mStartSlot, slot);
                mEndSlot = std::max(mEndSlot, ityp::Add(slot, VertexBufferSlot(uint8_t(1))));

                auto* d3d12BufferView = &mD3D12BufferViews[slot];
                d3d12BufferView->BufferLocation = buffer->GetVA() + offset;
                d3d12BufferView->SizeInBytes = size;
                // The bufferView stride is set based on the vertex state before a draw.
            }

            void Apply(ID3D12GraphicsCommandList* commandList,
                       const RenderPipeline* renderPipeline) {
                ASSERT(renderPipeline != nullptr);

                VertexBufferSlot startSlot = mStartSlot;
                VertexBufferSlot endSlot = mEndSlot;

                // If the vertex state has changed, we need to update the StrideInBytes
                // for the D3D12 buffer views. We also need to extend the dirty range to
                // touch all these slots because the stride may have changed.
                if (mLastAppliedRenderPipeline != renderPipeline) {
                    mLastAppliedRenderPipeline = renderPipeline;

                    for (VertexBufferSlot slot :
                         IterateBitSet(renderPipeline->GetVertexBufferSlotsUsed())) {
                        startSlot = std::min(startSlot, slot);
                        endSlot =
                            std::max(endSlot, ityp::Add(slot, VertexBufferSlot(uint8_t(1))));
                        mD3D12BufferViews[slot].StrideInBytes =
                            renderPipeline->GetVertexBuffer(slot).arrayStride;
                    }
                }

                if (endSlot <= startSlot) {
                    return;
                }

                // mD3D12BufferViews is kept up to date with the most recent data passed
                // to SetVertexBuffer. This makes it correct to only track the start
                // and end of the dirty range. When Apply is called,
                // we will at worst set non-dirty vertex buffers in duplicate.
                commandList->IASetVertexBuffers(static_cast<uint8_t>(startSlot),
                                                static_cast<uint8_t>(ityp::Sub(endSlot, startSlot)),
                                                &mD3D12BufferViews[startSlot]);

                mStartSlot = VertexBufferSlot(kMaxVertexBuffers);
                mEndSlot = VertexBufferSlot(uint8_t(0));
            }

          private:
            // startSlot and endSlot indicate the range of dirty vertex buffers.
            // If there are multiple calls to SetVertexBuffer, the start and end
            // represent the union of the dirty ranges (the union may have non-dirty
            // data in the middle of the range).
            const RenderPipeline* mLastAppliedRenderPipeline = nullptr;
            VertexBufferSlot mStartSlot{kMaxVertexBuffers};
            VertexBufferSlot mEndSlot{uint8_t(0)};
            ityp::array<VertexBufferSlot, D3D12_VERTEX_BUFFER_VIEW, kMaxVertexBuffers>
                mD3D12BufferViews = {};
        };

        void ResolveMultisampledRenderPass(CommandRecordingContext* commandContext,
                                           BeginRenderPassCmd* renderPass) {
            ASSERT(renderPass != nullptr);

            for (ColorAttachmentIndex i :
                 IterateBitSet(renderPass->attachmentState->GetColorAttachmentsMask())) {
                TextureViewBase* resolveTarget =
                    renderPass->colorAttachments[i].resolveTarget.Get();
                if (resolveTarget == nullptr) {
                    continue;
                }

                TextureViewBase* colorView = renderPass->colorAttachments[i].view.Get();
                Texture* colorTexture = ToBackend(colorView->GetTexture());
                Texture* resolveTexture = ToBackend(resolveTarget->GetTexture());

                // Transition the usages of the color attachment and resolve target.
                colorTexture->TrackUsageAndTransitionNow(commandContext,
                                                         D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
                                                         colorView->GetSubresourceRange());
                resolveTexture->TrackUsageAndTransitionNow(commandContext,
                                                           D3D12_RESOURCE_STATE_RESOLVE_DEST,
                                                           resolveTarget->GetSubresourceRange());

                // Do MSAA resolve with ResolveSubResource().
                ID3D12Resource* colorTextureHandle = colorTexture->GetD3D12Resource();
                ID3D12Resource* resolveTextureHandle = resolveTexture->GetD3D12Resource();
                const uint32_t resolveTextureSubresourceIndex = resolveTexture->GetSubresourceIndex(
                    resolveTarget->GetBaseMipLevel(), resolveTarget->GetBaseArrayLayer(),
                    Aspect::Color);
                constexpr uint32_t kColorTextureSubresourceIndex = 0;
                commandContext->GetCommandList()->ResolveSubresource(
                    resolveTextureHandle, resolveTextureSubresourceIndex, colorTextureHandle,
                    kColorTextureSubresourceIndex, colorTexture->GetD3D12Format());
            }
        }

    }  // anonymous namespace

    // static
    Ref<CommandBuffer> CommandBuffer::Create(CommandEncoder* encoder,
                                             const CommandBufferDescriptor* descriptor) {
        return AcquireRef(new CommandBuffer(encoder, descriptor));
    }

    CommandBuffer::CommandBuffer(CommandEncoder* encoder, const CommandBufferDescriptor* descriptor)
        : CommandBufferBase(encoder, descriptor) {
    }

    MaybeError CommandBuffer::RecordCommands(CommandRecordingContext* commandContext) {
        Device* device = ToBackend(GetDevice());
        BindGroupStateTracker bindingTracker(device);

        ID3D12GraphicsCommandList* commandList = commandContext->GetCommandList();

        // Make sure we use the correct descriptors for this command list. Could be done once per
        // actual command list but here is ok because there should be few command buffers.
        bindingTracker.SetID3D12DescriptorHeaps(commandList);

        size_t nextComputePassNumber = 0;
        size_t nextRenderPassNumber = 0;

        Command type;
        while (mCommands.NextCommandId(&type)) {
            switch (type) {
                case Command::BeginComputePass: {
                    mCommands.NextCommand<BeginComputePassCmd>();

                    bindingTracker.SetInComputePass(true);
                    DAWN_TRY(RecordComputePass(
                        commandContext, &bindingTracker,
                        GetResourceUsages().computePasses[nextComputePassNumber]));

                    nextComputePassNumber++;
                    break;
                }

                case Command::BeginRenderPass: {
                    BeginRenderPassCmd* beginRenderPassCmd =
                        mCommands.NextCommand<BeginRenderPassCmd>();

                    const bool passHasUAV = TransitionAndClearForSyncScope(
                        commandContext, GetResourceUsages().renderPasses[nextRenderPassNumber]);
                    bindingTracker.SetInComputePass(false);

                    LazyClearRenderPassAttachments(beginRenderPassCmd);
                    DAWN_TRY(RecordRenderPass(commandContext, &bindingTracker, beginRenderPassCmd,
                                              passHasUAV));

                    nextRenderPassNumber++;
                    break;
                }

                case Command::CopyBufferToBuffer: {
                    CopyBufferToBufferCmd* copy = mCommands.NextCommand<CopyBufferToBufferCmd>();
                    if (copy->size == 0) {
                        // Skip no-op copies.
                        break;
                    }
                    Buffer* srcBuffer = ToBackend(copy->source.Get());
                    Buffer* dstBuffer = ToBackend(copy->destination.Get());

                    DAWN_TRY(srcBuffer->EnsureDataInitialized(commandContext));
                    DAWN_TRY(dstBuffer->EnsureDataInitializedAsDestination(
                        commandContext, copy->destinationOffset, copy->size));

                    srcBuffer->TrackUsageAndTransitionNow(commandContext,
                                                          wgpu::BufferUsage::CopySrc);
                    dstBuffer->TrackUsageAndTransitionNow(commandContext,
                                                          wgpu::BufferUsage::CopyDst);

                    commandList->CopyBufferRegion(
                        dstBuffer->GetD3D12Resource(), copy->destinationOffset,
                        srcBuffer->GetD3D12Resource(), copy->sourceOffset, copy->size);
                    break;
                }

                case Command::CopyBufferToTexture: {
                    CopyBufferToTextureCmd* copy = mCommands.NextCommand<CopyBufferToTextureCmd>();
                    if (copy->copySize.width == 0 || copy->copySize.height == 0 ||
                        copy->copySize.depthOrArrayLayers == 0) {
                        // Skip no-op copies.
                        continue;
                    }
                    Buffer* buffer = ToBackend(copy->source.buffer.Get());
                    Texture* texture = ToBackend(copy->destination.texture.Get());

                    DAWN_TRY(buffer->EnsureDataInitialized(commandContext));

                    ASSERT(texture->GetDimension() != wgpu::TextureDimension::e1D);
                    SubresourceRange subresources =
                        GetSubresourcesAffectedByCopy(copy->destination, copy->copySize);

                    if (IsCompleteSubresourceCopiedTo(texture, copy->copySize,
                                                      copy->destination.mipLevel)) {
                        texture->SetIsSubresourceContentInitialized(true, subresources);
                    } else {
                        texture->EnsureSubresourceContentInitialized(commandContext, subresources);
                    }

                    buffer->TrackUsageAndTransitionNow(commandContext, wgpu::BufferUsage::CopySrc);
                    texture->TrackUsageAndTransitionNow(commandContext, wgpu::TextureUsage::CopyDst,
                                                        subresources);

                    RecordCopyBufferToTexture(commandContext, copy->destination,
                                              buffer->GetD3D12Resource(), copy->source.offset,
                                              copy->source.bytesPerRow, copy->source.rowsPerImage,
                                              copy->copySize, texture, subresources.aspects);

                    break;
                }

                case Command::CopyTextureToBuffer: {
                    CopyTextureToBufferCmd* copy = mCommands.NextCommand<CopyTextureToBufferCmd>();
                    if (copy->copySize.width == 0 || copy->copySize.height == 0 ||
                        copy->copySize.depthOrArrayLayers == 0) {
                        // Skip no-op copies.
                        continue;
                    }
                    Texture* texture = ToBackend(copy->source.texture.Get());
                    Buffer* buffer = ToBackend(copy->destination.buffer.Get());

                    DAWN_TRY(buffer->EnsureDataInitializedAsDestination(commandContext, copy));

                    ASSERT(texture->GetDimension() != wgpu::TextureDimension::e1D);
                    SubresourceRange subresources =
                        GetSubresourcesAffectedByCopy(copy->source, copy->copySize);

                    texture->EnsureSubresourceContentInitialized(commandContext, subresources);

                    texture->TrackUsageAndTransitionNow(commandContext, wgpu::TextureUsage::CopySrc,
                                                        subresources);
                    buffer->TrackUsageAndTransitionNow(commandContext, wgpu::BufferUsage::CopyDst);

                    RecordCopyTextureToBuffer(commandList, copy->source, copy->destination, texture,
                                              buffer, copy->copySize);

                    break;
                }

                case Command::CopyTextureToTexture: {
                    CopyTextureToTextureCmd* copy =
                        mCommands.NextCommand<CopyTextureToTextureCmd>();
                    if (copy->copySize.width == 0 || copy->copySize.height == 0 ||
                        copy->copySize.depthOrArrayLayers == 0) {
                        // Skip no-op copies.
                        continue;
                    }
                    Texture* source = ToBackend(copy->source.texture.Get());
                    Texture* destination = ToBackend(copy->destination.texture.Get());

                    SubresourceRange srcRange =
                        GetSubresourcesAffectedByCopy(copy->source, copy->copySize);
                    SubresourceRange dstRange =
                        GetSubresourcesAffectedByCopy(copy->destination, copy->copySize);

                    source->EnsureSubresourceContentInitialized(commandContext, srcRange);
                    if (IsCompleteSubresourceCopiedTo(destination, copy->copySize,
                                                      copy->destination.mipLevel)) {
                        destination->SetIsSubresourceContentInitialized(true, dstRange);
                    } else {
                        destination->EnsureSubresourceContentInitialized(commandContext, dstRange);
                    }

                    if (copy->source.texture.Get() == copy->destination.texture.Get() &&
                        copy->source.mipLevel == copy->destination.mipLevel) {
                        // When there are overlapped subresources, the layout of the overlapped
                        // subresources should all be COMMON instead of what we set now. Currently
                        // it is not allowed to copy with overlapped subresources, but we still
                        // add the ASSERT here as a reminder for this possible misuse.
                        ASSERT(!IsRangeOverlapped(copy->source.origin.z, copy->destination.origin.z,
                                                  copy->copySize.depthOrArrayLayers));
                    }
                    source->TrackUsageAndTransitionNow(commandContext, wgpu::TextureUsage::CopySrc,
                                                       srcRange);
                    destination->TrackUsageAndTransitionNow(commandContext,
                                                            wgpu::TextureUsage::CopyDst, dstRange);

                    ASSERT(srcRange.aspects == dstRange.aspects);
                    if (ShouldCopyUsingTemporaryBuffer(GetDevice(), copy->source,
                                                       copy->destination)) {
                        DAWN_TRY(RecordCopyTextureWithTemporaryBuffer(
                            commandContext, copy->source, copy->destination, copy->copySize));
                        break;
                    }

                    if (CanUseCopyResource(copy->source, copy->destination, copy->copySize)) {
                        commandList->CopyResource(destination->GetD3D12Resource(),
                                                  source->GetD3D12Resource());
                    } else if (source->GetDimension() == wgpu::TextureDimension::e3D &&
                               destination->GetDimension() == wgpu::TextureDimension::e3D) {
                        for (Aspect aspect : IterateEnumMask(srcRange.aspects)) {
                            D3D12_TEXTURE_COPY_LOCATION srcLocation =
                                ComputeTextureCopyLocationForTexture(source, copy->source.mipLevel,
                                                                     0, aspect);
                            D3D12_TEXTURE_COPY_LOCATION dstLocation =
                                ComputeTextureCopyLocationForTexture(
                                    destination, copy->destination.mipLevel, 0, aspect);

                            D3D12_BOX sourceRegion = ComputeD3D12BoxFromOffsetAndSize(
                                copy->source.origin, copy->copySize);

                            commandList->CopyTextureRegion(&dstLocation, copy->destination.origin.x,
                                                           copy->destination.origin.y,
                                                           copy->destination.origin.z, &srcLocation,
                                                           &sourceRegion);
                        }
                    } else {
                        // TODO(crbug.com/dawn/814): support copying with 1D.
                        ASSERT(source->GetDimension() != wgpu::TextureDimension::e1D &&
                               destination->GetDimension() != wgpu::TextureDimension::e1D);
                        const dawn_native::Extent3D copyExtentOneSlice = {
                            copy->copySize.width, copy->copySize.height, 1u};

                        for (Aspect aspect : IterateEnumMask(srcRange.aspects)) {
                            for (uint32_t z = 0; z < copy->copySize.depthOrArrayLayers; ++z) {
                                uint32_t sourceLayer = 0;
                                uint32_t sourceZ = 0;
                                switch (source->GetDimension()) {
                                    case wgpu::TextureDimension::e2D:
                                        sourceLayer = copy->source.origin.z + z;
                                        break;
                                    case wgpu::TextureDimension::e3D:
                                        sourceZ = copy->source.origin.z + z;
                                        break;
                                    case wgpu::TextureDimension::e1D:
                                        UNREACHABLE();
                                }

                                uint32_t destinationLayer = 0;
                                uint32_t destinationZ = 0;
                                switch (destination->GetDimension()) {
                                    case wgpu::TextureDimension::e2D:
                                        destinationLayer = copy->destination.origin.z + z;
                                        break;
                                    case wgpu::TextureDimension::e3D:
                                        destinationZ = copy->destination.origin.z + z;
                                        break;
                                    case wgpu::TextureDimension::e1D:
                                        UNREACHABLE();
                                }
                                D3D12_TEXTURE_COPY_LOCATION srcLocation =
                                    ComputeTextureCopyLocationForTexture(
                                        source, copy->source.mipLevel, sourceLayer, aspect);

                                D3D12_TEXTURE_COPY_LOCATION dstLocation =
                                    ComputeTextureCopyLocationForTexture(destination,
                                                                         copy->destination.mipLevel,
                                                                         destinationLayer, aspect);

                                Origin3D sourceOriginInSubresource = copy->source.origin;
                                sourceOriginInSubresource.z = sourceZ;
                                D3D12_BOX sourceRegion = ComputeD3D12BoxFromOffsetAndSize(
                                    sourceOriginInSubresource, copyExtentOneSlice);

                                commandList->CopyTextureRegion(
                                    &dstLocation, copy->destination.origin.x,
                                    copy->destination.origin.y, destinationZ, &srcLocation,
                                    &sourceRegion);
                            }
                        }
                    }
                    break;
                }

                case Command::ResolveQuerySet: {
                    ResolveQuerySetCmd* cmd = mCommands.NextCommand<ResolveQuerySetCmd>();
                    QuerySet* querySet = ToBackend(cmd->querySet.Get());
                    uint32_t firstQuery = cmd->firstQuery;
                    uint32_t queryCount = cmd->queryCount;
                    Buffer* destination = ToBackend(cmd->destination.Get());
                    uint64_t destinationOffset = cmd->destinationOffset;

                    DAWN_TRY(destination->EnsureDataInitializedAsDestination(
                        commandContext, destinationOffset, queryCount * sizeof(uint64_t)));

                    // Resolving unavailable queries is undefined behaviour on D3D12, we only can
                    // resolve the available part of sparse queries. In order to resolve the
                    // unavailables as 0s, we need to clear the resolving region of the destination
                    // buffer to 0s.
                    auto startIt = querySet->GetQueryAvailability().begin() + firstQuery;
                    auto endIt = querySet->GetQueryAvailability().begin() + firstQuery + queryCount;
                    bool hasUnavailableQueries = std::find(startIt, endIt, false) != endIt;
                    if (hasUnavailableQueries) {
                        DAWN_TRY(ClearBufferToZero(device, destination, destinationOffset,
                                                   queryCount * sizeof(uint64_t)));
                    }

                    destination->TrackUsageAndTransitionNow(commandContext,
                                                            wgpu::BufferUsage::QueryResolve);

                    RecordResolveQuerySetCmd(commandList, device, querySet, firstQuery, queryCount,
                                             destination, destinationOffset);

                    break;
                }

                case Command::WriteTimestamp: {
                    WriteTimestampCmd* cmd = mCommands.NextCommand<WriteTimestampCmd>();

                    RecordWriteTimestampCmd(commandList, cmd);
                    break;
                }

                case Command::InsertDebugMarker: {
                    InsertDebugMarkerCmd* cmd = mCommands.NextCommand<InsertDebugMarkerCmd>();
                    const char* label = mCommands.NextData<char>(cmd->length + 1);

                    if (ToBackend(GetDevice())->GetFunctions()->IsPIXEventRuntimeLoaded()) {
                        // PIX color is 1 byte per channel in ARGB format
                        constexpr uint64_t kPIXBlackColor = 0xff000000;
                        ToBackend(GetDevice())
                            ->GetFunctions()
                            ->pixSetMarkerOnCommandList(commandList, kPIXBlackColor, label);
                    }
                    break;
                }

                case Command::PopDebugGroup: {
                    mCommands.NextCommand<PopDebugGroupCmd>();

                    if (ToBackend(GetDevice())->GetFunctions()->IsPIXEventRuntimeLoaded()) {
                        ToBackend(GetDevice())
                            ->GetFunctions()
                            ->pixEndEventOnCommandList(commandList);
                    }
                    break;
                }

                case Command::PushDebugGroup: {
                    PushDebugGroupCmd* cmd = mCommands.NextCommand<PushDebugGroupCmd>();
                    const char* label = mCommands.NextData<char>(cmd->length + 1);

                    if (ToBackend(GetDevice())->GetFunctions()->IsPIXEventRuntimeLoaded()) {
                        // PIX color is 1 byte per channel in ARGB format
                        constexpr uint64_t kPIXBlackColor = 0xff000000;
                        ToBackend(GetDevice())
                            ->GetFunctions()
                            ->pixBeginEventOnCommandList(commandList, kPIXBlackColor, label);
                    }
                    break;
                }

                default:
                    UNREACHABLE();
            }
        }

        return {};
    }

    MaybeError CommandBuffer::RecordComputePass(CommandRecordingContext* commandContext,
                                                BindGroupStateTracker* bindingTracker,
                                                const ComputePassResourceUsage& resourceUsages) {
        uint64_t currentDispatch = 0;
        ID3D12GraphicsCommandList* commandList = commandContext->GetCommandList();

        Command type;
        while (mCommands.NextCommandId(&type)) {
            switch (type) {
                case Command::Dispatch: {
                    DispatchCmd* dispatch = mCommands.NextCommand<DispatchCmd>();

                    // Skip noop dispatches, it can cause D3D12 warning from validation layers and
                    // leads to device lost.
                    if (dispatch->x == 0 || dispatch->y == 0 || dispatch->z == 0) {
                        break;
                    }

                    TransitionAndClearForSyncScope(commandContext,
                                                   resourceUsages.dispatchUsages[currentDispatch]);
                    DAWN_TRY(bindingTracker->Apply(commandContext));

                    commandList->Dispatch(dispatch->x, dispatch->y, dispatch->z);
                    currentDispatch++;
                    break;
                }

                case Command::DispatchIndirect: {
                    DispatchIndirectCmd* dispatch = mCommands.NextCommand<DispatchIndirectCmd>();
                    Buffer* buffer = ToBackend(dispatch->indirectBuffer.Get());

                    TransitionAndClearForSyncScope(commandContext,
                                                   resourceUsages.dispatchUsages[currentDispatch]);
                    DAWN_TRY(bindingTracker->Apply(commandContext));

                    ComPtr<ID3D12CommandSignature> signature =
                        ToBackend(GetDevice())->GetDispatchIndirectSignature();
                    commandList->ExecuteIndirect(signature.Get(), 1, buffer->GetD3D12Resource(),
                                                 dispatch->indirectOffset, nullptr, 0);
                    currentDispatch++;
                    break;
                }

                case Command::EndComputePass: {
                    mCommands.NextCommand<EndComputePassCmd>();
                    return {};
                }

                case Command::SetComputePipeline: {
                    SetComputePipelineCmd* cmd = mCommands.NextCommand<SetComputePipelineCmd>();
                    ComputePipeline* pipeline = ToBackend(cmd->pipeline).Get();

                    commandList->SetPipelineState(pipeline->GetPipelineState());

                    bindingTracker->OnSetPipeline(pipeline);
                    break;
                }

                case Command::SetBindGroup: {
                    SetBindGroupCmd* cmd = mCommands.NextCommand<SetBindGroupCmd>();
                    BindGroup* group = ToBackend(cmd->group.Get());
                    uint32_t* dynamicOffsets = nullptr;

                    if (cmd->dynamicOffsetCount > 0) {
                        dynamicOffsets = mCommands.NextData<uint32_t>(cmd->dynamicOffsetCount);
                    }

                    bindingTracker->OnSetBindGroup(cmd->index, group, cmd->dynamicOffsetCount,
                                                   dynamicOffsets);
                    break;
                }

                case Command::InsertDebugMarker: {
                    InsertDebugMarkerCmd* cmd = mCommands.NextCommand<InsertDebugMarkerCmd>();
                    const char* label = mCommands.NextData<char>(cmd->length + 1);

                    if (ToBackend(GetDevice())->GetFunctions()->IsPIXEventRuntimeLoaded()) {
                        // PIX color is 1 byte per channel in ARGB format
                        constexpr uint64_t kPIXBlackColor = 0xff000000;
                        ToBackend(GetDevice())
                            ->GetFunctions()
                            ->pixSetMarkerOnCommandList(commandList, kPIXBlackColor, label);
                    }
                    break;
                }

                case Command::PopDebugGroup: {
                    mCommands.NextCommand<PopDebugGroupCmd>();

                    if (ToBackend(GetDevice())->GetFunctions()->IsPIXEventRuntimeLoaded()) {
                        ToBackend(GetDevice())
                            ->GetFunctions()
                            ->pixEndEventOnCommandList(commandList);
                    }
                    break;
                }

                case Command::PushDebugGroup: {
                    PushDebugGroupCmd* cmd = mCommands.NextCommand<PushDebugGroupCmd>();
                    const char* label = mCommands.NextData<char>(cmd->length + 1);

                    if (ToBackend(GetDevice())->GetFunctions()->IsPIXEventRuntimeLoaded()) {
                        // PIX color is 1 byte per channel in ARGB format
                        constexpr uint64_t kPIXBlackColor = 0xff000000;
                        ToBackend(GetDevice())
                            ->GetFunctions()
                            ->pixBeginEventOnCommandList(commandList, kPIXBlackColor, label);
                    }
                    break;
                }

                case Command::WriteTimestamp: {
                    WriteTimestampCmd* cmd = mCommands.NextCommand<WriteTimestampCmd>();

                    RecordWriteTimestampCmd(commandList, cmd);
                    break;
                }

                default:
                    UNREACHABLE();
            }
        }

        return {};
    }

    MaybeError CommandBuffer::SetupRenderPass(CommandRecordingContext* commandContext,
                                              BeginRenderPassCmd* renderPass,
                                              RenderPassBuilder* renderPassBuilder) {
        Device* device = ToBackend(GetDevice());

        for (ColorAttachmentIndex i :
             IterateBitSet(renderPass->attachmentState->GetColorAttachmentsMask())) {
            RenderPassColorAttachmentInfo& attachmentInfo = renderPass->colorAttachments[i];
            TextureView* view = ToBackend(attachmentInfo.view.Get());

            // Set view attachment.
            CPUDescriptorHeapAllocation rtvAllocation;
            DAWN_TRY_ASSIGN(
                rtvAllocation,
                device->GetRenderTargetViewAllocator()->AllocateTransientCPUDescriptors());

            const D3D12_RENDER_TARGET_VIEW_DESC viewDesc = view->GetRTVDescriptor();
            const D3D12_CPU_DESCRIPTOR_HANDLE baseDescriptor = rtvAllocation.GetBaseDescriptor();

            device->GetD3D12Device()->CreateRenderTargetView(
                ToBackend(view->GetTexture())->GetD3D12Resource(), &viewDesc, baseDescriptor);

            renderPassBuilder->SetRenderTargetView(i, baseDescriptor);

            // Set color load operation.
            renderPassBuilder->SetRenderTargetBeginningAccess(
                i, attachmentInfo.loadOp, attachmentInfo.clearColor, view->GetD3D12Format());

            // Set color store operation.
            if (attachmentInfo.resolveTarget != nullptr) {
                TextureView* resolveDestinationView = ToBackend(attachmentInfo.resolveTarget.Get());
                Texture* resolveDestinationTexture =
                    ToBackend(resolveDestinationView->GetTexture());

                resolveDestinationTexture->TrackUsageAndTransitionNow(
                    commandContext, D3D12_RESOURCE_STATE_RESOLVE_DEST,
                    resolveDestinationView->GetSubresourceRange());

                renderPassBuilder->SetRenderTargetEndingAccessResolve(i, attachmentInfo.storeOp,
                                                                      view, resolveDestinationView);
            } else {
                renderPassBuilder->SetRenderTargetEndingAccess(i, attachmentInfo.storeOp);
            }
        }

        if (renderPass->attachmentState->HasDepthStencilAttachment()) {
            RenderPassDepthStencilAttachmentInfo& attachmentInfo =
                renderPass->depthStencilAttachment;
            TextureView* view = ToBackend(renderPass->depthStencilAttachment.view.Get());

            // Set depth attachment.
            CPUDescriptorHeapAllocation dsvAllocation;
            DAWN_TRY_ASSIGN(
                dsvAllocation,
                device->GetDepthStencilViewAllocator()->AllocateTransientCPUDescriptors());

            const D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc = view->GetDSVDescriptor();
            const D3D12_CPU_DESCRIPTOR_HANDLE baseDescriptor = dsvAllocation.GetBaseDescriptor();

            device->GetD3D12Device()->CreateDepthStencilView(
                ToBackend(view->GetTexture())->GetD3D12Resource(), &viewDesc, baseDescriptor);

            renderPassBuilder->SetDepthStencilView(baseDescriptor);

            const bool hasDepth = view->GetTexture()->GetFormat().HasDepth();
            const bool hasStencil = view->GetTexture()->GetFormat().HasStencil();

            // Set depth/stencil load operations.
            if (hasDepth) {
                renderPassBuilder->SetDepthAccess(
                    attachmentInfo.depthLoadOp, attachmentInfo.depthStoreOp,
                    attachmentInfo.clearDepth, view->GetD3D12Format());
            } else {
                renderPassBuilder->SetDepthNoAccess();
            }

            if (hasStencil) {
                renderPassBuilder->SetStencilAccess(
                    attachmentInfo.stencilLoadOp, attachmentInfo.stencilStoreOp,
                    attachmentInfo.clearStencil, view->GetD3D12Format());
            } else {
                renderPassBuilder->SetStencilNoAccess();
            }

        } else {
            renderPassBuilder->SetDepthStencilNoAccess();
        }

        return {};
    }

    void CommandBuffer::EmulateBeginRenderPass(CommandRecordingContext* commandContext,
                                               const RenderPassBuilder* renderPassBuilder) const {
        ID3D12GraphicsCommandList* commandList = commandContext->GetCommandList();

        // Clear framebuffer attachments as needed.
        {
            for (ColorAttachmentIndex i(uint8_t(0));
                 i < renderPassBuilder->GetColorAttachmentCount(); i++) {
                // Load op - color
                if (renderPassBuilder->GetRenderPassRenderTargetDescriptors()[i]
                        .BeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR) {
                    commandList->ClearRenderTargetView(
                        renderPassBuilder->GetRenderPassRenderTargetDescriptors()[i].cpuDescriptor,
                        renderPassBuilder->GetRenderPassRenderTargetDescriptors()[i]
                            .BeginningAccess.Clear.ClearValue.Color,
                        0, nullptr);
                }
            }

            if (renderPassBuilder->HasDepth()) {
                D3D12_CLEAR_FLAGS clearFlags = {};
                float depthClear = 0.0f;
                uint8_t stencilClear = 0u;

                if (renderPassBuilder->GetRenderPassDepthStencilDescriptor()
                        ->DepthBeginningAccess.Type ==
                    D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR) {
                    clearFlags |= D3D12_CLEAR_FLAG_DEPTH;
                    depthClear = renderPassBuilder->GetRenderPassDepthStencilDescriptor()
                                     ->DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth;
                }
                if (renderPassBuilder->GetRenderPassDepthStencilDescriptor()
                        ->StencilBeginningAccess.Type ==
                    D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR) {
                    clearFlags |= D3D12_CLEAR_FLAG_STENCIL;
                    stencilClear =
                        renderPassBuilder->GetRenderPassDepthStencilDescriptor()
                            ->StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil;
                }

                if (clearFlags) {
                    commandList->ClearDepthStencilView(
                        renderPassBuilder->GetRenderPassDepthStencilDescriptor()->cpuDescriptor,
                        clearFlags, depthClear, stencilClear, 0, nullptr);
                }
            }
        }

        commandList->OMSetRenderTargets(
            static_cast<uint8_t>(renderPassBuilder->GetColorAttachmentCount()),
            renderPassBuilder->GetRenderTargetViews(), FALSE,
            renderPassBuilder->HasDepth()
                ? &renderPassBuilder->GetRenderPassDepthStencilDescriptor()->cpuDescriptor
                : nullptr);
    }

    MaybeError CommandBuffer::RecordRenderPass(CommandRecordingContext* commandContext,
                                               BindGroupStateTracker* bindingTracker,
                                               BeginRenderPassCmd* renderPass,
                                               const bool passHasUAV) {
        Device* device = ToBackend(GetDevice());
        const bool useRenderPass = device->IsToggleEnabled(Toggle::UseD3D12RenderPass);

        // renderPassBuilder must be scoped to RecordRenderPass because any underlying
        // D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS structs must remain
        // valid until after EndRenderPass() has been called.
        RenderPassBuilder renderPassBuilder(passHasUAV);

        DAWN_TRY(SetupRenderPass(commandContext, renderPass, &renderPassBuilder));

        // Use D3D12's native render pass API if it's available, otherwise emulate the
        // beginning and ending access operations.
        if (useRenderPass) {
            commandContext->GetCommandList4()->BeginRenderPass(
                static_cast<uint8_t>(renderPassBuilder.GetColorAttachmentCount()),
                renderPassBuilder.GetRenderPassRenderTargetDescriptors().data(),
                renderPassBuilder.HasDepth()
                    ? renderPassBuilder.GetRenderPassDepthStencilDescriptor()
                    : nullptr,
                renderPassBuilder.GetRenderPassFlags());
        } else {
            EmulateBeginRenderPass(commandContext, &renderPassBuilder);
        }

        ID3D12GraphicsCommandList* commandList = commandContext->GetCommandList();

        // Set up default dynamic state
        {
            uint32_t width = renderPass->width;
            uint32_t height = renderPass->height;
            D3D12_VIEWPORT viewport = {
                0.f, 0.f, static_cast<float>(width), static_cast<float>(height), 0.f, 1.f};
            D3D12_RECT scissorRect = {0, 0, static_cast<long>(width), static_cast<long>(height)};
            commandList->RSSetViewports(1, &viewport);
            commandList->RSSetScissorRects(1, &scissorRect);

            static constexpr std::array<float, 4> defaultBlendFactor = {0, 0, 0, 0};
            commandList->OMSetBlendFactor(&defaultBlendFactor[0]);
        }

        RenderPipeline* lastPipeline = nullptr;
        VertexBufferTracker vertexBufferTracker = {};

        auto EncodeRenderBundleCommand = [&](CommandIterator* iter, Command type) -> MaybeError {
            switch (type) {
                case Command::Draw: {
                    DrawCmd* draw = iter->NextCommand<DrawCmd>();

                    DAWN_TRY(bindingTracker->Apply(commandContext));
                    vertexBufferTracker.Apply(commandList, lastPipeline);
                    RecordFirstIndexOffset(commandList, lastPipeline, draw->firstVertex,
                                           draw->firstInstance);
                    commandList->DrawInstanced(draw->vertexCount, draw->instanceCount,
                                               draw->firstVertex, draw->firstInstance);
                    break;
                }

                case Command::DrawIndexed: {
                    DrawIndexedCmd* draw = iter->NextCommand<DrawIndexedCmd>();

                    DAWN_TRY(bindingTracker->Apply(commandContext));
                    vertexBufferTracker.Apply(commandList, lastPipeline);
                    RecordFirstIndexOffset(commandList, lastPipeline, draw->baseVertex,
                                           draw->firstInstance);
                    commandList->DrawIndexedInstanced(draw->indexCount, draw->instanceCount,
                                                      draw->firstIndex, draw->baseVertex,
                                                      draw->firstInstance);
                    break;
                }

                case Command::DrawIndirect: {
                    DrawIndirectCmd* draw = iter->NextCommand<DrawIndirectCmd>();

                    DAWN_TRY(bindingTracker->Apply(commandContext));
                    vertexBufferTracker.Apply(commandList, lastPipeline);
                    Buffer* buffer = ToBackend(draw->indirectBuffer.Get());
                    ComPtr<ID3D12CommandSignature> signature =
                        ToBackend(GetDevice())->GetDrawIndirectSignature();
                    commandList->ExecuteIndirect(signature.Get(), 1, buffer->GetD3D12Resource(),
                                                 draw->indirectOffset, nullptr, 0);
                    break;
                }

                case Command::DrawIndexedIndirect: {
                    DrawIndexedIndirectCmd* draw = iter->NextCommand<DrawIndexedIndirectCmd>();

                    DAWN_TRY(bindingTracker->Apply(commandContext));
                    vertexBufferTracker.Apply(commandList, lastPipeline);
                    Buffer* buffer = ToBackend(draw->indirectBuffer.Get());
                    ComPtr<ID3D12CommandSignature> signature =
                        ToBackend(GetDevice())->GetDrawIndexedIndirectSignature();
                    commandList->ExecuteIndirect(signature.Get(), 1, buffer->GetD3D12Resource(),
                                                 draw->indirectOffset, nullptr, 0);
                    break;
                }

                case Command::InsertDebugMarker: {
                    InsertDebugMarkerCmd* cmd = iter->NextCommand<InsertDebugMarkerCmd>();
                    const char* label = iter->NextData<char>(cmd->length + 1);

                    if (ToBackend(GetDevice())->GetFunctions()->IsPIXEventRuntimeLoaded()) {
                        // PIX color is 1 byte per channel in ARGB format
                        constexpr uint64_t kPIXBlackColor = 0xff000000;
                        ToBackend(GetDevice())
                            ->GetFunctions()
                            ->pixSetMarkerOnCommandList(commandList, kPIXBlackColor, label);
                    }
                    break;
                }

                case Command::PopDebugGroup: {
                    iter->NextCommand<PopDebugGroupCmd>();

                    if (ToBackend(GetDevice())->GetFunctions()->IsPIXEventRuntimeLoaded()) {
                        ToBackend(GetDevice())
                            ->GetFunctions()
                            ->pixEndEventOnCommandList(commandList);
                    }
                    break;
                }

                case Command::PushDebugGroup: {
                    PushDebugGroupCmd* cmd = iter->NextCommand<PushDebugGroupCmd>();
                    const char* label = iter->NextData<char>(cmd->length + 1);

                    if (ToBackend(GetDevice())->GetFunctions()->IsPIXEventRuntimeLoaded()) {
                        // PIX color is 1 byte per channel in ARGB format
                        constexpr uint64_t kPIXBlackColor = 0xff000000;
                        ToBackend(GetDevice())
                            ->GetFunctions()
                            ->pixBeginEventOnCommandList(commandList, kPIXBlackColor, label);
                    }
                    break;
                }

                case Command::SetRenderPipeline: {
                    SetRenderPipelineCmd* cmd = iter->NextCommand<SetRenderPipelineCmd>();
                    RenderPipeline* pipeline = ToBackend(cmd->pipeline).Get();

                    commandList->SetPipelineState(pipeline->GetPipelineState());
                    commandList->IASetPrimitiveTopology(pipeline->GetD3D12PrimitiveTopology());

                    bindingTracker->OnSetPipeline(pipeline);

                    lastPipeline = pipeline;
                    break;
                }

                case Command::SetBindGroup: {
                    SetBindGroupCmd* cmd = iter->NextCommand<SetBindGroupCmd>();
                    BindGroup* group = ToBackend(cmd->group.Get());
                    uint32_t* dynamicOffsets = nullptr;

                    if (cmd->dynamicOffsetCount > 0) {
                        dynamicOffsets = iter->NextData<uint32_t>(cmd->dynamicOffsetCount);
                    }

                    bindingTracker->OnSetBindGroup(cmd->index, group, cmd->dynamicOffsetCount,
                                                   dynamicOffsets);
                    break;
                }

                case Command::SetIndexBuffer: {
                    SetIndexBufferCmd* cmd = iter->NextCommand<SetIndexBufferCmd>();

                    D3D12_INDEX_BUFFER_VIEW bufferView;
                    bufferView.Format = DXGIIndexFormat(cmd->format);
                    bufferView.BufferLocation = ToBackend(cmd->buffer)->GetVA() + cmd->offset;
                    bufferView.SizeInBytes = cmd->size;

                    commandList->IASetIndexBuffer(&bufferView);
                    break;
                }

                case Command::SetVertexBuffer: {
                    SetVertexBufferCmd* cmd = iter->NextCommand<SetVertexBufferCmd>();

                    vertexBufferTracker.OnSetVertexBuffer(cmd->slot, ToBackend(cmd->buffer.Get()),
                                                          cmd->offset, cmd->size);
                    break;
                }

                default:
                    UNREACHABLE();
                    break;
            }
            return {};
        };

        Command type;
        while (mCommands.NextCommandId(&type)) {
            switch (type) {
                case Command::EndRenderPass: {
                    mCommands.NextCommand<EndRenderPassCmd>();
                    if (useRenderPass) {
                        commandContext->GetCommandList4()->EndRenderPass();
                    } else if (renderPass->attachmentState->GetSampleCount() > 1) {
                        ResolveMultisampledRenderPass(commandContext, renderPass);
                    }
                    return {};
                }

                case Command::SetStencilReference: {
                    SetStencilReferenceCmd* cmd = mCommands.NextCommand<SetStencilReferenceCmd>();

                    commandList->OMSetStencilRef(cmd->reference);
                    break;
                }

                case Command::SetViewport: {
                    SetViewportCmd* cmd = mCommands.NextCommand<SetViewportCmd>();
                    D3D12_VIEWPORT viewport;
                    viewport.TopLeftX = cmd->x;
                    viewport.TopLeftY = cmd->y;
                    viewport.Width = cmd->width;
                    viewport.Height = cmd->height;
                    viewport.MinDepth = cmd->minDepth;
                    viewport.MaxDepth = cmd->maxDepth;

                    commandList->RSSetViewports(1, &viewport);
                    break;
                }

                case Command::SetScissorRect: {
                    SetScissorRectCmd* cmd = mCommands.NextCommand<SetScissorRectCmd>();
                    D3D12_RECT rect;
                    rect.left = cmd->x;
                    rect.top = cmd->y;
                    rect.right = cmd->x + cmd->width;
                    rect.bottom = cmd->y + cmd->height;

                    commandList->RSSetScissorRects(1, &rect);
                    break;
                }

                case Command::SetBlendConstant: {
                    SetBlendConstantCmd* cmd = mCommands.NextCommand<SetBlendConstantCmd>();
                    const std::array<float, 4> color = ConvertToFloatColor(cmd->color);
                    commandList->OMSetBlendFactor(color.data());
                    break;
                }

                case Command::ExecuteBundles: {
                    ExecuteBundlesCmd* cmd = mCommands.NextCommand<ExecuteBundlesCmd>();
                    auto bundles = mCommands.NextData<Ref<RenderBundleBase>>(cmd->count);

                    for (uint32_t i = 0; i < cmd->count; ++i) {
                        CommandIterator* iter = bundles[i]->GetCommands();
                        iter->Reset();
                        while (iter->NextCommandId(&type)) {
                            DAWN_TRY(EncodeRenderBundleCommand(iter, type));
                        }
                    }
                    break;
                }

                case Command::BeginOcclusionQuery: {
                    BeginOcclusionQueryCmd* cmd = mCommands.NextCommand<BeginOcclusionQueryCmd>();
                    QuerySet* querySet = ToBackend(cmd->querySet.Get());
                    ASSERT(D3D12QueryType(querySet->GetQueryType()) ==
                           D3D12_QUERY_TYPE_BINARY_OCCLUSION);
                    commandList->BeginQuery(querySet->GetQueryHeap(),
                                            D3D12_QUERY_TYPE_BINARY_OCCLUSION, cmd->queryIndex);
                    break;
                }

                case Command::EndOcclusionQuery: {
                    EndOcclusionQueryCmd* cmd = mCommands.NextCommand<EndOcclusionQueryCmd>();
                    QuerySet* querySet = ToBackend(cmd->querySet.Get());
                    ASSERT(D3D12QueryType(querySet->GetQueryType()) ==
                           D3D12_QUERY_TYPE_BINARY_OCCLUSION);
                    commandList->EndQuery(querySet->GetQueryHeap(),
                                          D3D12_QUERY_TYPE_BINARY_OCCLUSION, cmd->queryIndex);
                    break;
                }

                case Command::WriteTimestamp: {
                    WriteTimestampCmd* cmd = mCommands.NextCommand<WriteTimestampCmd>();

                    RecordWriteTimestampCmd(commandList, cmd);
                    break;
                }

                default: {
                    DAWN_TRY(EncodeRenderBundleCommand(&mCommands, type));
                    break;
                }
            }
        }
        return {};
    }
}}  // namespace dawn_native::d3d12
