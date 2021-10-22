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

#include "dawn_native/RenderPassEncoder.h"

#include "common/Constants.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandEncoder.h"
#include "dawn_native/CommandValidation.h"
#include "dawn_native/Commands.h"
#include "dawn_native/Device.h"
#include "dawn_native/ObjectType_autogen.h"
#include "dawn_native/QuerySet.h"
#include "dawn_native/RenderBundle.h"
#include "dawn_native/RenderPipeline.h"

#include <math.h>
#include <cstring>

namespace dawn_native {
    namespace {

        // Check the query at queryIndex is unavailable, otherwise it cannot be written.
        MaybeError ValidateQueryIndexOverwrite(QuerySetBase* querySet,
                                               uint32_t queryIndex,
                                               const QueryAvailabilityMap& queryAvailabilityMap) {
            auto it = queryAvailabilityMap.find(querySet);
            DAWN_INVALID_IF(it != queryAvailabilityMap.end() && it->second[queryIndex],
                            "Query index %u of %s is written to twice in a render pass.",
                            queryIndex, querySet);

            return {};
        }

    }  // namespace

    // The usage tracker is passed in here, because it is prepopulated with usages from the
    // BeginRenderPassCmd. If we had RenderPassEncoder responsible for recording the
    // command, then this wouldn't be necessary.
    RenderPassEncoder::RenderPassEncoder(DeviceBase* device,
                                         CommandEncoder* commandEncoder,
                                         EncodingContext* encodingContext,
                                         RenderPassResourceUsageTracker usageTracker,
                                         Ref<AttachmentState> attachmentState,
                                         QuerySetBase* occlusionQuerySet,
                                         uint32_t renderTargetWidth,
                                         uint32_t renderTargetHeight)
        : RenderEncoderBase(device, encodingContext, std::move(attachmentState)),
          mCommandEncoder(commandEncoder),
          mRenderTargetWidth(renderTargetWidth),
          mRenderTargetHeight(renderTargetHeight),
          mOcclusionQuerySet(occlusionQuerySet) {
        mUsageTracker = std::move(usageTracker);
    }

    RenderPassEncoder::RenderPassEncoder(DeviceBase* device,
                                         CommandEncoder* commandEncoder,
                                         EncodingContext* encodingContext,
                                         ErrorTag errorTag)
        : RenderEncoderBase(device, encodingContext, errorTag), mCommandEncoder(commandEncoder) {
    }

    RenderPassEncoder* RenderPassEncoder::MakeError(DeviceBase* device,
                                                    CommandEncoder* commandEncoder,
                                                    EncodingContext* encodingContext) {
        return new RenderPassEncoder(device, commandEncoder, encodingContext, ObjectBase::kError);
    }

    ObjectType RenderPassEncoder::GetType() const {
        return ObjectType::RenderPassEncoder;
    }

    void RenderPassEncoder::TrackQueryAvailability(QuerySetBase* querySet, uint32_t queryIndex) {
        DAWN_ASSERT(querySet != nullptr);

        // Track the query availability with true on render pass for rewrite validation and query
        // reset on render pass on Vulkan
        mUsageTracker.TrackQueryAvailability(querySet, queryIndex);

        // Track it again on command encoder for zero-initializing when resolving unused queries.
        mCommandEncoder->TrackQueryAvailability(querySet, queryIndex);
    }

    void RenderPassEncoder::APIEndPass() {
        if (mEncodingContext->TryEncode(
                this,
                [&](CommandAllocator* allocator) -> MaybeError {
                    if (IsValidationEnabled()) {
                        DAWN_TRY(ValidateProgrammableEncoderEnd());

                        DAWN_INVALID_IF(
                            mOcclusionQueryActive,
                            "Render pass %s ended with incomplete occlusion query index %u of %s.",
                            this, mCurrentOcclusionQueryIndex, mOcclusionQuerySet.Get());
                    }

                    allocator->Allocate<EndRenderPassCmd>(Command::EndRenderPass);
                    DAWN_TRY(mEncodingContext->ExitRenderPass(this, std::move(mUsageTracker),
                                                              mCommandEncoder.Get(),
                                                              std::move(mIndirectDrawMetadata)));
                    return {};
                },
                "encoding EndPass().")) {
        }
    }

    void RenderPassEncoder::APISetStencilReference(uint32_t reference) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                SetStencilReferenceCmd* cmd =
                    allocator->Allocate<SetStencilReferenceCmd>(Command::SetStencilReference);
                cmd->reference = reference;

                return {};
            },
            "encoding SetStencilReference(%u)", reference);
    }

    void RenderPassEncoder::APISetBlendConstant(const Color* color) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                SetBlendConstantCmd* cmd =
                    allocator->Allocate<SetBlendConstantCmd>(Command::SetBlendConstant);
                cmd->color = *color;

                return {};
            },
            "encoding SetBlendConstant(%s).", color);
    }

    void RenderPassEncoder::APISetViewport(float x,
                                           float y,
                                           float width,
                                           float height,
                                           float minDepth,
                                           float maxDepth) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (IsValidationEnabled()) {
                    DAWN_INVALID_IF(
                        (isnan(x) || isnan(y) || isnan(width) || isnan(height) || isnan(minDepth) ||
                         isnan(maxDepth)),
                        "A parameter of the viewport (x: %f, y: %f, width: %f, height: %f, "
                        "minDepth: %f, maxDepth: %f) is NaN.",
                        x, y, width, height, minDepth, maxDepth);

                    DAWN_INVALID_IF(
                        x < 0 || y < 0 || width < 0 || height < 0,
                        "Viewport bounds (x: %f, y: %f, width: %f, height: %f) contains a negative "
                        "value.",
                        x, y, width, height);

                    DAWN_INVALID_IF(
                        x + width > mRenderTargetWidth || y + height > mRenderTargetHeight,
                        "Viewport bounds (x: %f, y: %f, width: %f, height: %f) are not contained "
                        "in "
                        "the render target dimensions (%u x %u).",
                        x, y, width, height, mRenderTargetWidth, mRenderTargetHeight);

                    // Check for depths being in [0, 1] and min <= max in 3 checks instead of 5.
                    DAWN_INVALID_IF(minDepth < 0 || minDepth > maxDepth || maxDepth > 1,
                                    "Viewport minDepth (%f) and maxDepth (%f) are not in [0, 1] or "
                                    "minDepth was "
                                    "greater than maxDepth.",
                                    minDepth, maxDepth);
                }

                SetViewportCmd* cmd = allocator->Allocate<SetViewportCmd>(Command::SetViewport);
                cmd->x = x;
                cmd->y = y;
                cmd->width = width;
                cmd->height = height;
                cmd->minDepth = minDepth;
                cmd->maxDepth = maxDepth;

                return {};
            },
            "encoding SetViewport(%f, %f, %f, %f, %f, %f).", x, y, width, height, minDepth,
            maxDepth);
    }

    void RenderPassEncoder::APISetScissorRect(uint32_t x,
                                              uint32_t y,
                                              uint32_t width,
                                              uint32_t height) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (IsValidationEnabled()) {
                    DAWN_INVALID_IF(
                        width > mRenderTargetWidth || height > mRenderTargetHeight ||
                            x > mRenderTargetWidth - width || y > mRenderTargetHeight - height,
                        "Scissor rect (x: %u, y: %u, width: %u, height: %u) is not contained in "
                        "the render target dimensions (%u x %u).",
                        x, y, width, height, mRenderTargetWidth, mRenderTargetHeight);
                }

                SetScissorRectCmd* cmd =
                    allocator->Allocate<SetScissorRectCmd>(Command::SetScissorRect);
                cmd->x = x;
                cmd->y = y;
                cmd->width = width;
                cmd->height = height;

                return {};
            },
            "encoding SetScissorRect(%u, %u, %u, %u).", x, y, width, height);
    }

    void RenderPassEncoder::APIExecuteBundles(uint32_t count,
                                              RenderBundleBase* const* renderBundles) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (IsValidationEnabled()) {
                    for (uint32_t i = 0; i < count; ++i) {
                        DAWN_TRY(GetDevice()->ValidateObject(renderBundles[i]));

                        // TODO(dawn:563): Give more detail about why the states are incompatible.
                        DAWN_INVALID_IF(
                            GetAttachmentState() != renderBundles[i]->GetAttachmentState(),
                            "Attachment state of renderBundles[%i] (%s) is not compatible with "
                            "attachment state of %s.",
                            i, renderBundles[i], this);
                    }
                }

                mCommandBufferState = CommandBufferStateTracker{};

                ExecuteBundlesCmd* cmd =
                    allocator->Allocate<ExecuteBundlesCmd>(Command::ExecuteBundles);
                cmd->count = count;

                Ref<RenderBundleBase>* bundles =
                    allocator->AllocateData<Ref<RenderBundleBase>>(count);
                for (uint32_t i = 0; i < count; ++i) {
                    bundles[i] = renderBundles[i];

                    const RenderPassResourceUsage& usages = bundles[i]->GetResourceUsage();
                    for (uint32_t i = 0; i < usages.buffers.size(); ++i) {
                        mUsageTracker.BufferUsedAs(usages.buffers[i], usages.bufferUsages[i]);
                    }

                    for (uint32_t i = 0; i < usages.textures.size(); ++i) {
                        mUsageTracker.AddRenderBundleTextureUsage(usages.textures[i],
                                                                  usages.textureUsages[i]);
                    }

                    if (IsValidationEnabled()) {
                        mIndirectDrawMetadata.AddBundle(renderBundles[i]);
                    }
                }

                return {};
            },
            "encoding ExecuteBundles(%u, ...)", count);
    }

    void RenderPassEncoder::APIBeginOcclusionQuery(uint32_t queryIndex) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (IsValidationEnabled()) {
                    DAWN_INVALID_IF(mOcclusionQuerySet.Get() == nullptr,
                                    "The occlusionQuerySet in RenderPassDescriptor is not set.");

                    // The type of querySet has been validated by ValidateRenderPassDescriptor

                    DAWN_INVALID_IF(queryIndex >= mOcclusionQuerySet->GetQueryCount(),
                                    "Query index (%u) exceeds the number of queries (%u) in %s.",
                                    queryIndex, mOcclusionQuerySet->GetQueryCount(),
                                    mOcclusionQuerySet.Get());

                    DAWN_INVALID_IF(mOcclusionQueryActive,
                                    "An occlusion query (%u) in %s is already active.",
                                    mCurrentOcclusionQueryIndex, mOcclusionQuerySet.Get());

                    DAWN_TRY_CONTEXT(
                        ValidateQueryIndexOverwrite(mOcclusionQuerySet.Get(), queryIndex,
                                                    mUsageTracker.GetQueryAvailabilityMap()),
                        "validating the occlusion query index (%u) in %s", queryIndex,
                        mOcclusionQuerySet.Get());
                }

                // Record the current query index for endOcclusionQuery.
                mCurrentOcclusionQueryIndex = queryIndex;
                mOcclusionQueryActive = true;

                BeginOcclusionQueryCmd* cmd =
                    allocator->Allocate<BeginOcclusionQueryCmd>(Command::BeginOcclusionQuery);
                cmd->querySet = mOcclusionQuerySet.Get();
                cmd->queryIndex = queryIndex;

                return {};
            },
            "encoding BeginOcclusionQuery(%u)", queryIndex);
    }

    void RenderPassEncoder::APIEndOcclusionQuery() {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (IsValidationEnabled()) {
                    DAWN_INVALID_IF(!mOcclusionQueryActive, "No occlusion queries are active.");
                }

                TrackQueryAvailability(mOcclusionQuerySet.Get(), mCurrentOcclusionQueryIndex);

                mOcclusionQueryActive = false;

                EndOcclusionQueryCmd* cmd =
                    allocator->Allocate<EndOcclusionQueryCmd>(Command::EndOcclusionQuery);
                cmd->querySet = mOcclusionQuerySet.Get();
                cmd->queryIndex = mCurrentOcclusionQueryIndex;

                return {};
            },
            "encoding EndOcclusionQuery()");
    }

    void RenderPassEncoder::APIWriteTimestamp(QuerySetBase* querySet, uint32_t queryIndex) {
        mEncodingContext->TryEncode(
            this,
            [&](CommandAllocator* allocator) -> MaybeError {
                if (IsValidationEnabled()) {
                    DAWN_TRY(GetDevice()->ValidateObject(querySet));
                    DAWN_TRY(ValidateTimestampQuery(querySet, queryIndex));
                    DAWN_TRY_CONTEXT(
                        ValidateQueryIndexOverwrite(querySet, queryIndex,
                                                    mUsageTracker.GetQueryAvailabilityMap()),
                        "validating the timestamp query index (%u) of %s", queryIndex, querySet);
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

}  // namespace dawn_native
