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
#include "dawn_native/metal/InputStateMTL.h"
#include "dawn_native/metal/PipelineLayoutMTL.h"
#include "dawn_native/metal/RenderPipelineMTL.h"
#include "dawn_native/metal/SamplerMTL.h"
#include "dawn_native/metal/TextureMTL.h"

namespace dawn_native { namespace metal {

    namespace {

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

                descriptor.colorAttachments[i].storeAction = MTLStoreActionStore;
            }

            if (renderPass->hasDepthStencilAttachment) {
                auto& attachmentInfo = renderPass->depthStencilAttachment;

                // TODO(jiawei.shao@intel.com): support rendering into a layer of a texture.
                id<MTLTexture> texture =
                    ToBackend(attachmentInfo.view->GetTexture())->GetMTLTexture();
                dawn::TextureFormat format = attachmentInfo.view->GetTexture()->GetFormat();

                if (TextureFormatHasDepth(format)) {
                    descriptor.depthAttachment.texture = texture;
                    descriptor.depthAttachment.storeAction = MTLStoreActionStore;

                    if (attachmentInfo.depthLoadOp == dawn::LoadOp::Clear) {
                        descriptor.depthAttachment.loadAction = MTLLoadActionClear;
                        descriptor.depthAttachment.clearDepth = attachmentInfo.clearDepth;
                    } else {
                        descriptor.depthAttachment.loadAction = MTLLoadActionLoad;
                    }
                }

                if (TextureFormatHasStencil(format)) {
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

        // Handles a call to SetBindGroup, directing the commands to the correct encoder.
        // There is a single function that takes both encoders to factor code. Other approaches like
        // templates wouldn't work because the name of methods are different between the two encoder
        // types.
        void ApplyBindGroup(uint32_t index,
                            BindGroup* group,
                            PipelineLayout* pipelineLayout,
                            id<MTLRenderCommandEncoder> render,
                            id<MTLComputeCommandEncoder> compute) {
            const auto& layout = group->GetLayout()->GetBindingInfo();

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
                        dawn::ShaderStage::Vertex)[index][bindingIndex];
                }
                if (hasFragStage) {
                    fragIndex = pipelineLayout->GetBindingIndexInfo(
                        dawn::ShaderStage::Fragment)[index][bindingIndex];
                }
                if (hasComputeStage) {
                    computeIndex = pipelineLayout->GetBindingIndexInfo(
                        dawn::ShaderStage::Compute)[index][bindingIndex];
                }

                switch (layout.types[bindingIndex]) {
                    case dawn::BindingType::UniformBuffer:
                    case dawn::BindingType::StorageBuffer: {
                        BufferBinding binding = group->GetBindingAsBufferBinding(bindingIndex);
                        const id<MTLBuffer> buffer = ToBackend(binding.buffer)->GetMTLBuffer();
                        const NSUInteger offset = binding.offset;

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
                }
            }
        }

    }  // anonymous namespace

    CommandBuffer::CommandBuffer(Device* device, CommandEncoderBase* encoder)
        : CommandBufferBase(device, encoder), mCommands(encoder->AcquireCommands()) {
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
                    EncodeRenderPass(commandBuffer, cmd);
                } break;

                case Command::CopyBufferToBuffer: {
                    CopyBufferToBufferCmd* copy = mCommands.NextCommand<CopyBufferToBufferCmd>();
                    auto& src = copy->source;
                    auto& dst = copy->destination;

                    encoders.EnsureBlit(commandBuffer);
                    [encoders.blit copyFromBuffer:ToBackend(src.buffer)->GetMTLBuffer()
                                     sourceOffset:src.offset
                                         toBuffer:ToBackend(dst.buffer)->GetMTLBuffer()
                                destinationOffset:dst.offset
                                             size:copy->size];
                } break;

                case Command::CopyBufferToTexture: {
                    CopyBufferToTextureCmd* copy = mCommands.NextCommand<CopyBufferToTextureCmd>();
                    auto& src = copy->source;
                    auto& dst = copy->destination;
                    auto& copySize = copy->copySize;
                    Buffer* buffer = ToBackend(src.buffer.Get());
                    Texture* texture = ToBackend(dst.texture.Get());

                    MTLOrigin origin;
                    origin.x = dst.origin.x;
                    origin.y = dst.origin.y;
                    origin.z = dst.origin.z;

                    MTLSize size;
                    size.width = copySize.width;
                    size.height = copySize.height;
                    size.depth = copySize.depth;

                    // When uploading textures from an unpacked buffer, Metal validation layer
                    // doesn't compute the correct range when checking if the buffer is big enough
                    // to contain the data for the whole copy. Instead of looking at the position
                    // of the last texel in the buffer, it computes the volume of the 3D box with
                    // rowPitch * imageHeight * copySize.depth. For example considering the pixel
                    // buffer below where in memory, each row data (D) of the texture is followed
                    // by some padding data (P):
                    //     |DDDDDDD|PP|
                    //     |DDDDDDD|PP|
                    //     |DDDDDDD|PP|
                    //     |DDDDDDD|PP|
                    //     |DDDDDDA|PP|
                    // The last pixel read will be A, but the driver will think it is the whole
                    // last padding row, causing it to generate an error when the pixel buffer is
                    // just big enough.

                    // We work around this limitation by detecting when Metal would complain and
                    // copy the last image and row separately using tight sourceBytesPerRow or
                    // sourceBytesPerImage.
                    uint32_t bytesPerImage = src.rowPitch * src.imageHeight;

                    // Check whether buffer size is big enough.
                    bool needWorkaround =
                        (buffer->GetSize() - src.offset < bytesPerImage * size.depth);

                    encoders.EnsureBlit(commandBuffer);

                    if (!needWorkaround) {
                        [encoders.blit copyFromBuffer:buffer->GetMTLBuffer()
                                         sourceOffset:src.offset
                                    sourceBytesPerRow:src.rowPitch
                                  sourceBytesPerImage:(src.rowPitch * src.imageHeight)
                                           sourceSize:size
                                            toTexture:texture->GetMTLTexture()
                                     destinationSlice:dst.slice
                                     destinationLevel:dst.level
                                    destinationOrigin:origin];
                        break;
                    }

                    uint32_t offset = src.offset;

                    // Doing all the copy except the last image.
                    if (size.depth > 1) {
                        [encoders.blit
                                 copyFromBuffer:buffer->GetMTLBuffer()
                                   sourceOffset:offset
                              sourceBytesPerRow:src.rowPitch
                            sourceBytesPerImage:(src.rowPitch * src.imageHeight)
                                     sourceSize:MTLSizeMake(size.width, size.height, size.depth - 1)
                                      toTexture:texture->GetMTLTexture()
                               destinationSlice:dst.slice
                               destinationLevel:dst.level
                              destinationOrigin:origin];

                        // Update offset to copy to the last image.
                        offset += (copySize.depth - 1) * bytesPerImage;
                    }

                    // Doing all the copy in last image except the last row.
                    if (size.height > 1) {
                        [encoders.blit copyFromBuffer:buffer->GetMTLBuffer()
                                         sourceOffset:offset
                                    sourceBytesPerRow:src.rowPitch
                                  sourceBytesPerImage:(src.rowPitch * (src.imageHeight - 1))
                                           sourceSize:MTLSizeMake(size.width, size.height - 1, 1)
                                            toTexture:texture->GetMTLTexture()
                                     destinationSlice:dst.slice
                                     destinationLevel:dst.level
                                    destinationOrigin:MTLOriginMake(origin.x, origin.y,
                                                                    origin.z + size.depth - 1)];

                        // Update offset to copy to the last row.
                        offset += (copySize.height - 1) * src.rowPitch;
                    }

                    // Doing the last row copy with the exact number of bytes in last row.
                    // Like copy to a 1D texture to workaround the issue.
                    uint32_t lastRowDataSize =
                        copySize.width * TextureFormatPixelSize(texture->GetFormat());

                    [encoders.blit
                             copyFromBuffer:buffer->GetMTLBuffer()
                               sourceOffset:offset
                          sourceBytesPerRow:lastRowDataSize
                        sourceBytesPerImage:lastRowDataSize
                                 sourceSize:MTLSizeMake(size.width, 1, 1)
                                  toTexture:texture->GetMTLTexture()
                           destinationSlice:dst.slice
                           destinationLevel:dst.level
                          destinationOrigin:MTLOriginMake(origin.x, origin.y + size.height - 1,
                                                          origin.z + size.depth - 1)];
                } break;

                case Command::CopyTextureToBuffer: {
                    CopyTextureToBufferCmd* copy = mCommands.NextCommand<CopyTextureToBufferCmd>();
                    auto& src = copy->source;
                    auto& dst = copy->destination;
                    auto& copySize = copy->copySize;
                    Texture* texture = ToBackend(src.texture.Get());
                    Buffer* buffer = ToBackend(dst.buffer.Get());

                    MTLOrigin origin;
                    origin.x = src.origin.x;
                    origin.y = src.origin.y;
                    origin.z = src.origin.z;

                    MTLSize size;
                    size.width = copySize.width;
                    size.height = copySize.height;
                    size.depth = copySize.depth;

                    // When Copy textures to an unpacked buffer, Metal validation layer doesn't
                    // compute the correct range when checking if the buffer is big enough to
                    // contain the data for the whole copy. Instead of looking at the position
                    // of the last texel in the buffer, it computes the volume of the 3D box with
                    // rowPitch * imageHeight * copySize.depth.
                    // For example considering the texture below where in memory, each row
                    // data (D) of the texture is followed by some padding data (P):
                    //     |DDDDDDD|PP|
                    //     |DDDDDDD|PP|
                    //     |DDDDDDD|PP|
                    //     |DDDDDDD|PP|
                    //     |DDDDDDA|PP|
                    // The last valid pixel read will be A, but the driver will think it needs the
                    // whole last padding row, causing it to generate an error when the buffer is
                    // just big enough.

                    // We work around this limitation by detecting when Metal would complain and
                    // copy the last image and row separately using tight destinationBytesPerRow or
                    // destinationBytesPerImage.
                    uint32_t bytesPerImage = dst.rowPitch * dst.imageHeight;

                    // Check whether buffer size is big enough.
                    bool needWorkaround =
                        (buffer->GetSize() - dst.offset < bytesPerImage * size.depth);

                    encoders.EnsureBlit(commandBuffer);

                    if (!needWorkaround) {
                        [encoders.blit copyFromTexture:texture->GetMTLTexture()
                                           sourceSlice:src.slice
                                           sourceLevel:src.level
                                          sourceOrigin:origin
                                            sourceSize:size
                                              toBuffer:buffer->GetMTLBuffer()
                                     destinationOffset:dst.offset
                                destinationBytesPerRow:dst.rowPitch
                              destinationBytesPerImage:(dst.rowPitch * dst.imageHeight)];
                        break;
                    }

                    uint32_t offset = dst.offset;

                    // Doing all the copy except the last image.
                    if (size.depth > 1) {
                        size.depth = copySize.depth - 1;

                        [encoders.blit copyFromTexture:texture->GetMTLTexture()
                                           sourceSlice:src.slice
                                           sourceLevel:src.level
                                          sourceOrigin:origin
                                            sourceSize:MTLSizeMake(size.width, size.height,
                                                                   size.depth - 1)
                                              toBuffer:buffer->GetMTLBuffer()
                                     destinationOffset:offset
                                destinationBytesPerRow:dst.rowPitch
                              destinationBytesPerImage:dst.rowPitch * dst.imageHeight];

                        // Update offset to copy from the last image.
                        offset += (copySize.depth - 1) * bytesPerImage;
                    }

                    // Doing all the copy in last image except the last row.
                    if (size.height > 1) {
                        [encoders.blit copyFromTexture:texture->GetMTLTexture()
                                           sourceSlice:src.slice
                                           sourceLevel:src.level
                                          sourceOrigin:MTLOriginMake(origin.x, origin.y,
                                                                     origin.z + size.depth - 1)
                                            sourceSize:MTLSizeMake(size.width, size.height - 1, 1)
                                              toBuffer:buffer->GetMTLBuffer()
                                     destinationOffset:offset
                                destinationBytesPerRow:dst.rowPitch
                              destinationBytesPerImage:dst.rowPitch * (dst.imageHeight - 1)];

                        // Update offset to copy from the last row.
                        offset += (copySize.height - 1) * dst.rowPitch;
                    }

                    // Doing the last row copy with the exact number of bytes in last row.
                    // Like copy from a 1D texture to workaround the issue.
                    uint32_t lastRowDataSize =
                        copySize.width * TextureFormatPixelSize(texture->GetFormat());

                    [encoders.blit
                                 copyFromTexture:texture->GetMTLTexture()
                                     sourceSlice:src.slice
                                     sourceLevel:src.level
                                    sourceOrigin:MTLOriginMake(origin.x, origin.y + size.height - 1,
                                                               origin.z + size.depth - 1)
                                      sourceSize:MTLSizeMake(size.width, 1, 1)
                                        toBuffer:buffer->GetMTLBuffer()
                               destinationOffset:offset
                          destinationBytesPerRow:lastRowDataSize
                        destinationBytesPerImage:lastRowDataSize];
                } break;

                default: { UNREACHABLE(); } break;
            }
        }

        encoders.Finish();
    }

    void CommandBuffer::EncodeComputePass(id<MTLCommandBuffer> commandBuffer) {
        ComputePipeline* lastPipeline = nullptr;
        std::array<uint32_t, kMaxPushConstants> pushConstants;

        // Will be autoreleased
        id<MTLComputeCommandEncoder> encoder = [commandBuffer computeCommandEncoder];

        // Set default values for push constants
        pushConstants.fill(0);
        [encoder setBytes:&pushConstants length:sizeof(uint32_t) * kMaxPushConstants atIndex:0];

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

                case Command::SetComputePipeline: {
                    SetComputePipelineCmd* cmd = mCommands.NextCommand<SetComputePipelineCmd>();
                    lastPipeline = ToBackend(cmd->pipeline).Get();

                    lastPipeline->Encode(encoder);
                } break;

                case Command::SetPushConstants: {
                    SetPushConstantsCmd* cmd = mCommands.NextCommand<SetPushConstantsCmd>();
                    uint32_t* values = mCommands.NextData<uint32_t>(cmd->count);

                    if (cmd->stages & dawn::ShaderStageBit::Compute) {
                        memcpy(&pushConstants[cmd->offset], values, cmd->count * sizeof(uint32_t));

                        [encoder setBytes:&pushConstants
                                   length:sizeof(uint32_t) * kMaxPushConstants
                                  atIndex:0];
                    }
                } break;

                case Command::SetBindGroup: {
                    SetBindGroupCmd* cmd = mCommands.NextCommand<SetBindGroupCmd>();
                    ApplyBindGroup(cmd->index, ToBackend(cmd->group.Get()),
                                   ToBackend(lastPipeline->GetLayout()), nil, encoder);
                } break;

                default: { UNREACHABLE(); } break;
            }
        }

        // EndComputePass should have been called
        UNREACHABLE();
    }

    void CommandBuffer::EncodeRenderPass(id<MTLCommandBuffer> commandBuffer,
                                         BeginRenderPassCmd* renderPassCmd) {
        RenderPipeline* lastPipeline = nullptr;
        id<MTLBuffer> indexBuffer = nil;
        uint32_t indexBufferBaseOffset = 0;

        std::array<uint32_t, kMaxPushConstants> vertexPushConstants;
        std::array<uint32_t, kMaxPushConstants> fragmentPushConstants;

        // This will be autoreleased
        id<MTLRenderCommandEncoder> encoder = [commandBuffer
            renderCommandEncoderWithDescriptor:CreateMTLRenderPassDescriptor(renderPassCmd)];

        // Set default values for push constants
        vertexPushConstants.fill(0);
        fragmentPushConstants.fill(0);

        [encoder setVertexBytes:&vertexPushConstants
                         length:sizeof(uint32_t) * kMaxPushConstants
                        atIndex:0];
        [encoder setFragmentBytes:&fragmentPushConstants
                           length:sizeof(uint32_t) * kMaxPushConstants
                          atIndex:0];

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
                    size_t formatSize = IndexFormatSize(lastPipeline->GetIndexFormat());

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
                    lastPipeline->Encode(encoder);
                } break;

                case Command::SetPushConstants: {
                    SetPushConstantsCmd* cmd = mCommands.NextCommand<SetPushConstantsCmd>();
                    uint32_t* values = mCommands.NextData<uint32_t>(cmd->count);

                    if (cmd->stages & dawn::ShaderStageBit::Vertex) {
                        memcpy(&vertexPushConstants[cmd->offset], values,
                               cmd->count * sizeof(uint32_t));
                        [encoder setVertexBytes:&vertexPushConstants
                                         length:sizeof(uint32_t) * kMaxPushConstants
                                        atIndex:0];
                    }

                    if (cmd->stages & dawn::ShaderStageBit::Fragment) {
                        memcpy(&fragmentPushConstants[cmd->offset], values,
                               cmd->count * sizeof(uint32_t));
                        [encoder setFragmentBytes:&fragmentPushConstants
                                           length:sizeof(uint32_t) * kMaxPushConstants
                                          atIndex:0];
                    }
                } break;

                case Command::SetStencilReference: {
                    SetStencilReferenceCmd* cmd = mCommands.NextCommand<SetStencilReferenceCmd>();
                    [encoder setStencilReferenceValue:cmd->reference];
                } break;

                case Command::SetScissorRect: {
                    SetScissorRectCmd* cmd = mCommands.NextCommand<SetScissorRectCmd>();
                    MTLScissorRect rect;
                    rect.x = cmd->x;
                    rect.y = cmd->y;
                    rect.width = cmd->width;
                    rect.height = cmd->height;

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
                    ApplyBindGroup(cmd->index, ToBackend(cmd->group.Get()),
                                   ToBackend(lastPipeline->GetLayout()), encoder, nil);
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
                    auto offsets = mCommands.NextData<uint32_t>(cmd->count);

                    std::array<id<MTLBuffer>, kMaxVertexInputs> mtlBuffers;
                    std::array<NSUInteger, kMaxVertexInputs> mtlOffsets;

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
