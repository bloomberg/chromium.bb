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

#include "dawn_native/CommandEncoder.h"

#include "common/BitSetIterator.h"
#include "common/Math.h"
#include "dawn_native/BindGroup.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandBuffer.h"
#include "dawn_native/CommandBufferStateTracker.h"
#include "dawn_native/CommandValidation.h"
#include "dawn_native/Commands.h"
#include "dawn_native/ComputePassEncoder.h"
#include "dawn_native/Device.h"
#include "dawn_native/ErrorData.h"
#include "dawn_native/ObjectType_autogen.h"
#include "dawn_native/QueryHelper.h"
#include "dawn_native/QuerySet.h"
#include "dawn_native/Queue.h"
#include "dawn_native/RenderPassEncoder.h"
#include "dawn_native/RenderPipeline.h"
#include "dawn_native/ValidationUtils_autogen.h"
#include "dawn_platform/DawnPlatform.h"
#include "dawn_platform/tracing/TraceEvent.h"

#include <cmath>
#include <map>

namespace dawn_native {

    namespace {

        MaybeError ValidateB2BCopyAlignment(uint64_t dataSize,
                                            uint64_t srcOffset,
                                            uint64_t dstOffset) {
            // Copy size must be a multiple of 4 bytes on macOS.
            DAWN_INVALID_IF(dataSize % 4 != 0, "Copy size (%u) is not a multiple of 4.", dataSize);

            // SourceOffset and destinationOffset must be multiples of 4 bytes on macOS.
            DAWN_INVALID_IF(
                srcOffset % 4 != 0 || dstOffset % 4 != 0,
                "Source offset (%u) or destination offset (%u) is not a multiple of 4 bytes,",
                srcOffset, dstOffset);

            return {};
        }

        MaybeError ValidateTextureSampleCountInBufferCopyCommands(const TextureBase* texture) {
            DAWN_INVALID_IF(texture->GetSampleCount() > 1,
                            "%s sample count (%u) is not 1 when copying to or from a buffer.",
                            texture, texture->GetSampleCount());

            return {};
        }

        MaybeError ValidateLinearTextureCopyOffset(const TextureDataLayout& layout,
                                                   const TexelBlockInfo& blockInfo,
                                                   const bool hasDepthOrStencil) {
            if (hasDepthOrStencil) {
                // For depth-stencil texture, buffer offset must be a multiple of 4.
                DAWN_INVALID_IF(layout.offset % 4 != 0,
                                "Offset (%u) is not a multiple of 4 for depth/stencil texture.",
                                layout.offset);
            } else {
                DAWN_INVALID_IF(layout.offset % blockInfo.byteSize != 0,
                                "Offset (%u) is not a multiple of the texel block byte size (%u).",
                                layout.offset, blockInfo.byteSize);
            }
            return {};
        }

        MaybeError ValidateTextureDepthStencilToBufferCopyRestrictions(
            const ImageCopyTexture& src) {
            Aspect aspectUsed;
            DAWN_TRY_ASSIGN(aspectUsed, SingleAspectUsedByImageCopyTexture(src));
            if (aspectUsed == Aspect::Depth) {
                switch (src.texture->GetFormat().format) {
                    case wgpu::TextureFormat::Depth24Plus:
                    case wgpu::TextureFormat::Depth24PlusStencil8:
                        return DAWN_FORMAT_VALIDATION_ERROR(
                            "The depth aspect of %s format %s cannot be selected in a texture to "
                            "buffer copy.",
                            src.texture, src.texture->GetFormat().format);
                    case wgpu::TextureFormat::Depth32Float:
                        break;

                    default:
                        UNREACHABLE();
                }
            }

            return {};
        }

        MaybeError ValidateAttachmentArrayLayersAndLevelCount(const TextureViewBase* attachment) {
            // Currently we do not support layered rendering.
            DAWN_INVALID_IF(attachment->GetLayerCount() > 1,
                            "The layer count (%u) of %s used as attachment is greater than 1.",
                            attachment->GetLayerCount(), attachment);

            DAWN_INVALID_IF(attachment->GetLevelCount() > 1,
                            "The mip level count (%u) of %s used as attachment is greater than 1.",
                            attachment->GetLevelCount(), attachment);

            return {};
        }

        MaybeError ValidateOrSetAttachmentSize(const TextureViewBase* attachment,
                                               uint32_t* width,
                                               uint32_t* height) {
            const Extent3D& attachmentSize =
                attachment->GetTexture()->GetMipLevelVirtualSize(attachment->GetBaseMipLevel());

            if (*width == 0) {
                DAWN_ASSERT(*height == 0);
                *width = attachmentSize.width;
                *height = attachmentSize.height;
                DAWN_ASSERT(*width != 0 && *height != 0);
            } else {
                DAWN_INVALID_IF(
                    *width != attachmentSize.width || *height != attachmentSize.height,
                    "Attachment %s size (width: %u, height: %u) does not match the size of the "
                    "other attachments (width: %u, height: %u).",
                    attachment, attachmentSize.width, attachmentSize.height, *width, *height);
            }

            return {};
        }

        MaybeError ValidateOrSetColorAttachmentSampleCount(const TextureViewBase* colorAttachment,
                                                           uint32_t* sampleCount) {
            if (*sampleCount == 0) {
                *sampleCount = colorAttachment->GetTexture()->GetSampleCount();
                DAWN_ASSERT(*sampleCount != 0);
            } else {
                DAWN_INVALID_IF(
                    *sampleCount != colorAttachment->GetTexture()->GetSampleCount(),
                    "Color attachment %s sample count (%u) does not match the sample count of the "
                    "other attachments (%u).",
                    colorAttachment, colorAttachment->GetTexture()->GetSampleCount(), *sampleCount);
            }

            return {};
        }

        MaybeError ValidateResolveTarget(const DeviceBase* device,
                                         const RenderPassColorAttachment& colorAttachment) {
            if (colorAttachment.resolveTarget == nullptr) {
                return {};
            }

            const TextureViewBase* resolveTarget = colorAttachment.resolveTarget;
            const TextureViewBase* attachment = colorAttachment.view;
            DAWN_TRY(device->ValidateObject(colorAttachment.resolveTarget));
            DAWN_TRY(ValidateCanUseAs(colorAttachment.resolveTarget->GetTexture(),
                                      wgpu::TextureUsage::RenderAttachment));

            DAWN_INVALID_IF(
                !attachment->GetTexture()->IsMultisampledTexture(),
                "Cannot set %s as a resolve target when the color attachment %s has a sample "
                "count of 1.",
                resolveTarget, attachment);

            DAWN_INVALID_IF(resolveTarget->GetTexture()->IsMultisampledTexture(),
                            "Cannot use %s as resolve target. Sample count (%u) is greater than 1.",
                            resolveTarget, resolveTarget->GetTexture()->GetSampleCount());

            DAWN_INVALID_IF(resolveTarget->GetLayerCount() > 1,
                            "The resolve target %s array layer count (%u) is not 1.", resolveTarget,
                            resolveTarget->GetLayerCount());

            DAWN_INVALID_IF(resolveTarget->GetLevelCount() > 1,
                            "The resolve target %s mip level count (%u) is not 1.", resolveTarget,
                            resolveTarget->GetLevelCount());

            const Extent3D& colorTextureSize =
                attachment->GetTexture()->GetMipLevelVirtualSize(attachment->GetBaseMipLevel());
            const Extent3D& resolveTextureSize =
                resolveTarget->GetTexture()->GetMipLevelVirtualSize(
                    resolveTarget->GetBaseMipLevel());
            DAWN_INVALID_IF(
                colorTextureSize.width != resolveTextureSize.width ||
                    colorTextureSize.height != resolveTextureSize.height,
                "The Resolve target %s size (width: %u, height: %u) does not match the color "
                "attachment %s size (width: %u, height: %u).",
                resolveTarget, resolveTextureSize.width, resolveTextureSize.height, attachment,
                colorTextureSize.width, colorTextureSize.height);

            wgpu::TextureFormat resolveTargetFormat = resolveTarget->GetFormat().format;
            DAWN_INVALID_IF(
                resolveTargetFormat != attachment->GetFormat().format,
                "The resolve target %s format (%s) does not match the color attachment %s format "
                "(%s).",
                resolveTarget, resolveTargetFormat, attachment, attachment->GetFormat().format);

            return {};
        }

        MaybeError ValidateRenderPassColorAttachment(
            DeviceBase* device,
            const RenderPassColorAttachment& colorAttachment,
            uint32_t* width,
            uint32_t* height,
            uint32_t* sampleCount) {
            TextureViewBase* attachment = colorAttachment.view;
            DAWN_TRY(device->ValidateObject(attachment));
            DAWN_TRY(
                ValidateCanUseAs(attachment->GetTexture(), wgpu::TextureUsage::RenderAttachment));

            DAWN_INVALID_IF(!(attachment->GetAspects() & Aspect::Color) ||
                                !attachment->GetFormat().isRenderable,
                            "The color attachment %s format (%s) is not color renderable.",
                            attachment, attachment->GetFormat().format);

            DAWN_TRY(ValidateLoadOp(colorAttachment.loadOp));
            DAWN_TRY(ValidateStoreOp(colorAttachment.storeOp));

            if (colorAttachment.loadOp == wgpu::LoadOp::Clear) {
                DAWN_INVALID_IF(std::isnan(colorAttachment.clearColor.r) ||
                                    std::isnan(colorAttachment.clearColor.g) ||
                                    std::isnan(colorAttachment.clearColor.b) ||
                                    std::isnan(colorAttachment.clearColor.a),
                                "Color clear value (%s) contain a NaN.",
                                &colorAttachment.clearColor);
            }

            DAWN_TRY(ValidateOrSetColorAttachmentSampleCount(attachment, sampleCount));

            DAWN_TRY(ValidateResolveTarget(device, colorAttachment));

            DAWN_TRY(ValidateAttachmentArrayLayersAndLevelCount(attachment));
            DAWN_TRY(ValidateOrSetAttachmentSize(attachment, width, height));

            return {};
        }

        MaybeError ValidateRenderPassDepthStencilAttachment(
            DeviceBase* device,
            const RenderPassDepthStencilAttachment* depthStencilAttachment,
            uint32_t* width,
            uint32_t* height,
            uint32_t* sampleCount) {
            DAWN_ASSERT(depthStencilAttachment != nullptr);

            TextureViewBase* attachment = depthStencilAttachment->view;
            DAWN_TRY(device->ValidateObject(attachment));
            DAWN_TRY(
                ValidateCanUseAs(attachment->GetTexture(), wgpu::TextureUsage::RenderAttachment));

            const Format& format = attachment->GetFormat();
            DAWN_INVALID_IF(
                !format.HasDepthOrStencil(),
                "The depth stencil attachment %s format (%s) is not a depth stencil format.",
                attachment, format.format);

            DAWN_INVALID_IF(!format.isRenderable,
                            "The depth stencil attachment %s format (%s) is not renderable.",
                            attachment, format.format);

            DAWN_INVALID_IF(attachment->GetAspects() != format.aspects,
                            "The depth stencil attachment %s must encompass all aspects.",
                            attachment);

            DAWN_TRY(ValidateLoadOp(depthStencilAttachment->depthLoadOp));
            DAWN_TRY(ValidateLoadOp(depthStencilAttachment->stencilLoadOp));
            DAWN_TRY(ValidateStoreOp(depthStencilAttachment->depthStoreOp));
            DAWN_TRY(ValidateStoreOp(depthStencilAttachment->stencilStoreOp));

            DAWN_INVALID_IF(
                attachment->GetAspects() == (Aspect::Depth | Aspect::Stencil) &&
                    depthStencilAttachment->depthReadOnly !=
                        depthStencilAttachment->stencilReadOnly,
                "depthReadOnly (%u) and stencilReadOnly (%u) must be the same when texture aspect "
                "is 'all'.",
                depthStencilAttachment->depthReadOnly, depthStencilAttachment->stencilReadOnly);

            DAWN_INVALID_IF(
                depthStencilAttachment->depthReadOnly &&
                    (depthStencilAttachment->depthLoadOp != wgpu::LoadOp::Load ||
                     depthStencilAttachment->depthStoreOp != wgpu::StoreOp::Store),
                "depthLoadOp (%s) is not %s or depthStoreOp (%s) is not %s when depthReadOnly "
                "is true.",
                depthStencilAttachment->depthLoadOp, wgpu::LoadOp::Load,
                depthStencilAttachment->depthStoreOp, wgpu::StoreOp::Store);

            DAWN_INVALID_IF(depthStencilAttachment->stencilReadOnly &&
                                (depthStencilAttachment->stencilLoadOp != wgpu::LoadOp::Load ||
                                 depthStencilAttachment->stencilStoreOp != wgpu::StoreOp::Store),
                            "stencilLoadOp (%s) is not %s or stencilStoreOp (%s) is not %s when "
                            "stencilReadOnly is true.",
                            depthStencilAttachment->stencilLoadOp, wgpu::LoadOp::Load,
                            depthStencilAttachment->stencilStoreOp, wgpu::StoreOp::Store);

            DAWN_INVALID_IF(depthStencilAttachment->depthLoadOp == wgpu::LoadOp::Clear &&
                                std::isnan(depthStencilAttachment->clearDepth),
                            "Depth clear value is NaN.");

            // *sampleCount == 0 must only happen when there is no color attachment. In that case we
            // do not need to validate the sample count of the depth stencil attachment.
            const uint32_t depthStencilSampleCount = attachment->GetTexture()->GetSampleCount();
            if (*sampleCount != 0) {
                DAWN_INVALID_IF(
                    depthStencilSampleCount != *sampleCount,
                    "The depth stencil attachment %s sample count (%u) does not match the sample "
                    "count of the other attachments (%u).",
                    attachment, depthStencilSampleCount, *sampleCount);
            } else {
                *sampleCount = depthStencilSampleCount;
            }

            DAWN_TRY(ValidateAttachmentArrayLayersAndLevelCount(attachment));
            DAWN_TRY(ValidateOrSetAttachmentSize(attachment, width, height));

            return {};
        }

        MaybeError ValidateRenderPassDescriptor(DeviceBase* device,
                                                const RenderPassDescriptor* descriptor,
                                                uint32_t* width,
                                                uint32_t* height,
                                                uint32_t* sampleCount) {
            DAWN_INVALID_IF(
                descriptor->colorAttachmentCount > kMaxColorAttachments,
                "Color attachment count (%u) exceeds the maximum number of color attachments (%u).",
                descriptor->colorAttachmentCount, kMaxColorAttachments);

            for (uint32_t i = 0; i < descriptor->colorAttachmentCount; ++i) {
                DAWN_TRY_CONTEXT(
                    ValidateRenderPassColorAttachment(device, descriptor->colorAttachments[i],
                                                      width, height, sampleCount),
                    "validating colorAttachments[%u].", i);
            }

            if (descriptor->depthStencilAttachment != nullptr) {
                DAWN_TRY_CONTEXT(
                    ValidateRenderPassDepthStencilAttachment(
                        device, descriptor->depthStencilAttachment, width, height, sampleCount),
                    "validating depthStencilAttachment.");
            }

            if (descriptor->occlusionQuerySet != nullptr) {
                DAWN_TRY(device->ValidateObject(descriptor->occlusionQuerySet));

                DAWN_INVALID_IF(
                    descriptor->occlusionQuerySet->GetQueryType() != wgpu::QueryType::Occlusion,
                    "The occlusionQuerySet %s type (%s) is not %s.", descriptor->occlusionQuerySet,
                    descriptor->occlusionQuerySet->GetQueryType(), wgpu::QueryType::Occlusion);
            }

            DAWN_INVALID_IF(descriptor->colorAttachmentCount == 0 &&
                                descriptor->depthStencilAttachment == nullptr,
                            "Render pass has no attachments.");

            return {};
        }

        MaybeError ValidateComputePassDescriptor(const DeviceBase* device,
                                                 const ComputePassDescriptor* descriptor) {
            return {};
        }

        MaybeError ValidateQuerySetResolve(const QuerySetBase* querySet,
                                           uint32_t firstQuery,
                                           uint32_t queryCount,
                                           const BufferBase* destination,
                                           uint64_t destinationOffset) {
            DAWN_INVALID_IF(firstQuery >= querySet->GetQueryCount(),
                            "First query (%u) exceeds the number of queries (%u) in %s.",
                            firstQuery, querySet->GetQueryCount(), querySet);

            DAWN_INVALID_IF(
                queryCount > querySet->GetQueryCount() - firstQuery,
                "The query range (firstQuery: %u, queryCount: %u) exceeds the number of queries "
                "(%u) in %s.",
                firstQuery, queryCount, querySet->GetQueryCount(), querySet);

            DAWN_INVALID_IF(destinationOffset % 256 != 0,
                            "The destination buffer %s offset (%u) is not a multiple of 256.",
                            destination, destinationOffset);

            uint64_t bufferSize = destination->GetSize();
            // The destination buffer must have enough storage, from destination offset, to contain
            // the result of resolved queries
            bool fitsInBuffer = destinationOffset <= bufferSize &&
                                (static_cast<uint64_t>(queryCount) * sizeof(uint64_t) <=
                                 (bufferSize - destinationOffset));
            DAWN_INVALID_IF(
                !fitsInBuffer,
                "The resolved %s data size (%u) would not fit in %s with size %u at the offset %u.",
                querySet, static_cast<uint64_t>(queryCount) * sizeof(uint64_t), destination,
                bufferSize, destinationOffset);

            return {};
        }

        MaybeError EncodeTimestampsToNanosecondsConversion(CommandEncoder* encoder,
                                                           QuerySetBase* querySet,
                                                           uint32_t firstQuery,
                                                           uint32_t queryCount,
                                                           BufferBase* destination,
                                                           uint64_t destinationOffset) {
            DeviceBase* device = encoder->GetDevice();

            // The availability got from query set is a reference to vector<bool>, need to covert
            // bool to uint32_t due to a user input in pipeline must not contain a bool type in
            // WGSL.
            std::vector<uint32_t> availability{querySet->GetQueryAvailability().begin(),
                                               querySet->GetQueryAvailability().end()};

            // Timestamp availability storage buffer
            BufferDescriptor availabilityDesc = {};
            availabilityDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
            availabilityDesc.size = querySet->GetQueryCount() * sizeof(uint32_t);
            Ref<BufferBase> availabilityBuffer;
            DAWN_TRY_ASSIGN(availabilityBuffer, device->CreateBuffer(&availabilityDesc));

            DAWN_TRY(device->GetQueue()->WriteBuffer(availabilityBuffer.Get(), 0,
                                                     availability.data(),
                                                     availability.size() * sizeof(uint32_t)));

            // Timestamp params uniform buffer
            TimestampParams params = {firstQuery, queryCount,
                                      static_cast<uint32_t>(destinationOffset),
                                      device->GetTimestampPeriodInNS()};

            BufferDescriptor parmsDesc = {};
            parmsDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
            parmsDesc.size = sizeof(params);
            Ref<BufferBase> paramsBuffer;
            DAWN_TRY_ASSIGN(paramsBuffer, device->CreateBuffer(&parmsDesc));

            DAWN_TRY(
                device->GetQueue()->WriteBuffer(paramsBuffer.Get(), 0, &params, sizeof(params)));

            return EncodeConvertTimestampsToNanoseconds(
                encoder, destination, availabilityBuffer.Get(), paramsBuffer.Get());
        }

        bool IsReadOnlyDepthStencilAttachment(
            const RenderPassDepthStencilAttachment* depthStencilAttachment) {
            DAWN_ASSERT(depthStencilAttachment != nullptr);
            Aspect aspects = depthStencilAttachment->view->GetAspects();
            DAWN_ASSERT(IsSubset(aspects, Aspect::Depth | Aspect::Stencil));

            if ((aspects & Aspect::Depth) && !depthStencilAttachment->depthReadOnly) {
                return false;
            }
            if (aspects & Aspect::Stencil && !depthStencilAttachment->stencilReadOnly) {
                return false;
            }
            return true;
        }

    }  // namespace

    CommandEncoder::CommandEncoder(DeviceBase* device, const CommandEncoderDescriptor*)
        : ApiObjectBase(device, kLabelNotImplemented), mEncodingContext(device, this) {
    }

    ObjectType CommandEncoder::GetType() const {
        return ObjectType::CommandEncoder;
    }

    CommandBufferResourceUsage CommandEncoder::AcquireResourceUsages() {
        return CommandBufferResourceUsage{
            mEncodingContext.AcquireRenderPassUsages(), mEncodingContext.AcquireComputePassUsages(),
            std::move(mTopLevelBuffers), std::move(mTopLevelTextures), std::move(mUsedQuerySets)};
    }

    CommandIterator CommandEncoder::AcquireCommands() {
        return mEncodingContext.AcquireCommands();
    }

    void CommandEncoder::TrackUsedQuerySet(QuerySetBase* querySet) {
        mUsedQuerySets.insert(querySet);
    }

    void CommandEncoder::TrackQueryAvailability(QuerySetBase* querySet, uint32_t queryIndex) {
        DAWN_ASSERT(querySet != nullptr);

        if (GetDevice()->IsValidationEnabled()) {
            TrackUsedQuerySet(querySet);
        }

        // Set the query at queryIndex to available for resolving in query set.
        querySet->SetQueryAvailability(queryIndex, true);
    }

    // Implementation of the API's command recording methods

    ComputePassEncoder* CommandEncoder::APIBeginComputePass(
        const ComputePassDescriptor* descriptor) {
        DeviceBase* device = GetDevice();

        bool success = mEncodingContext.TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                DAWN_TRY(ValidateComputePassDescriptor(device, descriptor));

                allocator->Allocate<BeginComputePassCmd>(Command::BeginComputePass);

                return {};
            },
            "encoding BeginComputePass(%s).", descriptor);

        if (success) {
            ComputePassEncoder* passEncoder =
                new ComputePassEncoder(device, this, &mEncodingContext);
            mEncodingContext.EnterPass(passEncoder);
            return passEncoder;
        }

        return ComputePassEncoder::MakeError(device, this, &mEncodingContext);
    }

    RenderPassEncoder* CommandEncoder::APIBeginRenderPass(const RenderPassDescriptor* descriptor) {
        DeviceBase* device = GetDevice();

        RenderPassResourceUsageTracker usageTracker;

        uint32_t width = 0;
        uint32_t height = 0;
        Ref<AttachmentState> attachmentState;
        bool success = mEncodingContext.TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                uint32_t sampleCount = 0;

                DAWN_TRY(ValidateRenderPassDescriptor(device, descriptor, &width, &height,
                                                      &sampleCount));

                ASSERT(width > 0 && height > 0 && sampleCount > 0);

                mEncodingContext.WillBeginRenderPass();
                BeginRenderPassCmd* cmd =
                    allocator->Allocate<BeginRenderPassCmd>(Command::BeginRenderPass);

                cmd->attachmentState = device->GetOrCreateAttachmentState(descriptor);
                attachmentState = cmd->attachmentState;

                for (ColorAttachmentIndex index :
                     IterateBitSet(cmd->attachmentState->GetColorAttachmentsMask())) {
                    uint8_t i = static_cast<uint8_t>(index);
                    TextureViewBase* view = descriptor->colorAttachments[i].view;
                    TextureViewBase* resolveTarget = descriptor->colorAttachments[i].resolveTarget;

                    cmd->colorAttachments[index].view = view;
                    cmd->colorAttachments[index].resolveTarget = resolveTarget;
                    cmd->colorAttachments[index].loadOp = descriptor->colorAttachments[i].loadOp;
                    cmd->colorAttachments[index].storeOp = descriptor->colorAttachments[i].storeOp;
                    cmd->colorAttachments[index].clearColor =
                        descriptor->colorAttachments[i].clearColor;

                    usageTracker.TextureViewUsedAs(view, wgpu::TextureUsage::RenderAttachment);

                    if (resolveTarget != nullptr) {
                        usageTracker.TextureViewUsedAs(resolveTarget,
                                                       wgpu::TextureUsage::RenderAttachment);
                    }
                }

                if (cmd->attachmentState->HasDepthStencilAttachment()) {
                    TextureViewBase* view = descriptor->depthStencilAttachment->view;

                    cmd->depthStencilAttachment.view = view;
                    cmd->depthStencilAttachment.clearDepth =
                        descriptor->depthStencilAttachment->clearDepth;
                    cmd->depthStencilAttachment.clearStencil =
                        descriptor->depthStencilAttachment->clearStencil;
                    cmd->depthStencilAttachment.depthLoadOp =
                        descriptor->depthStencilAttachment->depthLoadOp;
                    cmd->depthStencilAttachment.depthStoreOp =
                        descriptor->depthStencilAttachment->depthStoreOp;
                    cmd->depthStencilAttachment.stencilLoadOp =
                        descriptor->depthStencilAttachment->stencilLoadOp;
                    cmd->depthStencilAttachment.stencilStoreOp =
                        descriptor->depthStencilAttachment->stencilStoreOp;

                    if (IsReadOnlyDepthStencilAttachment(descriptor->depthStencilAttachment)) {
                        // TODO(dawn:485): Readonly depth/stencil attachment is not fully
                        // implemented. Disallow it as unsafe until the implementaion is completed.
                        DAWN_INVALID_IF(
                            device->IsToggleEnabled(Toggle::DisallowUnsafeAPIs),
                            "Readonly depth/stencil attachment is disallowed because it's not "
                            "fully implemented");

                        usageTracker.TextureViewUsedAs(view, kReadOnlyRenderAttachment);
                    } else {
                        usageTracker.TextureViewUsedAs(view, wgpu::TextureUsage::RenderAttachment);
                    }
                }

                cmd->width = width;
                cmd->height = height;

                cmd->occlusionQuerySet = descriptor->occlusionQuerySet;

                return {};
            },
            "encoding BeginRenderPass(%s).", descriptor);

        if (success) {
            RenderPassEncoder* passEncoder = new RenderPassEncoder(
                device, this, &mEncodingContext, std::move(usageTracker),
                std::move(attachmentState), descriptor->occlusionQuerySet, width, height);
            mEncodingContext.EnterPass(passEncoder);
            return passEncoder;
        }

        return RenderPassEncoder::MakeError(device, this, &mEncodingContext);
    }

    void CommandEncoder::APICopyBufferToBuffer(BufferBase* source,
                                               uint64_t sourceOffset,
                                               BufferBase* destination,
                                               uint64_t destinationOffset,
                                               uint64_t size) {
        mEncodingContext.TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (GetDevice()->IsValidationEnabled()) {
                    DAWN_TRY(GetDevice()->ValidateObject(source));
                    DAWN_TRY(GetDevice()->ValidateObject(destination));

                    DAWN_INVALID_IF(source == destination,
                                    "Source and destination are the same buffer (%s).", source);

                    DAWN_TRY_CONTEXT(ValidateCopySizeFitsInBuffer(source, sourceOffset, size),
                                     "validating source %s copy size.", source);
                    DAWN_TRY_CONTEXT(
                        ValidateCopySizeFitsInBuffer(destination, destinationOffset, size),
                        "validating destination %s copy size.", destination);
                    DAWN_TRY(ValidateB2BCopyAlignment(size, sourceOffset, destinationOffset));

                    DAWN_TRY_CONTEXT(ValidateCanUseAs(source, wgpu::BufferUsage::CopySrc),
                                     "validating source %s usage.", source);
                    DAWN_TRY_CONTEXT(ValidateCanUseAs(destination, wgpu::BufferUsage::CopyDst),
                                     "validating destination %s usage.", destination);

                    mTopLevelBuffers.insert(source);
                    mTopLevelBuffers.insert(destination);
                }

                CopyBufferToBufferCmd* copy =
                    allocator->Allocate<CopyBufferToBufferCmd>(Command::CopyBufferToBuffer);
                copy->source = source;
                copy->sourceOffset = sourceOffset;
                copy->destination = destination;
                copy->destinationOffset = destinationOffset;
                copy->size = size;

                return {};
            },
            "encoding CopyBufferToBuffer(%s, %u, %s, %u, %u).", source, sourceOffset, destination,
            destinationOffset, size);
    }

    void CommandEncoder::APICopyBufferToTexture(const ImageCopyBuffer* source,
                                                const ImageCopyTexture* destination,
                                                const Extent3D* copySize) {
        mEncodingContext.TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (GetDevice()->IsValidationEnabled()) {
                    DAWN_TRY(ValidateImageCopyBuffer(GetDevice(), *source));
                    DAWN_TRY_CONTEXT(ValidateCanUseAs(source->buffer, wgpu::BufferUsage::CopySrc),
                                     "validating source %s usage.", source->buffer);

                    DAWN_TRY(ValidateImageCopyTexture(GetDevice(), *destination, *copySize));
                    DAWN_TRY_CONTEXT(
                        ValidateCanUseAs(destination->texture, wgpu::TextureUsage::CopyDst),
                        "validating destination %s usage.", destination->texture);
                    DAWN_TRY(ValidateTextureSampleCountInBufferCopyCommands(destination->texture));

                    DAWN_TRY(ValidateLinearToDepthStencilCopyRestrictions(*destination));
                    // We validate texture copy range before validating linear texture data,
                    // because in the latter we divide copyExtent.width by blockWidth and
                    // copyExtent.height by blockHeight while the divisibility conditions are
                    // checked in validating texture copy range.
                    DAWN_TRY(ValidateTextureCopyRange(GetDevice(), *destination, *copySize));
                }
                const TexelBlockInfo& blockInfo =
                    destination->texture->GetFormat().GetAspectInfo(destination->aspect).block;
                if (GetDevice()->IsValidationEnabled()) {
                    DAWN_TRY(ValidateLinearTextureCopyOffset(
                        source->layout, blockInfo,
                        destination->texture->GetFormat().HasDepthOrStencil()));
                    DAWN_TRY(ValidateLinearTextureData(source->layout, source->buffer->GetSize(),
                                                       blockInfo, *copySize));

                    mTopLevelBuffers.insert(source->buffer);
                    mTopLevelTextures.insert(destination->texture);
                }

                TextureDataLayout srcLayout = source->layout;
                ApplyDefaultTextureDataLayoutOptions(&srcLayout, blockInfo, *copySize);

                CopyBufferToTextureCmd* copy =
                    allocator->Allocate<CopyBufferToTextureCmd>(Command::CopyBufferToTexture);
                copy->source.buffer = source->buffer;
                copy->source.offset = srcLayout.offset;
                copy->source.bytesPerRow = srcLayout.bytesPerRow;
                copy->source.rowsPerImage = srcLayout.rowsPerImage;
                copy->destination.texture = destination->texture;
                copy->destination.origin = destination->origin;
                copy->destination.mipLevel = destination->mipLevel;
                copy->destination.aspect =
                    ConvertAspect(destination->texture->GetFormat(), destination->aspect);
                copy->copySize = *copySize;

                return {};
            },
            "encoding CopyBufferToTexture(%s, %s, %s).", source->buffer, destination->texture,
            copySize);
    }

    void CommandEncoder::APICopyTextureToBuffer(const ImageCopyTexture* source,
                                                const ImageCopyBuffer* destination,
                                                const Extent3D* copySize) {
        mEncodingContext.TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (GetDevice()->IsValidationEnabled()) {
                    DAWN_TRY(ValidateImageCopyTexture(GetDevice(), *source, *copySize));
                    DAWN_TRY_CONTEXT(ValidateCanUseAs(source->texture, wgpu::TextureUsage::CopySrc),
                                     "validating source %s usage.", source->texture);
                    DAWN_TRY(ValidateTextureSampleCountInBufferCopyCommands(source->texture));
                    DAWN_TRY(ValidateTextureDepthStencilToBufferCopyRestrictions(*source));

                    DAWN_TRY(ValidateImageCopyBuffer(GetDevice(), *destination));
                    DAWN_TRY_CONTEXT(
                        ValidateCanUseAs(destination->buffer, wgpu::BufferUsage::CopyDst),
                        "validating destination %s usage.", destination->buffer);

                    // We validate texture copy range before validating linear texture data,
                    // because in the latter we divide copyExtent.width by blockWidth and
                    // copyExtent.height by blockHeight while the divisibility conditions are
                    // checked in validating texture copy range.
                    DAWN_TRY(ValidateTextureCopyRange(GetDevice(), *source, *copySize));
                }
                const TexelBlockInfo& blockInfo =
                    source->texture->GetFormat().GetAspectInfo(source->aspect).block;
                if (GetDevice()->IsValidationEnabled()) {
                    DAWN_TRY(ValidateLinearTextureCopyOffset(
                        destination->layout, blockInfo,
                        source->texture->GetFormat().HasDepthOrStencil()));
                    DAWN_TRY(ValidateLinearTextureData(
                        destination->layout, destination->buffer->GetSize(), blockInfo, *copySize));

                    mTopLevelTextures.insert(source->texture);
                    mTopLevelBuffers.insert(destination->buffer);
                }

                TextureDataLayout dstLayout = destination->layout;
                ApplyDefaultTextureDataLayoutOptions(&dstLayout, blockInfo, *copySize);

                CopyTextureToBufferCmd* copy =
                    allocator->Allocate<CopyTextureToBufferCmd>(Command::CopyTextureToBuffer);
                copy->source.texture = source->texture;
                copy->source.origin = source->origin;
                copy->source.mipLevel = source->mipLevel;
                copy->source.aspect = ConvertAspect(source->texture->GetFormat(), source->aspect);
                copy->destination.buffer = destination->buffer;
                copy->destination.offset = dstLayout.offset;
                copy->destination.bytesPerRow = dstLayout.bytesPerRow;
                copy->destination.rowsPerImage = dstLayout.rowsPerImage;
                copy->copySize = *copySize;

                return {};
            },
            "encoding CopyTextureToBuffer(%s, %s, %s).", source->texture, destination->buffer,
            copySize);
    }

    void CommandEncoder::APICopyTextureToTexture(const ImageCopyTexture* source,
                                                 const ImageCopyTexture* destination,
                                                 const Extent3D* copySize) {
        APICopyTextureToTextureHelper<false>(source, destination, copySize);
    }

    void CommandEncoder::APICopyTextureToTextureInternal(const ImageCopyTexture* source,
                                                         const ImageCopyTexture* destination,
                                                         const Extent3D* copySize) {
        APICopyTextureToTextureHelper<true>(source, destination, copySize);
    }

    template <bool Internal>
    void CommandEncoder::APICopyTextureToTextureHelper(const ImageCopyTexture* source,
                                                       const ImageCopyTexture* destination,
                                                       const Extent3D* copySize) {
        mEncodingContext.TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (GetDevice()->IsValidationEnabled()) {
                    DAWN_TRY(GetDevice()->ValidateObject(source->texture));
                    DAWN_TRY(GetDevice()->ValidateObject(destination->texture));

                    DAWN_TRY_CONTEXT(ValidateImageCopyTexture(GetDevice(), *source, *copySize),
                                     "validating source %s.", source->texture);
                    DAWN_TRY_CONTEXT(ValidateImageCopyTexture(GetDevice(), *destination, *copySize),
                                     "validating destination %s.", destination->texture);

                    DAWN_TRY(
                        ValidateTextureToTextureCopyRestrictions(*source, *destination, *copySize));

                    DAWN_TRY_CONTEXT(ValidateTextureCopyRange(GetDevice(), *source, *copySize),
                                     "validating source %s copy range.", source->texture);
                    DAWN_TRY_CONTEXT(ValidateTextureCopyRange(GetDevice(), *destination, *copySize),
                                     "validating source %s copy range.", destination->texture);

                    // For internal usages (CopyToCopyInternal) we don't care if the user has added
                    // CopySrc as a usage for this texture, but we will always add it internally.
                    if (Internal) {
                        DAWN_TRY(
                            ValidateInternalCanUseAs(source->texture, wgpu::TextureUsage::CopySrc));
                        DAWN_TRY(ValidateInternalCanUseAs(destination->texture,
                                                          wgpu::TextureUsage::CopyDst));
                    } else {
                        DAWN_TRY(ValidateCanUseAs(source->texture, wgpu::TextureUsage::CopySrc));
                        DAWN_TRY(
                            ValidateCanUseAs(destination->texture, wgpu::TextureUsage::CopyDst));
                    }

                    mTopLevelTextures.insert(source->texture);
                    mTopLevelTextures.insert(destination->texture);
                }

                CopyTextureToTextureCmd* copy =
                    allocator->Allocate<CopyTextureToTextureCmd>(Command::CopyTextureToTexture);
                copy->source.texture = source->texture;
                copy->source.origin = source->origin;
                copy->source.mipLevel = source->mipLevel;
                copy->source.aspect = ConvertAspect(source->texture->GetFormat(), source->aspect);
                copy->destination.texture = destination->texture;
                copy->destination.origin = destination->origin;
                copy->destination.mipLevel = destination->mipLevel;
                copy->destination.aspect =
                    ConvertAspect(destination->texture->GetFormat(), destination->aspect);
                copy->copySize = *copySize;

                return {};
            },
            "encoding CopyTextureToTexture(%s, %s, %s).", source->texture, destination->texture,
            copySize);
    }

    void CommandEncoder::APIInjectValidationError(const char* message) {
        if (mEncodingContext.CheckCurrentEncoder(this)) {
            mEncodingContext.HandleError(DAWN_VALIDATION_ERROR(message));
        }
    }

    void CommandEncoder::APIInsertDebugMarker(const char* groupLabel) {
        mEncodingContext.TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                InsertDebugMarkerCmd* cmd =
                    allocator->Allocate<InsertDebugMarkerCmd>(Command::InsertDebugMarker);
                cmd->length = strlen(groupLabel);

                char* label = allocator->AllocateData<char>(cmd->length + 1);
                memcpy(label, groupLabel, cmd->length + 1);

                return {};
            },
            "encoding InsertDebugMarker(\"%s\").", groupLabel);
    }

    void CommandEncoder::APIPopDebugGroup() {
        mEncodingContext.TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (GetDevice()->IsValidationEnabled()) {
                    DAWN_INVALID_IF(
                        mDebugGroupStackSize == 0,
                        "Every call to PopDebugGroup must be balanced by a corresponding call to "
                        "PushDebugGroup.");
                }
                allocator->Allocate<PopDebugGroupCmd>(Command::PopDebugGroup);
                mDebugGroupStackSize--;
                mEncodingContext.PopDebugGroupLabel();

                return {};
            },
            "encoding PopDebugGroup().");
    }

    void CommandEncoder::APIPushDebugGroup(const char* groupLabel) {
        mEncodingContext.TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                PushDebugGroupCmd* cmd =
                    allocator->Allocate<PushDebugGroupCmd>(Command::PushDebugGroup);
                cmd->length = strlen(groupLabel);

                char* label = allocator->AllocateData<char>(cmd->length + 1);
                memcpy(label, groupLabel, cmd->length + 1);

                mDebugGroupStackSize++;
                mEncodingContext.PushDebugGroupLabel(groupLabel);

                return {};
            },
            "encoding PushDebugGroup(\"%s\").", groupLabel);
    }

    void CommandEncoder::APIResolveQuerySet(QuerySetBase* querySet,
                                            uint32_t firstQuery,
                                            uint32_t queryCount,
                                            BufferBase* destination,
                                            uint64_t destinationOffset) {
        mEncodingContext.TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (GetDevice()->IsValidationEnabled()) {
                    DAWN_TRY(GetDevice()->ValidateObject(querySet));
                    DAWN_TRY(GetDevice()->ValidateObject(destination));

                    DAWN_TRY(ValidateQuerySetResolve(querySet, firstQuery, queryCount, destination,
                                                     destinationOffset));

                    DAWN_TRY(ValidateCanUseAs(destination, wgpu::BufferUsage::QueryResolve));

                    TrackUsedQuerySet(querySet);
                    mTopLevelBuffers.insert(destination);
                }

                ResolveQuerySetCmd* cmd =
                    allocator->Allocate<ResolveQuerySetCmd>(Command::ResolveQuerySet);
                cmd->querySet = querySet;
                cmd->firstQuery = firstQuery;
                cmd->queryCount = queryCount;
                cmd->destination = destination;
                cmd->destinationOffset = destinationOffset;

                // Encode internal compute pipeline for timestamp query
                if (querySet->GetQueryType() == wgpu::QueryType::Timestamp) {
                    DAWN_TRY(EncodeTimestampsToNanosecondsConversion(
                        this, querySet, firstQuery, queryCount, destination, destinationOffset));
                }

                return {};
            },
            "encoding ResolveQuerySet(%s, %u, %u, %s, %u).", querySet, firstQuery, queryCount,
            destination, destinationOffset);
    }

    void CommandEncoder::APIWriteBuffer(BufferBase* buffer,
                                        uint64_t bufferOffset,
                                        const uint8_t* data,
                                        uint64_t size) {
        mEncodingContext.TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (GetDevice()->IsValidationEnabled()) {
                    DAWN_TRY(ValidateWriteBuffer(GetDevice(), buffer, bufferOffset, size));
                }

                WriteBufferCmd* cmd = allocator->Allocate<WriteBufferCmd>(Command::WriteBuffer);
                cmd->buffer = buffer;
                cmd->offset = bufferOffset;
                cmd->size = size;

                uint8_t* inlinedData = allocator->AllocateData<uint8_t>(size);
                memcpy(inlinedData, data, size);

                mTopLevelBuffers.insert(buffer);

                return {};
            },
            "encoding WriteBuffer(%s, %u, ..., %u).", buffer, bufferOffset, size);
    }

    void CommandEncoder::APIWriteTimestamp(QuerySetBase* querySet, uint32_t queryIndex) {
        mEncodingContext.TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (GetDevice()->IsValidationEnabled()) {
                    DAWN_TRY(GetDevice()->ValidateObject(querySet));
                    DAWN_TRY(ValidateTimestampQuery(querySet, queryIndex));
                }

                TrackQueryAvailability(querySet, queryIndex);

                WriteTimestampCmd* cmd =
                    allocator->Allocate<WriteTimestampCmd>(Command::WriteTimestamp);
                cmd->querySet = querySet;
                cmd->queryIndex = queryIndex;

                return {};
            },
            "encoding WriteTimestamp(%s, %u).", querySet, queryIndex);
    }

    CommandBufferBase* CommandEncoder::APIFinish(const CommandBufferDescriptor* descriptor) {
        Ref<CommandBufferBase> commandBuffer;
        if (GetDevice()->ConsumedError(FinishInternal(descriptor), &commandBuffer)) {
            return CommandBufferBase::MakeError(GetDevice());
        }
        ASSERT(!IsError());
        return commandBuffer.Detach();
    }

    void CommandEncoder::EncodeSetValidatedBufferLocationsInternal(
        std::vector<DeferredBufferLocationUpdate> updates) {
        ASSERT(GetDevice()->IsValidationEnabled());
        mEncodingContext.TryEncode(this, [&](CommandAllocator* allocator) -> MaybeError {
            SetValidatedBufferLocationsInternalCmd* cmd =
                allocator->Allocate<SetValidatedBufferLocationsInternalCmd>(
                    Command::SetValidatedBufferLocationsInternal);
            cmd->updates = std::move(updates);
            return {};
        });
    }

    ResultOrError<Ref<CommandBufferBase>> CommandEncoder::FinishInternal(
        const CommandBufferDescriptor* descriptor) {
        DeviceBase* device = GetDevice();

        // Even if mEncodingContext.Finish() validation fails, calling it will mutate the internal
        // state of the encoding context. The internal state is set to finished, and subsequent
        // calls to encode commands will generate errors.
        DAWN_TRY(mEncodingContext.Finish());
        DAWN_TRY(device->ValidateIsAlive());

        if (device->IsValidationEnabled()) {
            DAWN_TRY(ValidateFinish());
        }
        return device->CreateCommandBuffer(this, descriptor);
    }

    // Implementation of the command buffer validation that can be precomputed before submit
    MaybeError CommandEncoder::ValidateFinish() const {
        TRACE_EVENT0(GetDevice()->GetPlatform(), Validation, "CommandEncoder::ValidateFinish");
        DAWN_TRY(GetDevice()->ValidateObject(this));

        for (const RenderPassResourceUsage& passUsage : mEncodingContext.GetRenderPassUsages()) {
            DAWN_TRY_CONTEXT(ValidateSyncScopeResourceUsage(passUsage),
                             "validating render pass usage.");
        }

        for (const ComputePassResourceUsage& passUsage : mEncodingContext.GetComputePassUsages()) {
            for (const SyncScopeResourceUsage& scope : passUsage.dispatchUsages) {
                DAWN_TRY_CONTEXT(ValidateSyncScopeResourceUsage(scope),
                                 "validating compute pass usage.");
            }
        }

        DAWN_INVALID_IF(
            mDebugGroupStackSize != 0,
            "PushDebugGroup called %u time(s) without a corresponding PopDebugGroup prior to "
            "calling Finish.",
            mDebugGroupStackSize);

        return {};
    }

}  // namespace dawn_native
