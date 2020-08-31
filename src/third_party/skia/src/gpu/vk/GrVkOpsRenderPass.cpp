/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/vk/GrVkOpsRenderPass.h"

#include "include/core/SkDrawable.h"
#include "include/core/SkRect.h"
#include "include/gpu/GrBackendDrawableInfo.h"
#include "src/gpu/GrContextPriv.h"
#include "src/gpu/GrFixedClip.h"
#include "src/gpu/GrOpFlushState.h"
#include "src/gpu/GrPipeline.h"
#include "src/gpu/GrRenderTargetPriv.h"
#include "src/gpu/vk/GrVkCommandBuffer.h"
#include "src/gpu/vk/GrVkCommandPool.h"
#include "src/gpu/vk/GrVkGpu.h"
#include "src/gpu/vk/GrVkPipeline.h"
#include "src/gpu/vk/GrVkRenderPass.h"
#include "src/gpu/vk/GrVkRenderTarget.h"
#include "src/gpu/vk/GrVkResourceProvider.h"
#include "src/gpu/vk/GrVkSemaphore.h"
#include "src/gpu/vk/GrVkTexture.h"

/////////////////////////////////////////////////////////////////////////////

void get_vk_load_store_ops(GrLoadOp loadOpIn, GrStoreOp storeOpIn,
                           VkAttachmentLoadOp* loadOp, VkAttachmentStoreOp* storeOp) {
    switch (loadOpIn) {
        case GrLoadOp::kLoad:
            *loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            break;
        case GrLoadOp::kClear:
            *loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            break;
        case GrLoadOp::kDiscard:
            *loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            break;
        default:
            SK_ABORT("Invalid LoadOp");
            *loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    }

    switch (storeOpIn) {
        case GrStoreOp::kStore:
            *storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            break;
        case GrStoreOp::kDiscard:
            *storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            break;
        default:
            SK_ABORT("Invalid StoreOp");
            *storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    }
}

GrVkOpsRenderPass::GrVkOpsRenderPass(GrVkGpu* gpu) : fGpu(gpu) {}

bool GrVkOpsRenderPass::init(const GrOpsRenderPass::LoadAndStoreInfo& colorInfo,
                             const GrOpsRenderPass::StencilLoadAndStoreInfo& stencilInfo,
                             const SkPMColor4f& clearColor) {

    VkAttachmentLoadOp loadOp;
    VkAttachmentStoreOp storeOp;
    get_vk_load_store_ops(colorInfo.fLoadOp, colorInfo.fStoreOp,
                          &loadOp, &storeOp);
    GrVkRenderPass::LoadStoreOps vkColorOps(loadOp, storeOp);

    get_vk_load_store_ops(stencilInfo.fLoadOp, stencilInfo.fStoreOp,
                          &loadOp, &storeOp);
    GrVkRenderPass::LoadStoreOps vkStencilOps(loadOp, storeOp);

    GrVkRenderTarget* vkRT = static_cast<GrVkRenderTarget*>(fRenderTarget);
    GrVkImage* targetImage = vkRT->msaaImage() ? vkRT->msaaImage() : vkRT;

    // Change layout of our render target so it can be used as the color attachment.
    // TODO: If we know that we will never be blending or loading the attachment we could drop the
    // VK_ACCESS_COLOR_ATTACHMENT_READ_BIT.
    targetImage->setImageLayout(fGpu,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                false);

    // If we are using a stencil attachment we also need to update its layout
    if (GrStencilAttachment* stencil = fRenderTarget->renderTargetPriv().getStencilAttachment()) {
        GrVkStencilAttachment* vkStencil = (GrVkStencilAttachment*)stencil;
        // We need the write and read access bits since we may load and store the stencil.
        // The initial load happens in the VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT so we
        // wait there.
        vkStencil->setImageLayout(fGpu,
                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                  false);
    }

    const GrVkResourceProvider::CompatibleRPHandle& rpHandle =
            vkRT->compatibleRenderPassHandle();
    if (rpHandle.isValid()) {
        fCurrentRenderPass = fGpu->resourceProvider().findRenderPass(rpHandle,
                                                                     vkColorOps,
                                                                     vkStencilOps);
    } else {
        fCurrentRenderPass = fGpu->resourceProvider().findRenderPass(vkRT,
                                                                     vkColorOps,
                                                                     vkStencilOps);
    }
    if (!fCurrentRenderPass) {
        return false;
    }

    VkClearValue vkClearColor;
    vkClearColor.color.float32[0] = clearColor[0];
    vkClearColor.color.float32[1] = clearColor[1];
    vkClearColor.color.float32[2] = clearColor[2];
    vkClearColor.color.float32[3] = clearColor[3];

    if (!fGpu->vkCaps().preferPrimaryOverSecondaryCommandBuffers()) {
        SkASSERT(fGpu->cmdPool());
        fCurrentSecondaryCommandBuffer = fGpu->cmdPool()->findOrCreateSecondaryCommandBuffer(fGpu);
        if (!fCurrentSecondaryCommandBuffer) {
            fCurrentRenderPass = nullptr;
            return false;
        }
        fCurrentSecondaryCommandBuffer->begin(fGpu, vkRT->getFramebuffer(), fCurrentRenderPass);
    }

    if (!fGpu->beginRenderPass(fCurrentRenderPass, &vkClearColor, vkRT, fOrigin, fBounds,
                               SkToBool(fCurrentSecondaryCommandBuffer))) {
        if (fCurrentSecondaryCommandBuffer) {
            fCurrentSecondaryCommandBuffer->end(fGpu);
        }
        fCurrentRenderPass = nullptr;
        return false;
    }
    return true;
}

bool GrVkOpsRenderPass::initWrapped() {
    GrVkRenderTarget* vkRT = static_cast<GrVkRenderTarget*>(fRenderTarget);
    SkASSERT(vkRT->wrapsSecondaryCommandBuffer());
    fCurrentRenderPass = vkRT->externalRenderPass();
    SkASSERT(fCurrentRenderPass);
    fCurrentRenderPass->ref();

    fCurrentSecondaryCommandBuffer.reset(
            GrVkSecondaryCommandBuffer::Create(vkRT->getExternalSecondaryCommandBuffer()));
    if (!fCurrentSecondaryCommandBuffer) {
        return false;
    }
    fCurrentSecondaryCommandBuffer->begin(fGpu, nullptr, fCurrentRenderPass);
    return true;
}

GrVkOpsRenderPass::~GrVkOpsRenderPass() {
    this->reset();
}

GrGpu* GrVkOpsRenderPass::gpu() { return fGpu; }

GrVkCommandBuffer* GrVkOpsRenderPass::currentCommandBuffer() {
    if (fCurrentSecondaryCommandBuffer) {
        return fCurrentSecondaryCommandBuffer.get();
    }
    return fGpu->currentCommandBuffer();
}

void GrVkOpsRenderPass::submit() {
    if (!fRenderTarget) {
        return;
    }
    if (!fCurrentRenderPass) {
        SkASSERT(fGpu->isDeviceLost());
        return;
    }

    // We don't want to actually submit the secondary command buffer if it is wrapped.
    if (this->wrapsSecondaryCommandBuffer()) {
        // We pass the ownership of the GrVkSecondaryCommandBuffer to the special wrapped
        // GrVkRenderTarget since it's lifetime matches the lifetime we need to keep the
        // GrManagedResources on the GrVkSecondaryCommandBuffer alive.
        static_cast<GrVkRenderTarget*>(fRenderTarget)->addWrappedGrSecondaryCommandBuffer(
                std::move(fCurrentSecondaryCommandBuffer));
        return;
    }

    if (fCurrentSecondaryCommandBuffer) {
        fGpu->submitSecondaryCommandBuffer(std::move(fCurrentSecondaryCommandBuffer));
    }
    fGpu->endRenderPass(fRenderTarget, fOrigin, fBounds);
}

bool GrVkOpsRenderPass::set(GrRenderTarget* rt, GrSurfaceOrigin origin, const SkIRect& bounds,
                            const GrOpsRenderPass::LoadAndStoreInfo& colorInfo,
                            const GrOpsRenderPass::StencilLoadAndStoreInfo& stencilInfo,
                            const SkTArray<GrSurfaceProxy*, true>& sampledProxies) {
    SkASSERT(!fRenderTarget);
    SkASSERT(fGpu == rt->getContext()->priv().getGpu());

#ifdef SK_DEBUG
    fIsActive = true;
#endif

    this->INHERITED::set(rt, origin);

    for (int i = 0; i < sampledProxies.count(); ++i) {
        if (sampledProxies[i]->isInstantiated()) {
            SkASSERT(sampledProxies[i]->asTextureProxy());
            GrVkTexture* vkTex = static_cast<GrVkTexture*>(sampledProxies[i]->peekTexture());
            SkASSERT(vkTex);
            vkTex->setImageLayout(
                    fGpu, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, false);
        }
    }

    SkASSERT(bounds.isEmpty() || SkIRect::MakeWH(rt->width(), rt->height()).contains(bounds));
    fBounds = bounds;

    if (this->wrapsSecondaryCommandBuffer()) {
        return this->initWrapped();
    }

    return this->init(colorInfo, stencilInfo, colorInfo.fClearColor);
}

void GrVkOpsRenderPass::reset() {
    if (fCurrentSecondaryCommandBuffer) {
        // The active GrVkCommandPool on the GrVkGpu should still be the same pool we got the
        // secondary command buffer from since we haven't submitted any work yet.
        SkASSERT(fGpu->cmdPool());
        fCurrentSecondaryCommandBuffer.release()->recycle(fGpu->cmdPool());
    }
    if (fCurrentRenderPass) {
        fCurrentRenderPass->unref();
        fCurrentRenderPass = nullptr;
    }
    fCurrentCBIsEmpty = true;

    fRenderTarget = nullptr;

#ifdef SK_DEBUG
    fIsActive = false;
#endif
}

bool GrVkOpsRenderPass::wrapsSecondaryCommandBuffer() const {
    GrVkRenderTarget* vkRT = static_cast<GrVkRenderTarget*>(fRenderTarget);
    return vkRT->wrapsSecondaryCommandBuffer();
}

////////////////////////////////////////////////////////////////////////////////

void GrVkOpsRenderPass::onClearStencilClip(const GrFixedClip& clip, bool insideStencilMask) {
    if (!fCurrentRenderPass) {
        SkASSERT(fGpu->isDeviceLost());
        return;
    }

    SkASSERT(!clip.hasWindowRectangles());

    GrStencilAttachment* sb = fRenderTarget->renderTargetPriv().getStencilAttachment();
    // this should only be called internally when we know we have a
    // stencil buffer.
    SkASSERT(sb);
    int stencilBitCount = sb->bits();

    // The contract with the callers does not guarantee that we preserve all bits in the stencil
    // during this clear. Thus we will clear the entire stencil to the desired value.

    VkClearDepthStencilValue vkStencilColor;
    memset(&vkStencilColor, 0, sizeof(VkClearDepthStencilValue));
    if (insideStencilMask) {
        vkStencilColor.stencil = (1 << (stencilBitCount - 1));
    } else {
        vkStencilColor.stencil = 0;
    }

    VkClearRect clearRect;
    // Flip rect if necessary
    SkIRect vkRect;
    if (!clip.scissorEnabled()) {
        vkRect.setXYWH(0, 0, fRenderTarget->width(), fRenderTarget->height());
    } else if (kBottomLeft_GrSurfaceOrigin != fOrigin) {
        vkRect = clip.scissorRect();
    } else {
        const SkIRect& scissor = clip.scissorRect();
        vkRect.setLTRB(scissor.fLeft, fRenderTarget->height() - scissor.fBottom,
                       scissor.fRight, fRenderTarget->height() - scissor.fTop);
    }

    clearRect.rect.offset = { vkRect.fLeft, vkRect.fTop };
    clearRect.rect.extent = { (uint32_t)vkRect.width(), (uint32_t)vkRect.height() };

    clearRect.baseArrayLayer = 0;
    clearRect.layerCount = 1;

    uint32_t stencilIndex;
    SkAssertResult(fCurrentRenderPass->stencilAttachmentIndex(&stencilIndex));

    VkClearAttachment attachment;
    attachment.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    attachment.colorAttachment = 0; // this value shouldn't matter
    attachment.clearValue.depthStencil = vkStencilColor;

    this->currentCommandBuffer()->clearAttachments(fGpu, 1, &attachment, 1, &clearRect);
    fCurrentCBIsEmpty = false;
}

void GrVkOpsRenderPass::onClear(const GrFixedClip& clip, const SkPMColor4f& color) {
    if (!fCurrentRenderPass) {
        SkASSERT(fGpu->isDeviceLost());
        return;
    }

    // parent class should never let us get here with no RT
    SkASSERT(!clip.hasWindowRectangles());

    VkClearColorValue vkColor = {{color.fR, color.fG, color.fB, color.fA}};

    // If we end up in a situation where we are calling clear without a scissior then in general it
    // means we missed an opportunity higher up the stack to set the load op to be a clear. However,
    // there are situations where higher up we couldn't discard the previous ops and set a clear
    // load op (e.g. if we needed to execute a wait op). Thus we also have the empty check here.
    // TODO: Make the waitOp a RenderTask instead so we can clear out the GrOpsTask for a clear. We
    // can then reenable this assert assuming we can't get messed up by a waitOp.
    //SkASSERT(!fCurrentCBIsEmpty || clip.scissorEnabled());

    // We always do a sub rect clear with clearAttachments since we are inside a render pass
    VkClearRect clearRect;
    // Flip rect if necessary
    SkIRect vkRect;
    if (!clip.scissorEnabled()) {
        vkRect.setXYWH(0, 0, fRenderTarget->width(), fRenderTarget->height());
    } else if (kBottomLeft_GrSurfaceOrigin != fOrigin) {
        vkRect = clip.scissorRect();
    } else {
        const SkIRect& scissor = clip.scissorRect();
        vkRect.setLTRB(scissor.fLeft, fRenderTarget->height() - scissor.fBottom,
                       scissor.fRight, fRenderTarget->height() - scissor.fTop);
    }
    clearRect.rect.offset = { vkRect.fLeft, vkRect.fTop };
    clearRect.rect.extent = { (uint32_t)vkRect.width(), (uint32_t)vkRect.height() };
    clearRect.baseArrayLayer = 0;
    clearRect.layerCount = 1;

    uint32_t colorIndex;
    SkAssertResult(fCurrentRenderPass->colorAttachmentIndex(&colorIndex));

    VkClearAttachment attachment;
    attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    attachment.colorAttachment = colorIndex;
    attachment.clearValue.color = vkColor;

    this->currentCommandBuffer()->clearAttachments(fGpu, 1, &attachment, 1, &clearRect);
    fCurrentCBIsEmpty = false;
    return;
}

////////////////////////////////////////////////////////////////////////////////

void GrVkOpsRenderPass::addAdditionalRenderPass(bool mustUseSecondaryCommandBuffer) {
    SkASSERT(!this->wrapsSecondaryCommandBuffer());
    GrVkRenderTarget* vkRT = static_cast<GrVkRenderTarget*>(fRenderTarget);

    GrVkRenderPass::LoadStoreOps vkColorOps(VK_ATTACHMENT_LOAD_OP_LOAD,
                                            VK_ATTACHMENT_STORE_OP_STORE);
    GrVkRenderPass::LoadStoreOps vkStencilOps(VK_ATTACHMENT_LOAD_OP_LOAD,
                                              VK_ATTACHMENT_STORE_OP_STORE);

    const GrVkResourceProvider::CompatibleRPHandle& rpHandle =
            vkRT->compatibleRenderPassHandle();
    SkASSERT(fCurrentRenderPass);
    fCurrentRenderPass->unref();
    if (rpHandle.isValid()) {
        fCurrentRenderPass = fGpu->resourceProvider().findRenderPass(rpHandle,
                                                                     vkColorOps,
                                                                     vkStencilOps);
    } else {
        fCurrentRenderPass = fGpu->resourceProvider().findRenderPass(vkRT,
                                                                     vkColorOps,
                                                                     vkStencilOps);
    }
    if (!fCurrentRenderPass) {
        return;
    }

    VkClearValue vkClearColor;
    memset(&vkClearColor, 0, sizeof(VkClearValue));

    if (!fGpu->vkCaps().preferPrimaryOverSecondaryCommandBuffers() ||
        mustUseSecondaryCommandBuffer) {
        SkASSERT(fGpu->cmdPool());
        fCurrentSecondaryCommandBuffer = fGpu->cmdPool()->findOrCreateSecondaryCommandBuffer(fGpu);
        if (!fCurrentSecondaryCommandBuffer) {
            fCurrentRenderPass = nullptr;
            return;
        }
        fCurrentSecondaryCommandBuffer->begin(fGpu, vkRT->getFramebuffer(), fCurrentRenderPass);
    }

    // We use the same fBounds as the whole GrVkOpsRenderPass since we have no way of tracking the
    // bounds in GrOpsTask for parts before and after inline uploads separately.
    if (!fGpu->beginRenderPass(fCurrentRenderPass, &vkClearColor, vkRT, fOrigin, fBounds,
                               SkToBool(fCurrentSecondaryCommandBuffer))) {
        if (fCurrentSecondaryCommandBuffer) {
            fCurrentSecondaryCommandBuffer->end(fGpu);
        }
        fCurrentRenderPass = nullptr;
    }
}

void GrVkOpsRenderPass::inlineUpload(GrOpFlushState* state, GrDeferredTextureUploadFn& upload) {
    if (!fCurrentRenderPass) {
        SkASSERT(fGpu->isDeviceLost());
        return;
    }
    if (fCurrentSecondaryCommandBuffer) {
        fCurrentSecondaryCommandBuffer->end(fGpu);
        fGpu->submitSecondaryCommandBuffer(std::move(fCurrentSecondaryCommandBuffer));
    }
    fGpu->endRenderPass(fRenderTarget, fOrigin, fBounds);

    // We pass in true here to signal that after the upload we need to set the upload textures
    // layout back to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
    state->doUpload(upload, true);

    this->addAdditionalRenderPass(false);
}

////////////////////////////////////////////////////////////////////////////////

void GrVkOpsRenderPass::onEnd() {
    if (fCurrentSecondaryCommandBuffer) {
        fCurrentSecondaryCommandBuffer->end(fGpu);
    }
}

bool GrVkOpsRenderPass::onBindPipeline(const GrProgramInfo& programInfo, const SkRect& drawBounds) {
    if (!fCurrentRenderPass) {
        SkASSERT(fGpu->isDeviceLost());
        return false;
    }

    SkRect rtRect = SkRect::Make(fBounds);
    if (rtRect.intersect(drawBounds)) {
        rtRect.roundOut(&fCurrentPipelineBounds);
    } else {
        fCurrentPipelineBounds.setEmpty();
    }

    GrVkCommandBuffer* currentCB = this->currentCommandBuffer();
    SkASSERT(fCurrentRenderPass);

    VkRenderPass compatibleRenderPass = fCurrentRenderPass->vkRenderPass();

    fCurrentPipelineState = fGpu->resourceProvider().findOrCreateCompatiblePipelineState(
            fRenderTarget, programInfo, compatibleRenderPass);
    if (!fCurrentPipelineState) {
        return false;
    }

    fCurrentPipelineState->bindPipeline(fGpu, currentCB);

    // Both the 'programInfo' and this renderPass have an origin. Since they come from the
    // same place (i.e., the target renderTargetProxy) they had best agree.
    SkASSERT(programInfo.origin() == fOrigin);

    if (!fCurrentPipelineState->setAndBindUniforms(fGpu, fRenderTarget, programInfo, currentCB)) {
        return false;
    }

    if (!programInfo.pipeline().isScissorTestEnabled()) {
        // "Disable" scissor by setting it to the full pipeline bounds.
        GrVkPipeline::SetDynamicScissorRectState(fGpu, currentCB, fRenderTarget, fOrigin,
                                                 fCurrentPipelineBounds);
    }
    GrVkPipeline::SetDynamicViewportState(fGpu, currentCB, fRenderTarget);
    GrVkPipeline::SetDynamicBlendConstantState(fGpu, currentCB,
                                               programInfo.pipeline().writeSwizzle(),
                                               programInfo.pipeline().getXferProcessor());

    return true;
}

void GrVkOpsRenderPass::onSetScissorRect(const SkIRect& scissor) {
    SkIRect combinedScissorRect;
    if (!combinedScissorRect.intersect(fCurrentPipelineBounds, scissor)) {
        combinedScissorRect = SkIRect::MakeEmpty();
    }
    GrVkPipeline::SetDynamicScissorRectState(fGpu, this->currentCommandBuffer(), fRenderTarget,
                                             fOrigin, combinedScissorRect);
}

#ifdef SK_DEBUG
void check_sampled_texture(GrTexture* tex, GrRenderTarget* rt, GrVkGpu* gpu) {
    SkASSERT(!tex->isProtected() || (rt->isProtected() && gpu->protectedContext()));
    GrVkTexture* vkTex = static_cast<GrVkTexture*>(tex);
    SkASSERT(vkTex->currentLayout() == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
#endif

bool GrVkOpsRenderPass::onBindTextures(const GrPrimitiveProcessor& primProc,
                                       const GrSurfaceProxy* const primProcTextures[],
                                       const GrPipeline& pipeline) {
#ifdef SK_DEBUG
    SkASSERT(fCurrentPipelineState);
    for (int i = 0; i < primProc.numTextureSamplers(); ++i) {
        check_sampled_texture(primProcTextures[i]->peekTexture(), fRenderTarget, fGpu);
    }
    GrFragmentProcessor::PipelineTextureSamplerRange textureSamplerRange(pipeline);
    for (auto [sampler, fp] : textureSamplerRange) {
        check_sampled_texture(sampler.peekTexture(), fRenderTarget, fGpu);
    }
    if (GrTexture* dstTexture = pipeline.peekDstTexture()) {
        check_sampled_texture(dstTexture, fRenderTarget, fGpu);
    }
#endif
    return fCurrentPipelineState->setAndBindTextures(fGpu, primProc, pipeline, primProcTextures,
                                                     this->currentCommandBuffer());
}

void GrVkOpsRenderPass::onBindBuffers(const GrBuffer* indexBuffer, const GrBuffer* instanceBuffer,
                                      const GrBuffer* vertexBuffer,
                                      GrPrimitiveRestart primRestart) {
    SkASSERT(GrPrimitiveRestart::kNo == primRestart);
    if (!fCurrentRenderPass) {
        SkASSERT(fGpu->isDeviceLost());
        return;
    }
    SkASSERT(fCurrentPipelineState);
    SkASSERT(!fGpu->caps()->usePrimitiveRestart());  // Ignore primitiveRestart parameter.

    GrVkCommandBuffer* currCmdBuf = this->currentCommandBuffer();
    SkASSERT(currCmdBuf);

    // There is no need to put any memory barriers to make sure host writes have finished here.
    // When a command buffer is submitted to a queue, there is an implicit memory barrier that
    // occurs for all host writes. Additionally, BufferMemoryBarriers are not allowed inside of
    // an active RenderPass.

    // Here our vertex and instance inputs need to match the same 0-based bindings they were
    // assigned in GrVkPipeline. That is, vertex first (if any) followed by instance.
    uint32_t binding = 0;
    if (auto* vkVertexBuffer = static_cast<const GrVkMeshBuffer*>(vertexBuffer)) {
        SkASSERT(!vkVertexBuffer->isCpuBuffer());
        SkASSERT(!vkVertexBuffer->isMapped());
        currCmdBuf->bindInputBuffer(fGpu, binding++, vkVertexBuffer);
    }
    if (auto* vkInstanceBuffer = static_cast<const GrVkMeshBuffer*>(instanceBuffer)) {
        SkASSERT(!vkInstanceBuffer->isCpuBuffer());
        SkASSERT(!vkInstanceBuffer->isMapped());
        currCmdBuf->bindInputBuffer(fGpu, binding++, vkInstanceBuffer);
    }
    if (auto* vkIndexBuffer = static_cast<const GrVkMeshBuffer*>(indexBuffer)) {
        SkASSERT(!vkIndexBuffer->isCpuBuffer());
        SkASSERT(!vkIndexBuffer->isMapped());
        currCmdBuf->bindIndexBuffer(fGpu, vkIndexBuffer);
    }
}

void GrVkOpsRenderPass::onDrawInstanced(int instanceCount,
                                        int baseInstance,
                                        int vertexCount, int baseVertex) {
    if (!fCurrentRenderPass) {
        SkASSERT(fGpu->isDeviceLost());
        return;
    }
    SkASSERT(fCurrentPipelineState);
    this->currentCommandBuffer()->draw(fGpu, vertexCount, instanceCount, baseVertex, baseInstance);
    fGpu->stats()->incNumDraws();
    fCurrentCBIsEmpty = false;
}

void GrVkOpsRenderPass::onDrawIndexedInstanced(int indexCount, int baseIndex, int instanceCount,
                                               int baseInstance, int baseVertex) {
    if (!fCurrentRenderPass) {
        SkASSERT(fGpu->isDeviceLost());
        return;
    }
    SkASSERT(fCurrentPipelineState);
    this->currentCommandBuffer()->drawIndexed(fGpu, indexCount, instanceCount,
                                              baseIndex, baseVertex, baseInstance);
    fGpu->stats()->incNumDraws();
    fCurrentCBIsEmpty = false;
}

void GrVkOpsRenderPass::onDrawIndirect(const GrBuffer* drawIndirectBuffer, size_t offset,
                                       int drawCount) {
    SkASSERT(!drawIndirectBuffer->isCpuBuffer());
    if (!fCurrentRenderPass) {
        SkASSERT(fGpu->isDeviceLost());
        return;
    }
    SkASSERT(fCurrentPipelineState);
    this->currentCommandBuffer()->drawIndirect(
            fGpu, static_cast<const GrVkMeshBuffer*>(drawIndirectBuffer), offset, drawCount,
            sizeof(GrDrawIndirectCommand));
    fGpu->stats()->incNumDraws();
    fCurrentCBIsEmpty = false;
}

void GrVkOpsRenderPass::onDrawIndexedIndirect(const GrBuffer* drawIndirectBuffer, size_t offset,
                                              int drawCount) {
    SkASSERT(!drawIndirectBuffer->isCpuBuffer());
    if (!fCurrentRenderPass) {
        SkASSERT(fGpu->isDeviceLost());
        return;
    }
    SkASSERT(fCurrentPipelineState);
    this->currentCommandBuffer()->drawIndexedIndirect(
            fGpu, static_cast<const GrVkMeshBuffer*>(drawIndirectBuffer), offset, drawCount,
            sizeof(GrDrawIndexedIndirectCommand));
    fGpu->stats()->incNumDraws();
    fCurrentCBIsEmpty = false;
}

////////////////////////////////////////////////////////////////////////////////

void GrVkOpsRenderPass::onExecuteDrawable(std::unique_ptr<SkDrawable::GpuDrawHandler> drawable) {
    if (!fCurrentRenderPass) {
        SkASSERT(fGpu->isDeviceLost());
        return;
    }
    GrVkRenderTarget* target = static_cast<GrVkRenderTarget*>(fRenderTarget);

    GrVkImage* targetImage = target->msaaImage() ? target->msaaImage() : target;

    VkRect2D bounds;
    bounds.offset = { 0, 0 };
    bounds.extent = { 0, 0 };

    if (!fCurrentSecondaryCommandBuffer) {
        fGpu->endRenderPass(fRenderTarget, fOrigin, fBounds);
        this->addAdditionalRenderPass(true);
        // We may have failed to start a new render pass
        if (!fCurrentRenderPass) {
            SkASSERT(fGpu->isDeviceLost());
            return;
        }
    }
    SkASSERT(fCurrentSecondaryCommandBuffer);

    GrVkDrawableInfo vkInfo;
    vkInfo.fSecondaryCommandBuffer = fCurrentSecondaryCommandBuffer->vkCommandBuffer();
    vkInfo.fCompatibleRenderPass = fCurrentRenderPass->vkRenderPass();
    SkAssertResult(fCurrentRenderPass->colorAttachmentIndex(&vkInfo.fColorAttachmentIndex));
    vkInfo.fFormat = targetImage->imageFormat();
    vkInfo.fDrawBounds = &bounds;
#ifdef SK_BUILD_FOR_ANDROID_FRAMEWORK
    vkInfo.fImage = targetImage->image();
#else
    vkInfo.fImage = VK_NULL_HANDLE;
#endif //SK_BUILD_FOR_ANDROID_FRAMEWORK

    GrBackendDrawableInfo info(vkInfo);

    // After we draw into the command buffer via the drawable, cached state we have may be invalid.
    this->currentCommandBuffer()->invalidateState();
    // Also assume that the drawable produced output.
    fCurrentCBIsEmpty = false;

    drawable->draw(info);
    fGpu->addDrawable(std::move(drawable));
}

