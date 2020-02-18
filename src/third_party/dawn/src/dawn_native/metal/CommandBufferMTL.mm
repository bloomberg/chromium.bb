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

#include "dawn_native/metal/CommandBufferMTL.h"

#include "dawn_native/BindGroup.h"
#include "dawn_native/CommandEncoder.h"
#include "dawn_native/Commands.h"
#include "dawn_native/metal/BufferMTL.h"
#include "dawn_native/metal/ComputePipelineMTL.h"
#include "dawn_native/metal/DeviceMTL.h"
#include "dawn_native/metal/PipelineLayoutMTL.h"
#include "dawn_native/metal/RenderPipelineMTL.h"
#include "dawn_native/metal/SamplerMTL.h"
#include "dawn_native/metal/TextureMTL.h"

namespace dawn_native { namespace metal {

    struct GlobalEncoders {
        id<MTLBlitCommandEncoder> blit = nil;

        void Finish() {
            if (blit != nil) {
                [blit endEncoding];
                blit = nil;  // This will be autoreleased.
            }
        }

        void EnsureBlit(id<MTLCommandBuffer> commandBuffer) {
            if (blit == nil) {
                blit = [commandBuffer blitCommandEncoder];
            }
        }
    };

    namespace {

        // Creates an autoreleased MTLRenderPassDescriptor matching desc
        MTLRenderPassDescriptor* CreateMTLRenderPassDescriptor(BeginRenderPassCmd* renderPass) {
            MTLRenderPassDescriptor* descriptor = [MTLRenderPassDescriptor renderPassDescriptor];

            for (uint32_t i : IterateBitSet(renderPass->colorAttachmentsSet)) {
                auto& attachmentInfo = renderPass->colorAttachments[i];

                if (attachmentInfo.loadOp == dawn::LoadOp::Clear) {
                    descriptor.colorAttachments[i].loadAction = MTLLoadActionClear;
                    descriptor.colorAttachments[i].clearColor =
                        MTLClearColorMake(attachmentInfo.clearColor.r, attachmentInfo.clearColor.g,
                                          attachmentInfo.clearColor.b, attachmentInfo.clearColor.a);
                } else {
                    descriptor.colorAttachments[i].loadAction = MTLLoadActionLoad;
                }

                descriptor.colorAttachments[i].texture =
                    ToBackend(attachmentInfo.view->GetTexture())->GetMTLTexture();
                descriptor.colorAttachments[i].level = attachmentInfo.view->GetBaseMipLevel();
                descriptor.colorAttachments[i].slice = attachmentInfo.view->GetBaseArrayLayer();

                if (attachmentInfo.storeOp == dawn::StoreOp::Store) {
                    if (attachmentInfo.resolveTarget.Get() != nullptr) {
                        descriptor.colorAttachments[i].resolveTexture =
                            ToBackend(attachmentInfo.resolveTarget->GetTexture())->GetMTLTexture();
                        descriptor.colorAttachments[i].resolveLevel =
                            attachmentInfo.resolveTarget->GetBaseMipLevel();
                        descriptor.colorAttachments[i].resolveSlice =
                            attachmentInfo.resolveTarget->GetBaseArrayLayer();
                        descriptor.colorAttachments[i].storeAction =
                            MTLStoreActionStoreAndMultisampleResolve;
                    } else {
                        descriptor.colorAttachments[i].storeAction = MTLStoreActionStore;
                    }
                }
            }

            if (renderPass->hasDepthStencilAttachment) {
                auto& attachmentInfo = renderPass->depthStencilAttachment;

                // TODO(jiawei.shao@intel.com): support rendering into a layer of a texture.
                id<MTLTexture> texture =
                    ToBackend(attachmentInfo.view->GetTexture())->GetMTLTexture();
                const Format& format = attachmentInfo.view->GetTexture()->GetFormat();

                if (format.HasDepth()) {
                    descriptor.depthAttachment.texture = texture;
                    descriptor.depthAttachment.storeAction = MTLStoreActionStore;

                    if (attachmentInfo.depthLoadOp == dawn::LoadOp::Clear) {
                        descriptor.depthAttachment.loadAction = MTLLoadActionClear;
                        descriptor.depthAttachment.clearDepth = attachmentInfo.clearDepth;
                    } else {
                        descriptor.depthAttachment.loadAction = MTLLoadActionLoad;
                    }
                }

                if (format.HasStencil()) {
                    descriptor.stencilAttachment.texture = texture;
                    descriptor.stencilAttachment.storeAction = MTLStoreActionStore;

                    if (attachmentInfo.stencilLoadOp == dawn::LoadOp::Clear) {
                        descriptor.stencilAttachment.loadAction = MTLLoadActionClear;
                        descriptor.stencilAttachment.clearStencil = attachmentInfo.clearStencil;
                    } else {
                        descriptor.stencilAttachment.loadAction = MTLLoadActionLoad;
                    }
                }
            }

            return descriptor;
        }

        // Helper function for Toggle EmulateStoreAndMSAAResolve
        void ResolveInAnotherRenderPass(
            id<MTLCommandBuffer> commandBuffer,
            const MTLRenderPassDescriptor* mtlRenderPass,
            const std::array<id<MTLTexture>, kMaxColorAttachments>& resolveTextures) {
            MTLRenderPassDescriptor* mtlRenderPassForResolve =
                [MTLRenderPassDescriptor renderPassDescriptor];
            for (uint32_t i = 0; i < kMaxColorAttachments; ++i) {
                if (resolveTextures[i] == nil) {
                    continue;
                }

                mtlRenderPassForResolve.colorAttachments[i].texture =
                    mtlRenderPass.colorAttachments[i].texture;
                mtlRenderPassForResolve.colorAttachments[i].loadAction = MTLLoadActionLoad;
                mtlRenderPassForResolve.colorAttachments[i].storeAction =
                    MTLStoreActionMultisampleResolve;
                mtlRenderPassForResolve.colorAttachments[i].resolveTexture = resolveTextures[i];
                mtlRenderPassForResolve.colorAttachments[i].resolveLevel =
                    mtlRenderPass.colorAttachments[i].resolveLevel;
                mtlRenderPassForResolve.colorAttachments[i].resolveSlice =
                    mtlRenderPass.colorAttachments[i].resolveSlice;
            }

            id<MTLRenderCommandEncoder> encoder =
                [commandBuffer renderCommandEncoderWithDescriptor:mtlRenderPassForResolve];
            [encoder endEncoding];
        }

        // Helper functions for Toggle AlwaysResolveIntoZeroLevelAndLayer
        id<MTLTexture> CreateResolveTextureForWorkaround(Device* device,
                                                         MTLPixelFormat mtlFormat,
                                                         uint32_t width,
                                                         uint32_t height) {
            MTLTextureDescriptor* mtlDesc = [MTLTextureDescriptor new];
            mtlDesc.textureType = MTLTextureType2D;
            mtlDesc.usage = MTLTextureUsageRenderTarget;
            mtlDesc.pixelFormat = mtlFormat;
            mtlDesc.width = width;
            mtlDesc.height = height;
            mtlDesc.depth = 1;
            mtlDesc.mipmapLevelCount = 1;
            mtlDesc.arrayLength = 1;
            mtlDesc.storageMode = MTLStorageModePrivate;
            mtlDesc.sampleCount = 1;
            id<MTLTexture> resolveTexture =
                [device->GetMTLDevice() newTextureWithDescriptor:mtlDesc];
            [mtlDesc release];
            return resolveTexture;
        }

        void CopyIntoTrueResolveTarget(id<MTLCommandBuffer> commandBuffer,
                                       id<MTLTexture> mtlTrueResolveTexture,
                                       uint32_t trueResolveLevel,
                                       uint32_t trueResolveSlice,
                                       id<MTLTexture> temporaryResolveTexture,
                                       uint32_t width,
                                       uint32_t height,
                                       GlobalEncoders* encoders) {
            encoders->EnsureBlit(commandBuffer);
            [encoders->blit copyFromTexture:temporaryResolveTexture
                                sourceSlice:0
                                sourceLevel:0
                               sourceOrigin:MTLOriginMake(0, 0, 0)
                                 sourceSize:MTLSizeMake(width, height, 1)
                                  toTexture:mtlTrueResolveTexture
                           destinationSlice:trueResolveSlice
                           destinationLevel:trueResolveLevel
                          destinationOrigin:MTLOriginMake(0, 0, 0)];
        }

        // Handles a call to SetBindGroup, directing the commands to the correct encoder.
        // There is a single function that takes both encoders to factor code. Other approaches like
        // templates wouldn't work because the name of methods are different between the two encoder
        // types.
        void ApplyBindGroup(uint32_t index,
                            BindGroup* group,
                            uint32_t dynamicOffsetCount,
                            uint64_t* dynamicOffsets,
                            PipelineLayout* pipelineLayout,
                            id<MTLRenderCommandEncoder> render,
                            id<MTLComputeCommandEncoder> compute) {
            const auto& layout = group->GetLayout()->GetBindingInfo();
            uint32_t currentDynamicBufferIndex = 0;

            // TODO(kainino@chromium.org): Maintain buffers and offsets arrays in BindGroup
            // so that we only have to do one setVertexBuffers and one setFragmentBuffers
            // call here.
            for (uint32_t bindingIndex : IterateBitSet(layout.mask)) {
                auto stage = layout.visibilities[bindingIndex];
                bool hasVertStage = stage & dawn::ShaderStageBit::Vertex && render != nil;
                bool hasFragStage = stage & dawn::ShaderStageBit::Fragment && render != nil;
                bool hasComputeStage = stage & dawn::ShaderStageBit::Compute && compute != nil;

                uint32_t vertIndex = 0;
                uint32_t fragIndex = 0;
                uint32_t computeIndex = 0;

                if (hasVertStage) {
                    vertIndex = pipelineLayout->GetBindingIndexInfo(
                        ShaderStage::Vertex)[index][bindingIndex];
                }
                if (hasFragStage) {
                    fragIndex = pipelineLayout->GetBindingIndexInfo(
                        ShaderStage::Fragment)[index][bindingIndex];
                }
                if (hasComputeStage) {
                    computeIndex = pipelineLayout->GetBindingIndexInfo(
                        ShaderStage::Compute)[index][bindingIndex];
                }

                switch (layout.types[bindingIndex]) {
                    case dawn::BindingType::UniformBuffer:
                    case dawn::BindingType::StorageBuffer: {
                        BufferBinding binding = group->GetBindingAsBufferBinding(bindingIndex);
                        const id<MTLBuffer> buffer = ToBackend(binding.buffer)->GetMTLBuffer();
                        NSUInteger offset = binding.offset;

                        // TODO(shaobo.yan@intel.com): Record bound buffer status to use
                        // setBufferOffset to achieve better performance.
                        if (layout.dynamic[bindingIndex]) {
                            offset += dynamicOffsets[currentDynamicBufferIndex];
                            currentDynamicBufferIndex++;
                        }

                        if (hasVertStage) {
                            [render setVertexBuffers:&buffer
                                             offsets:&offset
                                           withRange:NSMakeRange(vertIndex, 1)];
                        }
                        if (hasFragStage) {
                            [render setFragmentBuffers:&buffer
                                               offsets:&offset
                                             withRange:NSMakeRange(fragIndex, 1)];
                        }
                        if (hasComputeStage) {
                            [compute setBuffers:&buffer
                                        offsets:&offset
                                      withRange:NSMakeRange(computeIndex, 1)];
                        }

                    } break;

                    case dawn::BindingType::Sampler: {
                        auto sampler = ToBackend(group->GetBindingAsSampler(bindingIndex));
                        if (hasVertStage) {
                            [render setVertexSamplerState:sampler->GetMTLSamplerState()
                                                  atIndex:vertIndex];
                        }
                        if (hasFragStage) {
                            [render setFragmentSamplerState:sampler->GetMTLSamplerState()
                                                    atIndex:fragIndex];
                        }
                        if (hasComputeStage) {
                            [compute setSamplerState:sampler->GetMTLSamplerState()
                                             atIndex:computeIndex];
                        }
                    } break;

                    case dawn::BindingType::SampledTexture: {
                        auto textureView = ToBackend(group->GetBindingAsTextureView(bindingIndex));
                        if (hasVertStage) {
                            [render setVertexTexture:textureView->GetMTLTexture()
                                             atIndex:vertIndex];
                        }
                        if (hasFragStage) {
                            [render setFragmentTexture:textureView->GetMTLTexture()
                                               atIndex:fragIndex];
                        }
                        if (hasComputeStage) {
                            [compute setTexture:textureView->GetMTLTexture() atIndex:computeIndex];
                        }
                    } break;

                    case dawn::BindingType::StorageTexture:
                    case dawn::BindingType::ReadonlyStorageBuffer:
                        UNREACHABLE();
                        break;
                }
            }
        }

        struct TextureBufferCopySplit {
            static constexpr uint32_t kMaxTextureBufferCopyRegions = 3;

            struct CopyInfo {
                NSUInteger bufferOffset;
                NSUInteger bytesPerRow;
                NSUInteger bytesPerImage;
                MTLOrigin textureOrigin;
                MTLSize copyExtent;
            };

            uint32_t count = 0;
            std::array<CopyInfo, kMaxTextureBufferCopyRegions> copies;
        };

        MTLOrigin MakeMTLOrigin(Origin3D origin) {
            return MTLOriginMake(origin.x, origin.y, origin.z);
        }

        MTLSize MakeMTLSize(Extent3D extent) {
            return MTLSizeMake(extent.width, extent.height, extent.depth);
        }

        TextureBufferCopySplit ComputeTextureBufferCopySplit(Origin3D origin,
                                                             Extent3D copyExtent,
                                                             Format textureFormat,
                                                             Extent3D virtualSizeAtLevel,
                                                             uint64_t bufferSize,
                                                             uint64_t bufferOffset,
                                                             uint32_t rowPitch,
                                                             uint32_t imageHeight) {
            TextureBufferCopySplit copy;

            // When copying textures from/to an unpacked buffer, the Metal validation layer doesn't
            // compute the correct range when checking if the buffer is big enough to contain the
            // data for the whole copy. Instead of looking at the position of the last texel in the
            // buffer, it computes the volume of the 3D box with rowPitch * (imageHeight /
            // format.blockHeight) * copySize.depth. For example considering the pixel buffer below
            // where in memory, each row data (D) of the texture is followed by some padding data
            // (P):
            //     |DDDDDDD|PP|
            //     |DDDDDDD|PP|
            //     |DDDDDDD|PP|
            //     |DDDDDDD|PP|
            //     |DDDDDDA|PP|
            // The last pixel read will be A, but the driver will think it is the whole last padding
            // row, causing it to generate an error when the pixel buffer is just big enough.

            // We work around this limitation by detecting when Metal would complain and copy the
            // last image and row separately using tight sourceBytesPerRow or sourceBytesPerImage.
            uint32_t rowPitchCountPerImage = imageHeight / textureFormat.blockHeight;
            uint32_t bytesPerImage = rowPitch * rowPitchCountPerImage;

            // Metal validation layer requires that if the texture's pixel format is a compressed
            // format, the sourceSize must be a multiple of the pixel format's block size or be
            // clamped to the edge of the texture if the block extends outside the bounds of a
            // texture.
            uint32_t clampedCopyExtentWidth =
                (origin.x + copyExtent.width > virtualSizeAtLevel.width)
                    ? (virtualSizeAtLevel.width - origin.x)
                    : copyExtent.width;
            uint32_t clampedCopyExtentHeight =
                (origin.y + copyExtent.height > virtualSizeAtLevel.height)
                    ? (virtualSizeAtLevel.height - origin.y)
                    : copyExtent.height;

            // Check whether buffer size is big enough.
            bool needWorkaround = bufferSize - bufferOffset < bytesPerImage * copyExtent.depth;
            if (!needWorkaround) {
                copy.count = 1;
                copy.copies[0].bufferOffset = bufferOffset;
                copy.copies[0].bytesPerRow = rowPitch;
                copy.copies[0].bytesPerImage = bytesPerImage;
                copy.copies[0].textureOrigin = MakeMTLOrigin(origin);
                copy.copies[0].copyExtent =
                    MTLSizeMake(clampedCopyExtentWidth, clampedCopyExtentHeight, copyExtent.depth);
                return copy;
            }

            uint64_t currentOffset = bufferOffset;

            // Doing all the copy except the last image.
            if (copyExtent.depth > 1) {
                copy.copies[copy.count].bufferOffset = currentOffset;
                copy.copies[copy.count].bytesPerRow = rowPitch;
                copy.copies[copy.count].bytesPerImage = bytesPerImage;
                copy.copies[copy.count].textureOrigin = MakeMTLOrigin(origin);
                copy.copies[copy.count].copyExtent = MTLSizeMake(
                    clampedCopyExtentWidth, clampedCopyExtentHeight, copyExtent.depth - 1);

                ++copy.count;

                // Update offset to copy to the last image.
                currentOffset += (copyExtent.depth - 1) * bytesPerImage;
            }

            // Doing all the copy in last image except the last row.
            uint32_t copyBlockRowCount = copyExtent.height / textureFormat.blockHeight;
            if (copyBlockRowCount > 1) {
                copy.copies[copy.count].bufferOffset = currentOffset;
                copy.copies[copy.count].bytesPerRow = rowPitch;
                copy.copies[copy.count].bytesPerImage = rowPitch * (copyBlockRowCount - 1);
                copy.copies[copy.count].textureOrigin =
                    MTLOriginMake(origin.x, origin.y, origin.z + copyExtent.depth - 1);

                ASSERT(copyExtent.height - textureFormat.blockHeight < virtualSizeAtLevel.height);
                copy.copies[copy.count].copyExtent = MTLSizeMake(
                    clampedCopyExtentWidth, copyExtent.height - textureFormat.blockHeight, 1);

                ++copy.count;

                // Update offset to copy to the last row.
                currentOffset += (copyBlockRowCount - 1) * rowPitch;
            }

            // Doing the last row copy with the exact number of bytes in last row.
            // Workaround this issue in a way just like the copy to a 1D texture.
            uint32_t lastRowDataSize =
                (copyExtent.width / textureFormat.blockWidth) * textureFormat.blockByteSize;
            uint32_t lastRowCopyExtentHeight =
                textureFormat.blockHeight + clampedCopyExtentHeight - copyExtent.height;
            ASSERT(lastRowCopyExtentHeight <= textureFormat.blockHeight);

            copy.copies[copy.count].bufferOffset = currentOffset;
            copy.copies[copy.count].bytesPerRow = lastRowDataSize;
            copy.copies[copy.count].bytesPerImage = lastRowDataSize;
            copy.copies[copy.count].textureOrigin =
                MTLOriginMake(origin.x, origin.y + copyExtent.height - textureFormat.blockHeight,
                              origin.z + copyExtent.depth - 1);
            copy.copies[copy.count].copyExtent =
                MTLSizeMake(clampedCopyExtentWidth, lastRowCopyExtentHeight, 1);
            ++copy.count;

            return copy;
        }
    }  // anonymous namespace

    CommandBuffer::CommandBuffer(CommandEncoderBase* encoder,
                                 const CommandBufferDescriptor* descriptor)
        : CommandBufferBase(encoder, descriptor), mCommands(encoder->AcquireCommands()) {
    }

    CommandBuffer::~CommandBuffer() {
        FreeCommands(&mCommands);
    }

    void CommandBuffer::FillCommands(id<MTLCommandBuffer> commandBuffer) {
        GlobalEncoders encoders;

        Command type;
        while (mCommands.NextCommandId(&type)) {
            switch (type) {
                case Command::BeginComputePass: {
                    mCommands.NextCommand<BeginComputePassCmd>();
                    encoders.Finish();
                    EncodeComputePass(commandBuffer);
                } break;

                case Command::BeginRenderPass: {
                    BeginRenderPassCmd* cmd = mCommands.NextCommand<BeginRenderPassCmd>();
                    encoders.Finish();
                    MTLRenderPassDescriptor* descriptor = CreateMTLRenderPassDescriptor(cmd);
                    EncodeRenderPass(commandBuffer, descriptor, &encoders, cmd->width, cmd->height);
                } break;

                case Command::CopyBufferToBuffer: {
                    CopyBufferToBufferCmd* copy = mCommands.NextCommand<CopyBufferToBufferCmd>();

                    encoders.EnsureBlit(commandBuffer);
                    [encoders.blit copyFromBuffer:ToBackend(copy->source)->GetMTLBuffer()
                                     sourceOffset:copy->sourceOffset
                                         toBuffer:ToBackend(copy->destination)->GetMTLBuffer()
                                destinationOffset:copy->destinationOffset
                                             size:copy->size];
                } break;

                case Command::CopyBufferToTexture: {
                    CopyBufferToTextureCmd* copy = mCommands.NextCommand<CopyBufferToTextureCmd>();
                    auto& src = copy->source;
                    auto& dst = copy->destination;
                    auto& copySize = copy->copySize;
                    Buffer* buffer = ToBackend(src.buffer.Get());
                    Texture* texture = ToBackend(dst.texture.Get());

                    Extent3D virtualSizeAtLevel = texture->GetMipLevelVirtualSize(dst.mipLevel);
                    TextureBufferCopySplit splittedCopies = ComputeTextureBufferCopySplit(
                        dst.origin, copySize, texture->GetFormat(), virtualSizeAtLevel,
                        buffer->GetSize(), src.offset, src.rowPitch, src.imageHeight);

                    encoders.EnsureBlit(commandBuffer);
                    for (uint32_t i = 0; i < splittedCopies.count; ++i) {
                        const TextureBufferCopySplit::CopyInfo& copyInfo = splittedCopies.copies[i];
                        [encoders.blit copyFromBuffer:buffer->GetMTLBuffer()
                                         sourceOffset:copyInfo.bufferOffset
                                    sourceBytesPerRow:copyInfo.bytesPerRow
                                  sourceBytesPerImage:copyInfo.bytesPerImage
                                           sourceSize:copyInfo.copyExtent
                                            toTexture:texture->GetMTLTexture()
                                     destinationSlice:dst.arrayLayer
                                     destinationLevel:dst.mipLevel
                                    destinationOrigin:copyInfo.textureOrigin];
                    }
                } break;

                case Command::CopyTextureToBuffer: {
                    CopyTextureToBufferCmd* copy = mCommands.NextCommand<CopyTextureToBufferCmd>();
                    auto& src = copy->source;
                    auto& dst = copy->destination;
                    auto& copySize = copy->copySize;
                    Texture* texture = ToBackend(src.texture.Get());
                    Buffer* buffer = ToBackend(dst.buffer.Get());

                    Extent3D virtualSizeAtLevel = texture->GetMipLevelVirtualSize(src.mipLevel);
                    TextureBufferCopySplit splittedCopies = ComputeTextureBufferCopySplit(
                        src.origin, copySize, texture->GetFormat(), virtualSizeAtLevel,
                        buffer->GetSize(), dst.offset, dst.rowPitch, dst.imageHeight);

                    encoders.EnsureBlit(commandBuffer);
                    for (uint32_t i = 0; i < splittedCopies.count; ++i) {
                        const TextureBufferCopySplit::CopyInfo& copyInfo = splittedCopies.copies[i];
                        [encoders.blit copyFromTexture:texture->GetMTLTexture()
                                           sourceSlice:src.arrayLayer
                                           sourceLevel:src.mipLevel
                                          sourceOrigin:copyInfo.textureOrigin
                                            sourceSize:copyInfo.copyExtent
                                              toBuffer:buffer->GetMTLBuffer()
                                     destinationOffset:copyInfo.bufferOffset
                                destinationBytesPerRow:copyInfo.bytesPerRow
                              destinationBytesPerImage:copyInfo.bytesPerImage];
                    }
                } break;

                case Command::CopyTextureToTexture: {
                    CopyTextureToTextureCmd* copy =
                        mCommands.NextCommand<CopyTextureToTextureCmd>();
                    Texture* srcTexture = ToBackend(copy->source.texture.Get());
                    Texture* dstTexture = ToBackend(copy->destination.texture.Get());

                    encoders.EnsureBlit(commandBuffer);

                    [encoders.blit copyFromTexture:srcTexture->GetMTLTexture()
                                       sourceSlice:copy->source.arrayLayer
                                       sourceLevel:copy->source.mipLevel
                                      sourceOrigin:MakeMTLOrigin(copy->source.origin)
                                        sourceSize:MakeMTLSize(copy->copySize)
                                         toTexture:dstTexture->GetMTLTexture()
                                  destinationSlice:copy->destination.arrayLayer
                                  destinationLevel:copy->destination.mipLevel
                                 destinationOrigin:MakeMTLOrigin(copy->destination.origin)];
                } break;

                default: { UNREACHABLE(); } break;
            }
        }

        encoders.Finish();
    }

    void CommandBuffer::EncodeComputePass(id<MTLCommandBuffer> commandBuffer) {
        ComputePipeline* lastPipeline = nullptr;

        // Will be autoreleased
        id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];

        Command type;
        while (mCommands.NextCommandId(&type)) {
            switch (type) {
                case Command::EndComputePass: {
                    mCommands.NextCommand<EndComputePassCmd>();
                    [encoder endEncoding];
                    return;
                } break;

                case Command::Dispatch: {
                    DispatchCmd* dispatch = mCommands.NextCommand<DispatchCmd>();
                    [encoder dispatchThreadgroups:MTLSizeMake(dispatch->x, dispatch->y, dispatch->z)
                            threadsPerThreadgroup:lastPipeline->GetLocalWorkGroupSize()];
                } break;

                case Command::DispatchIndirect: {
                    DispatchIndirectCmd* dispatch = mCommands.NextCommand<DispatchIndirectCmd>();

                    Buffer* buffer = ToBackend(dispatch->indirectBuffer.Get());
                    id<MTLBuffer> indirectBuffer = buffer->GetMTLBuffer();
                    [encoder dispatchThreadgroupsWithIndirectBuffer:indirectBuffer
                                               indirectBufferOffset:dispatch->indirectOffset
                                              threadsPerThreadgroup:lastPipeline
                                                                        ->GetLocalWorkGroupSize()];
                } break;

                case Command::SetComputePipeline: {
                    SetComputePipelineCmd* cmd = mCommands.NextCommand<SetComputePipelineCmd>();
                    lastPipeline = ToBackend(cmd->pipeline).Get();

                    lastPipeline->Encode(encoder);
                } break;

                case Command::SetBindGroup: {
                    SetBindGroupCmd* cmd = mCommands.NextCommand<SetBindGroupCmd>();
                    uint64_t* dynamicOffsets = nullptr;
                    if (cmd->dynamicOffsetCount > 0) {
                        dynamicOffsets = mCommands.NextData<uint64_t>(cmd->dynamicOffsetCount);
                    }

                    ApplyBindGroup(cmd->index, ToBackend(cmd->group.Get()), cmd->dynamicOffsetCount,
                                   dynamicOffsets, ToBackend(lastPipeline->GetLayout()), nil,
                                   encoder);
                } break;

                default: { UNREACHABLE(); } break;
            }
        }

        // EndComputePass should have been called
        UNREACHABLE();
    }

    void CommandBuffer::EncodeRenderPass(id<MTLCommandBuffer> commandBuffer,
                                         MTLRenderPassDescriptor* mtlRenderPass,
                                         GlobalEncoders* globalEncoders,
                                         uint32_t width,
                                         uint32_t height) {
        ASSERT(mtlRenderPass && globalEncoders);

        Device* device = ToBackend(GetDevice());

        // Handle Toggle AlwaysResolveIntoZeroLevelAndLayer. We must handle this before applying
        // the store + MSAA resolve workaround, otherwise this toggle will never be handled because
        // the resolve texture is removed when applying the store + MSAA resolve workaround.
        if (device->IsToggleEnabled(Toggle::AlwaysResolveIntoZeroLevelAndLayer)) {
            std::array<id<MTLTexture>, kMaxColorAttachments> trueResolveTextures = {};
            std::array<uint32_t, kMaxColorAttachments> trueResolveLevels = {};
            std::array<uint32_t, kMaxColorAttachments> trueResolveSlices = {};

            // Use temporary resolve texture on the resolve targets with non-zero resolveLevel or
            // resolveSlice.
            bool useTemporaryResolveTexture = false;
            std::array<id<MTLTexture>, kMaxColorAttachments> temporaryResolveTextures = {};
            for (uint32_t i = 0; i < kMaxColorAttachments; ++i) {
                if (mtlRenderPass.colorAttachments[i].resolveTexture == nil) {
                    continue;
                }

                if (mtlRenderPass.colorAttachments[i].resolveLevel == 0 &&
                    mtlRenderPass.colorAttachments[i].resolveSlice == 0) {
                    continue;
                }

                trueResolveTextures[i] = mtlRenderPass.colorAttachments[i].resolveTexture;
                trueResolveLevels[i] = mtlRenderPass.colorAttachments[i].resolveLevel;
                trueResolveSlices[i] = mtlRenderPass.colorAttachments[i].resolveSlice;

                const MTLPixelFormat mtlFormat = trueResolveTextures[i].pixelFormat;
                temporaryResolveTextures[i] =
                    CreateResolveTextureForWorkaround(device, mtlFormat, width, height);

                mtlRenderPass.colorAttachments[i].resolveTexture = temporaryResolveTextures[i];
                mtlRenderPass.colorAttachments[i].resolveLevel = 0;
                mtlRenderPass.colorAttachments[i].resolveSlice = 0;
                useTemporaryResolveTexture = true;
            }

            // If we need to use a temporary resolve texture we need to copy the result of MSAA
            // resolve back to the true resolve targets.
            if (useTemporaryResolveTexture) {
                EncodeRenderPass(commandBuffer, mtlRenderPass, globalEncoders, width, height);
                for (uint32_t i = 0; i < kMaxColorAttachments; ++i) {
                    if (trueResolveTextures[i] == nil) {
                        continue;
                    }

                    ASSERT(temporaryResolveTextures[i] != nil);
                    CopyIntoTrueResolveTarget(commandBuffer, trueResolveTextures[i],
                                              trueResolveLevels[i], trueResolveSlices[i],
                                              temporaryResolveTextures[i], width, height,
                                              globalEncoders);
                }
                return;
            }
        }

        // Handle Store + MSAA resolve workaround (Toggle EmulateStoreAndMSAAResolve).
        if (device->IsToggleEnabled(Toggle::EmulateStoreAndMSAAResolve)) {
            bool hasStoreAndMSAAResolve = false;

            // Remove any store + MSAA resolve and remember them.
            std::array<id<MTLTexture>, kMaxColorAttachments> resolveTextures = {};
            for (uint32_t i = 0; i < kMaxColorAttachments; ++i) {
                if (mtlRenderPass.colorAttachments[i].storeAction ==
                    MTLStoreActionStoreAndMultisampleResolve) {
                    hasStoreAndMSAAResolve = true;
                    resolveTextures[i] = mtlRenderPass.colorAttachments[i].resolveTexture;

                    mtlRenderPass.colorAttachments[i].storeAction = MTLStoreActionStore;
                    mtlRenderPass.colorAttachments[i].resolveTexture = nil;
                }
            }

            // If we found a store + MSAA resolve we need to resolve in a different render pass.
            if (hasStoreAndMSAAResolve) {
                EncodeRenderPass(commandBuffer, mtlRenderPass, globalEncoders, width, height);
                ResolveInAnotherRenderPass(commandBuffer, mtlRenderPass, resolveTextures);
                return;
            }
        }

        EncodeRenderPassInternal(commandBuffer, mtlRenderPass, width, height);
    }

    void CommandBuffer::EncodeRenderPassInternal(id<MTLCommandBuffer> commandBuffer,
                                                 MTLRenderPassDescriptor* mtlRenderPass,
                                                 uint32_t width,
                                                 uint32_t height) {
        RenderPipeline* lastPipeline = nullptr;
        id<MTLBuffer> indexBuffer = nil;
        uint32_t indexBufferBaseOffset = 0;

        // This will be autoreleased
        id<MTLRenderCommandEncoder> encoder =
            [commandBuffer renderCommandEncoderWithDescriptor:mtlRenderPass];

        Command type;
        while (mCommands.NextCommandId(&type)) {
            switch (type) {
                case Command::EndRenderPass: {
                    mCommands.NextCommand<EndRenderPassCmd>();
                    [encoder endEncoding];
                    return;
                } break;

                case Command::Draw: {
                    DrawCmd* draw = mCommands.NextCommand<DrawCmd>();

                    // The instance count must be non-zero, otherwise no-op
                    if (draw->instanceCount != 0) {
                        [encoder drawPrimitives:lastPipeline->GetMTLPrimitiveTopology()
                                    vertexStart:draw->firstVertex
                                    vertexCount:draw->vertexCount
                                  instanceCount:draw->instanceCount
                                   baseInstance:draw->firstInstance];
                    }
                } break;

                case Command::DrawIndexed: {
                    DrawIndexedCmd* draw = mCommands.NextCommand<DrawIndexedCmd>();
                    size_t formatSize =
                        IndexFormatSize(lastPipeline->GetVertexInputDescriptor()->indexFormat);

                    // The index and instance count must be non-zero, otherwise no-op
                    if (draw->indexCount != 0 && draw->instanceCount != 0) {
                        [encoder drawIndexedPrimitives:lastPipeline->GetMTLPrimitiveTopology()
                                            indexCount:draw->indexCount
                                             indexType:lastPipeline->GetMTLIndexType()
                                           indexBuffer:indexBuffer
                                     indexBufferOffset:indexBufferBaseOffset +
                                                       draw->firstIndex * formatSize
                                         instanceCount:draw->instanceCount
                                            baseVertex:draw->baseVertex
                                          baseInstance:draw->firstInstance];
                    }
                } break;

                case Command::DrawIndirect: {
                    DrawIndirectCmd* draw = mCommands.NextCommand<DrawIndirectCmd>();

                    Buffer* buffer = ToBackend(draw->indirectBuffer.Get());
                    id<MTLBuffer> indirectBuffer = buffer->GetMTLBuffer();
                    [encoder drawPrimitives:lastPipeline->GetMTLPrimitiveTopology()
                              indirectBuffer:indirectBuffer
                        indirectBufferOffset:draw->indirectOffset];
                } break;

                case Command::DrawIndexedIndirect: {
                    DrawIndirectCmd* draw = mCommands.NextCommand<DrawIndirectCmd>();

                    Buffer* buffer = ToBackend(draw->indirectBuffer.Get());
                    id<MTLBuffer> indirectBuffer = buffer->GetMTLBuffer();
                    [encoder drawIndexedPrimitives:lastPipeline->GetMTLPrimitiveTopology()
                                         indexType:lastPipeline->GetMTLIndexType()
                                       indexBuffer:indexBuffer
                                 indexBufferOffset:indexBufferBaseOffset
                                    indirectBuffer:indirectBuffer
                              indirectBufferOffset:draw->indirectOffset];
                } break;

                case Command::InsertDebugMarker: {
                    InsertDebugMarkerCmd* cmd = mCommands.NextCommand<InsertDebugMarkerCmd>();
                    auto label = mCommands.NextData<char>(cmd->length + 1);
                    NSString* mtlLabel = [[NSString alloc] initWithUTF8String:label];

                    [encoder insertDebugSignpost:mtlLabel];
                    [mtlLabel release];
                } break;

                case Command::PopDebugGroup: {
                    mCommands.NextCommand<PopDebugGroupCmd>();

                    [encoder popDebugGroup];
                } break;

                case Command::PushDebugGroup: {
                    PushDebugGroupCmd* cmd = mCommands.NextCommand<PushDebugGroupCmd>();
                    auto label = mCommands.NextData<char>(cmd->length + 1);
                    NSString* mtlLabel = [[NSString alloc] initWithUTF8String:label];

                    [encoder pushDebugGroup:mtlLabel];
                    [mtlLabel release];
                } break;

                case Command::SetRenderPipeline: {
                    SetRenderPipelineCmd* cmd = mCommands.NextCommand<SetRenderPipelineCmd>();
                    lastPipeline = ToBackend(cmd->pipeline).Get();

                    [encoder setDepthStencilState:lastPipeline->GetMTLDepthStencilState()];
                    [encoder setFrontFacingWinding:lastPipeline->GetMTLFrontFace()];
                    [encoder setCullMode:lastPipeline->GetMTLCullMode()];
                    lastPipeline->Encode(encoder);
                } break;

                case Command::SetStencilReference: {
                    SetStencilReferenceCmd* cmd = mCommands.NextCommand<SetStencilReferenceCmd>();
                    [encoder setStencilReferenceValue:cmd->reference];
                } break;

                case Command::SetViewport: {
                    SetViewportCmd* cmd = mCommands.NextCommand<SetViewportCmd>();
                    MTLViewport viewport;
                    viewport.originX = cmd->x;
                    viewport.originY = cmd->y;
                    viewport.width = cmd->width;
                    viewport.height = cmd->height;
                    viewport.znear = cmd->minDepth;
                    viewport.zfar = cmd->maxDepth;

                    [encoder setViewport:viewport];
                } break;

                case Command::SetScissorRect: {
                    SetScissorRectCmd* cmd = mCommands.NextCommand<SetScissorRectCmd>();
                    MTLScissorRect rect;
                    rect.x = cmd->x;
                    rect.y = cmd->y;
                    rect.width = cmd->width;
                    rect.height = cmd->height;

                    // The scissor rect x + width must be <= render pass width
                    if ((rect.x + rect.width) > width) {
                        rect.width = width - rect.x;
                    }
                    // The scissor rect y + height must be <= render pass height
                    if ((rect.y + rect.height > height)) {
                        rect.height = height - rect.y;
                    }

                    [encoder setScissorRect:rect];
                } break;

                case Command::SetBlendColor: {
                    SetBlendColorCmd* cmd = mCommands.NextCommand<SetBlendColorCmd>();
                    [encoder setBlendColorRed:cmd->color.r
                                        green:cmd->color.g
                                         blue:cmd->color.b
                                        alpha:cmd->color.a];
                } break;

                case Command::SetBindGroup: {
                    SetBindGroupCmd* cmd = mCommands.NextCommand<SetBindGroupCmd>();
                    uint64_t* dynamicOffsets = nullptr;
                    if (cmd->dynamicOffsetCount > 0) {
                        dynamicOffsets = mCommands.NextData<uint64_t>(cmd->dynamicOffsetCount);
                    }

                    ApplyBindGroup(cmd->index, ToBackend(cmd->group.Get()), cmd->dynamicOffsetCount,
                                   dynamicOffsets, ToBackend(lastPipeline->GetLayout()), encoder,
                                   nil);
                } break;

                case Command::SetIndexBuffer: {
                    SetIndexBufferCmd* cmd = mCommands.NextCommand<SetIndexBufferCmd>();
                    auto b = ToBackend(cmd->buffer.Get());
                    indexBuffer = b->GetMTLBuffer();
                    indexBufferBaseOffset = cmd->offset;
                } break;

                case Command::SetVertexBuffers: {
                    SetVertexBuffersCmd* cmd = mCommands.NextCommand<SetVertexBuffersCmd>();
                    auto buffers = mCommands.NextData<Ref<BufferBase>>(cmd->count);
                    auto offsets = mCommands.NextData<uint64_t>(cmd->count);

                    std::array<id<MTLBuffer>, kMaxVertexBuffers> mtlBuffers;
                    std::array<NSUInteger, kMaxVertexBuffers> mtlOffsets;

                    // Perhaps an "array of vertex buffers(+offsets?)" should be
                    // a Dawn API primitive to avoid reconstructing this array?
                    for (uint32_t i = 0; i < cmd->count; ++i) {
                        Buffer* buffer = ToBackend(buffers[i].Get());
                        mtlBuffers[i] = buffer->GetMTLBuffer();
                        mtlOffsets[i] = offsets[i];
                    }

                    [encoder setVertexBuffers:mtlBuffers.data()
                                      offsets:mtlOffsets.data()
                                    withRange:NSMakeRange(kMaxBindingsPerGroup + cmd->startSlot,
                                                          cmd->count)];
                } break;

                default: { UNREACHABLE(); } break;
            }
        }

        // EndRenderPass should have been called
        UNREACHABLE();
    }

}}  // namespace dawn_native::metal
