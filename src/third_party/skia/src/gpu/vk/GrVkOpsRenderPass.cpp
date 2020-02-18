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
#include "src/gpu/GrMesh.h"
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

GrVkPrimaryCommandBufferTask::~GrVkPrimaryCommandBufferTask() = default;
GrVkPrimaryCommandBufferTask::GrVkPrimaryCommandBufferTask() = default;

namespace {

class InlineUpload : public GrVkPrimaryCommandBufferTask {
public:
    InlineUpload(GrOpFlushState* state, const GrDeferredTextureUploadFn& upload)
            : fFlushState(state), fUpload(upload) {}

    void execute(const Args& args) override { fFlushState->doUpload(fUpload, true); }

private:
    GrOpFlushState* fFlushState;
    GrDeferredTextureUploadFn fUpload;
};

}  // anonymous namespace

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

void GrVkOpsRenderPass::init() {
    GrVkRenderPass::LoadStoreOps vkColorOps(fVkColorLoadOp, fVkColorStoreOp);
    GrVkRenderPass::LoadStoreOps vkStencilOps(fVkStencilLoadOp, fVkStencilStoreOp);

    CommandBufferInfo& cbInfo = fCommandBufferInfos.push_back();
    SkASSERT(fCommandBufferInfos.count() == 1);
    fCurrentCmdInfo = 0;

    GrVkRenderTarget* vkRT = static_cast<GrVkRenderTarget*>(fRenderTarget);
    const GrVkResourceProvider::CompatibleRPHandle& rpHandle = vkRT->compatibleRenderPassHandle();
    if (rpHandle.isValid()) {
        cbInfo.fRenderPass = fGpu->resourceProvider().findRenderPass(rpHandle,
                                                                     vkColorOps,
                                                                     vkStencilOps);
    } else {
        cbInfo.fRenderPass = fGpu->resourceProvider().findRenderPass(*vkRT,
                                                                     vkColorOps,
                                                                     vkStencilOps);
    }

    cbInfo.fColorClearValue.color.float32[0] = fClearColor[0];
    cbInfo.fColorClearValue.color.float32[1] = fClearColor[1];
    cbInfo.fColorClearValue.color.float32[2] = fClearColor[2];
    cbInfo.fColorClearValue.color.float32[3] = fClearColor[3];

    if (VK_ATTACHMENT_LOAD_OP_CLEAR == fVkColorLoadOp) {
        cbInfo.fBounds = SkRect::MakeWH(vkRT->width(), vkRT->height());
    } else {
        cbInfo.fBounds.setEmpty();
    }

    if (VK_ATTACHMENT_LOAD_OP_CLEAR == fVkColorLoadOp) {
        cbInfo.fLoadStoreState = LoadStoreState::kStartsWithClear;
    } else if (VK_ATTACHMENT_LOAD_OP_LOAD == fVkColorLoadOp &&
               VK_ATTACHMENT_STORE_OP_STORE == fVkColorStoreOp) {
        cbInfo.fLoadStoreState = LoadStoreState::kLoadAndStore;
    } else if (VK_ATTACHMENT_LOAD_OP_DONT_CARE == fVkColorLoadOp) {
        cbInfo.fLoadStoreState = LoadStoreState::kStartsWithDiscard;
    }

    cbInfo.fCommandBuffer = fGpu->cmdPool()->findOrCreateSecondaryCommandBuffer(fGpu);
    cbInfo.currentCmdBuf()->begin(fGpu, vkRT->framebuffer(), cbInfo.fRenderPass);
}

void GrVkOpsRenderPass::initWrapped() {
    CommandBufferInfo& cbInfo = fCommandBufferInfos.push_back();
    SkASSERT(fCommandBufferInfos.count() == 1);
    fCurrentCmdInfo = 0;

    GrVkRenderTarget* vkRT = static_cast<GrVkRenderTarget*>(fRenderTarget);
    SkASSERT(vkRT->wrapsSecondaryCommandBuffer());
    cbInfo.fRenderPass = vkRT->externalRenderPass();
    cbInfo.fRenderPass->ref();

    cbInfo.fBounds.setEmpty();
    cbInfo.fCommandBuffer.reset(
            GrVkSecondaryCommandBuffer::Create(vkRT->getExternalSecondaryCommandBuffer()));
    cbInfo.currentCmdBuf()->begin(fGpu, nullptr, cbInfo.fRenderPass);
}

GrVkOpsRenderPass::~GrVkOpsRenderPass() {
    this->reset();
}

GrGpu* GrVkOpsRenderPass::gpu() { return fGpu; }

void GrVkOpsRenderPass::end() {
    if (fCurrentCmdInfo >= 0) {
        fCommandBufferInfos[fCurrentCmdInfo].currentCmdBuf()->end(fGpu);
    }
}

void GrVkOpsRenderPass::submit() {
    if (!fRenderTarget) {
        return;
    }

    GrVkRenderTarget* vkRT = static_cast<GrVkRenderTarget*>(fRenderTarget);
    GrVkImage* targetImage = vkRT->msaaImage() ? vkRT->msaaImage() : vkRT;
    GrStencilAttachment* stencil = fRenderTarget->renderTargetPriv().getStencilAttachment();
    auto currPreCmd = fPreCommandBufferTasks.begin();

    GrVkPrimaryCommandBufferTask::Args taskArgs{fGpu, fRenderTarget};
    for (int i = 0; i < fCommandBufferInfos.count(); ++i) {
        CommandBufferInfo& cbInfo = fCommandBufferInfos[i];

        for (int c = 0; c < cbInfo.fNumPreCmds; ++c, ++currPreCmd) {
            currPreCmd->execute(taskArgs);
        }

        // TODO: Many things create a scratch texture which adds the discard immediately, but then
        // don't draw to it right away. This causes the discard to be ignored and we get yelled at
        // for loading uninitialized data. However, once MDB lands with reordering, the discard will
        // get reordered with the rest of the draw commands and we can remove the discard check.
        if (cbInfo.fIsEmpty &&
            cbInfo.fLoadStoreState != LoadStoreState::kStartsWithClear &&
            cbInfo.fLoadStoreState != LoadStoreState::kStartsWithDiscard) {
            // We have sumbitted no actual draw commands to the command buffer and we are not using
            // the render pass to do a clear so there is no need to submit anything.
            continue;
        }

        // We don't want to actually submit the secondary command buffer if it is wrapped.
        if (this->wrapsSecondaryCommandBuffer()) {
            // There should have only been one secondary command buffer in the wrapped case so it is
            // safe to just return here.
            SkASSERT(fCommandBufferInfos.count() == 1);
            return;
        }

        // Make sure if we only have a discard load that we execute the discard on the whole image.
        // TODO: Once we improve our tracking of discards so that we never end up flushing a discard
        // call with no actually ops, remove this.
        if (cbInfo.fIsEmpty && cbInfo.fLoadStoreState == LoadStoreState::kStartsWithDiscard) {
            cbInfo.fBounds = SkRect::MakeWH(vkRT->width(), vkRT->height());
        }

        if (cbInfo.fBounds.intersect(SkRect::MakeIWH(fRenderTarget->width(),
                                                     fRenderTarget->height()))) {
            // Make sure we do the following layout changes after all copies, uploads, or any other
            // pre-work is done since we may change the layouts in the pre-work. Also since the
            // draws will be submitted in different render passes, we need to guard againts write
            // and write issues.

            // Change layout of our render target so it can be used as the color attachment.
            // TODO: If we know that we will never be blending or loading the attachment we could
            // drop the VK_ACCESS_COLOR_ATTACHMENT_READ_BIT.
            targetImage->setImageLayout(fGpu,
                                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                        false);

            // If we are using a stencil attachment we also need to update its layout
            if (stencil) {
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

            SkIRect iBounds;
            cbInfo.fBounds.roundOut(&iBounds);

            fGpu->submitSecondaryCommandBuffer(std::move(cbInfo.fCommandBuffer), cbInfo.fRenderPass,
                                               &cbInfo.fColorClearValue, vkRT, fOrigin, iBounds);
        }
    }
    SkASSERT(currPreCmd == fPreCommandBufferTasks.end());
}

void GrVkOpsRenderPass::set(GrRenderTarget* rt, GrSurfaceOrigin origin,
                            const GrOpsRenderPass::LoadAndStoreInfo& colorInfo,
                            const GrOpsRenderPass::StencilLoadAndStoreInfo& stencilInfo,
                            const SkTArray<GrTextureProxy*, true>& sampledProxies) {
    SkASSERT(!fRenderTarget);
    SkASSERT(fCommandBufferInfos.empty());
    SkASSERT(-1 == fCurrentCmdInfo);
    SkASSERT(fGpu == rt->getContext()->priv().getGpu());
    SkASSERT(!fLastPipelineState);

#ifdef SK_DEBUG
    fIsActive = true;
#endif

    this->INHERITED::set(rt, origin);

    for (int i = 0; i < sampledProxies.count(); ++i) {
        if (sampledProxies[i]->isInstantiated()) {
            GrVkTexture* vkTex = static_cast<GrVkTexture*>(sampledProxies[i]->peekTexture());
            SkASSERT(vkTex);
            vkTex->setImageLayout(
                    fGpu, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, false);
        }
    }

    if (this->wrapsSecondaryCommandBuffer()) {
        this->initWrapped();
        return;
    }

    fClearColor = colorInfo.fClearColor;

    get_vk_load_store_ops(colorInfo.fLoadOp, colorInfo.fStoreOp,
                          &fVkColorLoadOp, &fVkColorStoreOp);

    get_vk_load_store_ops(stencilInfo.fLoadOp, stencilInfo.fStoreOp,
                          &fVkStencilLoadOp, &fVkStencilStoreOp);

    this->init();
}

void GrVkOpsRenderPass::reset() {
    for (int i = 0; i < fCommandBufferInfos.count(); ++i) {
        CommandBufferInfo& cbInfo = fCommandBufferInfos[i];
        if (cbInfo.fCommandBuffer) {
            cbInfo.fCommandBuffer.release()->recycle(fGpu);
        }
        cbInfo.fRenderPass->unref(fGpu);
    }
    fCommandBufferInfos.reset();
    fPreCommandBufferTasks.reset();

    fCurrentCmdInfo = -1;

    fLastPipelineState = nullptr;
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

void GrVkOpsRenderPass::insertEventMarker(const char* msg) {
    // TODO: does Vulkan have a correlate?
}

void GrVkOpsRenderPass::onClearStencilClip(const GrFixedClip& clip, bool insideStencilMask) {
    SkASSERT(!clip.hasWindowRectangles());

    CommandBufferInfo& cbInfo = fCommandBufferInfos[fCurrentCmdInfo];

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
    SkAssertResult(cbInfo.fRenderPass->stencilAttachmentIndex(&stencilIndex));

    VkClearAttachment attachment;
    attachment.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    attachment.colorAttachment = 0; // this value shouldn't matter
    attachment.clearValue.depthStencil = vkStencilColor;

    cbInfo.currentCmdBuf()->clearAttachments(fGpu, 1, &attachment, 1, &clearRect);
    cbInfo.fIsEmpty = false;

    // Update command buffer bounds
    if (!clip.scissorEnabled()) {
        cbInfo.fBounds.join(fRenderTarget->getBoundsRect());
    } else {
        cbInfo.fBounds.join(SkRect::Make(clip.scissorRect()));
    }
}

void GrVkOpsRenderPass::onClear(const GrFixedClip& clip, const SkPMColor4f& color) {
    // parent class should never let us get here with no RT
    SkASSERT(!clip.hasWindowRectangles());

    CommandBufferInfo& cbInfo = fCommandBufferInfos[fCurrentCmdInfo];

    VkClearColorValue vkColor = {{color.fR, color.fG, color.fB, color.fA}};

    // If we end up in a situation where we are calling clear without a scissior then in general it
    // means we missed an opportunity higher up the stack to set the load op to be a clear. However,
    // there are situations where higher up we couldn't discard the previous ops and set a clear
    // load op (e.g. if we needed to execute a wait op). Thus we also have the empty check here.
    // TODO: Make the waitOp a RenderTask instead so we can clear out the GrOpsTask for a clear. We
    // can then reenable this assert assuming we can't get messed up by a waitOp.
    //SkASSERT(!cbInfo.fIsEmpty || clip.scissorEnabled());

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
    SkAssertResult(cbInfo.fRenderPass->colorAttachmentIndex(&colorIndex));

    VkClearAttachment attachment;
    attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    attachment.colorAttachment = colorIndex;
    attachment.clearValue.color = vkColor;

    cbInfo.currentCmdBuf()->clearAttachments(fGpu, 1, &attachment, 1, &clearRect);
    cbInfo.fIsEmpty = false;

    // Update command buffer bounds
    if (!clip.scissorEnabled()) {
        cbInfo.fBounds.join(fRenderTarget->getBoundsRect());
    } else {
        cbInfo.fBounds.join(SkRect::Make(clip.scissorRect()));
    }
    return;
}

////////////////////////////////////////////////////////////////////////////////

void GrVkOpsRenderPass::addAdditionalRenderPass() {
    GrVkRenderTarget* vkRT = static_cast<GrVkRenderTarget*>(fRenderTarget);

    fCommandBufferInfos[fCurrentCmdInfo].currentCmdBuf()->end(fGpu);

    CommandBufferInfo& cbInfo = fCommandBufferInfos.push_back();
    fCurrentCmdInfo++;

    GrVkRenderPass::LoadStoreOps vkColorOps(VK_ATTACHMENT_LOAD_OP_LOAD,
                                            VK_ATTACHMENT_STORE_OP_STORE);
    GrVkRenderPass::LoadStoreOps vkStencilOps(VK_ATTACHMENT_LOAD_OP_LOAD,
                                              VK_ATTACHMENT_STORE_OP_STORE);

    const GrVkResourceProvider::CompatibleRPHandle& rpHandle =
            vkRT->compatibleRenderPassHandle();
    if (rpHandle.isValid()) {
        cbInfo.fRenderPass = fGpu->resourceProvider().findRenderPass(rpHandle,
                                                                     vkColorOps,
                                                                     vkStencilOps);
    } else {
        cbInfo.fRenderPass = fGpu->resourceProvider().findRenderPass(*vkRT,
                                                                     vkColorOps,
                                                                     vkStencilOps);
    }
    cbInfo.fLoadStoreState = LoadStoreState::kLoadAndStore;

    cbInfo.fCommandBuffer = fGpu->cmdPool()->findOrCreateSecondaryCommandBuffer(fGpu);
    // It shouldn't matter what we set the clear color to here since we will assume loading of the
    // attachment.
    memset(&cbInfo.fColorClearValue, 0, sizeof(VkClearValue));
    cbInfo.fBounds.setEmpty();

    cbInfo.currentCmdBuf()->begin(fGpu, vkRT->framebuffer(), cbInfo.fRenderPass);
}

void GrVkOpsRenderPass::inlineUpload(GrOpFlushState* state,
                                          GrDeferredTextureUploadFn& upload) {
    if (!fCommandBufferInfos[fCurrentCmdInfo].fIsEmpty) {
        this->addAdditionalRenderPass();
    }

    fPreCommandBufferTasks.emplace<InlineUpload>(state, upload);
    ++fCommandBufferInfos[fCurrentCmdInfo].fNumPreCmds;
}

////////////////////////////////////////////////////////////////////////////////

void GrVkOpsRenderPass::bindGeometry(const GrGpuBuffer* indexBuffer,
                                          const GrGpuBuffer* vertexBuffer,
                                          const GrGpuBuffer* instanceBuffer) {
    GrVkSecondaryCommandBuffer* currCmdBuf = fCommandBufferInfos[fCurrentCmdInfo].currentCmdBuf();
    // There is no need to put any memory barriers to make sure host writes have finished here.
    // When a command buffer is submitted to a queue, there is an implicit memory barrier that
    // occurs for all host writes. Additionally, BufferMemoryBarriers are not allowed inside of
    // an active RenderPass.

    // Here our vertex and instance inputs need to match the same 0-based bindings they were
    // assigned in GrVkPipeline. That is, vertex first (if any) followed by instance.
    uint32_t binding = 0;

    if (vertexBuffer) {
        SkASSERT(vertexBuffer);
        SkASSERT(!vertexBuffer->isMapped());

        currCmdBuf->bindInputBuffer(fGpu, binding++,
                                    static_cast<const GrVkVertexBuffer*>(vertexBuffer));
    }

    if (instanceBuffer) {
        SkASSERT(instanceBuffer);
        SkASSERT(!instanceBuffer->isMapped());

        currCmdBuf->bindInputBuffer(fGpu, binding++,
                                    static_cast<const GrVkVertexBuffer*>(instanceBuffer));
    }
    if (indexBuffer) {
        SkASSERT(indexBuffer);
        SkASSERT(!indexBuffer->isMapped());

        currCmdBuf->bindIndexBuffer(fGpu, static_cast<const GrVkIndexBuffer*>(indexBuffer));
    }
}

GrVkPipelineState* GrVkOpsRenderPass::prepareDrawState(
        const GrPrimitiveProcessor& primProc,
        const GrPipeline& pipeline,
        const GrPipeline::FixedDynamicState* fixedDynamicState,
        const GrPipeline::DynamicStateArrays* dynamicStateArrays,
        GrPrimitiveType primitiveType) {
    CommandBufferInfo& cbInfo = fCommandBufferInfos[fCurrentCmdInfo];
    SkASSERT(cbInfo.fRenderPass);

    VkRenderPass compatibleRenderPass = cbInfo.fRenderPass->vkRenderPass();

    const GrTextureProxy* const* primProcProxies = nullptr;
    if (dynamicStateArrays && dynamicStateArrays->fPrimitiveProcessorTextures) {
        primProcProxies = dynamicStateArrays->fPrimitiveProcessorTextures;
    } else if (fixedDynamicState) {
        primProcProxies = fixedDynamicState->fPrimitiveProcessorTextures;
    }

    SkASSERT(SkToBool(primProcProxies) == SkToBool(primProc.numTextureSamplers()));

    GrVkPipelineState* pipelineState =
        fGpu->resourceProvider().findOrCreateCompatiblePipelineState(fRenderTarget, fOrigin,
                                                                     pipeline,
                                                                     primProc,
                                                                     primProcProxies,
                                                                     primitiveType,
                                                                     compatibleRenderPass);
    if (!pipelineState) {
        return pipelineState;
    }

    fLastPipelineState = pipelineState;

    pipelineState->bindPipeline(fGpu, cbInfo.currentCmdBuf());

    pipelineState->setAndBindUniforms(fGpu, fRenderTarget, fOrigin,
                                      primProc, pipeline, cbInfo.currentCmdBuf());

    // Check whether we need to bind textures between each GrMesh. If not we can bind them all now.
    bool setTextures = !(dynamicStateArrays && dynamicStateArrays->fPrimitiveProcessorTextures);
    if (setTextures) {
        pipelineState->setAndBindTextures(fGpu, primProc, pipeline, primProcProxies,
                                          cbInfo.currentCmdBuf());
    }

    if (!pipeline.isScissorEnabled()) {
        GrVkPipeline::SetDynamicScissorRectState(fGpu, cbInfo.currentCmdBuf(),
                                                 fRenderTarget, fOrigin,
                                                 SkIRect::MakeWH(fRenderTarget->width(),
                                                                 fRenderTarget->height()));
    } else if (!dynamicStateArrays || !dynamicStateArrays->fScissorRects) {
        SkASSERT(fixedDynamicState);
        GrVkPipeline::SetDynamicScissorRectState(fGpu, cbInfo.currentCmdBuf(), fRenderTarget,
                                                 fOrigin,
                                                 fixedDynamicState->fScissorRect);
    }
    GrVkPipeline::SetDynamicViewportState(fGpu, cbInfo.currentCmdBuf(), fRenderTarget);
    GrVkPipeline::SetDynamicBlendConstantState(fGpu, cbInfo.currentCmdBuf(),
                                               pipeline.outputSwizzle(),
                                               pipeline.getXferProcessor());

    return pipelineState;
}

#ifdef SK_DEBUG
void check_sampled_texture(GrTexture* tex, GrRenderTarget* rt, GrVkGpu* gpu) {
    SkASSERT(!tex->isProtected() || (rt->isProtected() && gpu->protectedContext()));
    GrVkTexture* vkTex = static_cast<GrVkTexture*>(tex);
    SkASSERT(vkTex->currentLayout() == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
#endif


void GrVkOpsRenderPass::onDraw(const GrPrimitiveProcessor& primProc,
                                    const GrPipeline& pipeline,
                                    const GrPipeline::FixedDynamicState* fixedDynamicState,
                                    const GrPipeline::DynamicStateArrays* dynamicStateArrays,
                                    const GrMesh meshes[],
                                    int meshCount,
                                    const SkRect& bounds) {
    if (!meshCount) {
        return;
    }

    CommandBufferInfo& cbInfo = fCommandBufferInfos[fCurrentCmdInfo];

#ifdef SK_DEBUG
    if (dynamicStateArrays && dynamicStateArrays->fPrimitiveProcessorTextures) {
        for (int m = 0, i = 0; m < meshCount; ++m) {
            for (int s = 0; s < primProc.numTextureSamplers(); ++s, ++i) {
                auto texture = dynamicStateArrays->fPrimitiveProcessorTextures[i]->peekTexture();
                check_sampled_texture(texture, fRenderTarget, fGpu);
            }
        }
    } else {
        for (int i = 0; i < primProc.numTextureSamplers(); ++i) {
            auto texture = fixedDynamicState->fPrimitiveProcessorTextures[i]->peekTexture();
            check_sampled_texture(texture, fRenderTarget, fGpu);
        }
    }
    GrFragmentProcessor::Iter iter(pipeline);
    while (const GrFragmentProcessor* fp = iter.next()) {
        for (int i = 0; i < fp->numTextureSamplers(); ++i) {
            const GrFragmentProcessor::TextureSampler& sampler = fp->textureSampler(i);
            check_sampled_texture(sampler.peekTexture(), fRenderTarget, fGpu);
        }
    }
    if (GrTexture* dstTexture = pipeline.peekDstTexture()) {
        check_sampled_texture(dstTexture, fRenderTarget, fGpu);
    }
#endif

    GrPrimitiveType primitiveType = meshes[0].primitiveType();
    GrVkPipelineState* pipelineState = this->prepareDrawState(primProc, pipeline, fixedDynamicState,
                                                              dynamicStateArrays, primitiveType);
    if (!pipelineState) {
        return;
    }

    bool dynamicScissor =
            pipeline.isScissorEnabled() && dynamicStateArrays && dynamicStateArrays->fScissorRects;
    bool dynamicTextures = dynamicStateArrays && dynamicStateArrays->fPrimitiveProcessorTextures;

    for (int i = 0; i < meshCount; ++i) {
        const GrMesh& mesh = meshes[i];
        if (mesh.primitiveType() != primitiveType) {
            SkDEBUGCODE(pipelineState = nullptr);
            primitiveType = mesh.primitiveType();
            pipelineState = this->prepareDrawState(primProc, pipeline, fixedDynamicState,
                                                   dynamicStateArrays, primitiveType);
            if (!pipelineState) {
                return;
            }
        }

        if (dynamicScissor) {
            GrVkPipeline::SetDynamicScissorRectState(fGpu, cbInfo.currentCmdBuf(), fRenderTarget,
                                                     fOrigin,
                                                     dynamicStateArrays->fScissorRects[i]);
        }
        if (dynamicTextures) {
            GrTextureProxy* const* meshProxies = dynamicStateArrays->fPrimitiveProcessorTextures +
                                                 primProc.numTextureSamplers() * i;
            pipelineState->setAndBindTextures(fGpu, primProc, pipeline, meshProxies,
                                              cbInfo.currentCmdBuf());
        }
        SkASSERT(pipelineState);
        mesh.sendToGpu(this);
    }

    cbInfo.fBounds.join(bounds);
    cbInfo.fIsEmpty = false;
}

void GrVkOpsRenderPass::sendInstancedMeshToGpu(GrPrimitiveType,
                                               const GrBuffer* vertexBuffer,
                                               int vertexCount,
                                               int baseVertex,
                                               const GrBuffer* instanceBuffer,
                                               int instanceCount,
                                               int baseInstance) {
    CommandBufferInfo& cbInfo = fCommandBufferInfos[fCurrentCmdInfo];
    SkASSERT(!vertexBuffer || !vertexBuffer->isCpuBuffer());
    SkASSERT(!instanceBuffer || !instanceBuffer->isCpuBuffer());
    auto gpuVertexBuffer = static_cast<const GrGpuBuffer*>(vertexBuffer);
    auto gpuInstanceBuffer = static_cast<const GrGpuBuffer*>(instanceBuffer);
    this->bindGeometry(nullptr, gpuVertexBuffer, gpuInstanceBuffer);
    cbInfo.currentCmdBuf()->draw(fGpu, vertexCount, instanceCount, baseVertex, baseInstance);
    fGpu->stats()->incNumDraws();
}

void GrVkOpsRenderPass::sendIndexedInstancedMeshToGpu(GrPrimitiveType,
                                                      const GrBuffer* indexBuffer,
                                                      int indexCount,
                                                      int baseIndex,
                                                      const GrBuffer* vertexBuffer,
                                                      int baseVertex,
                                                      const GrBuffer* instanceBuffer,
                                                      int instanceCount,
                                                      int baseInstance,
                                                      GrPrimitiveRestart restart) {
    SkASSERT(restart == GrPrimitiveRestart::kNo);
    CommandBufferInfo& cbInfo = fCommandBufferInfos[fCurrentCmdInfo];
    SkASSERT(!vertexBuffer || !vertexBuffer->isCpuBuffer());
    SkASSERT(!instanceBuffer || !instanceBuffer->isCpuBuffer());
    SkASSERT(!indexBuffer->isCpuBuffer());
    auto gpuIndexxBuffer = static_cast<const GrGpuBuffer*>(indexBuffer);
    auto gpuVertexBuffer = static_cast<const GrGpuBuffer*>(vertexBuffer);
    auto gpuInstanceBuffer = static_cast<const GrGpuBuffer*>(instanceBuffer);
    this->bindGeometry(gpuIndexxBuffer, gpuVertexBuffer, gpuInstanceBuffer);
    cbInfo.currentCmdBuf()->drawIndexed(fGpu, indexCount, instanceCount,
                                        baseIndex, baseVertex, baseInstance);
    fGpu->stats()->incNumDraws();
}

////////////////////////////////////////////////////////////////////////////////

void GrVkOpsRenderPass::executeDrawable(std::unique_ptr<SkDrawable::GpuDrawHandler> drawable) {
    GrVkRenderTarget* target = static_cast<GrVkRenderTarget*>(fRenderTarget);

    GrVkImage* targetImage = target->msaaImage() ? target->msaaImage() : target;

    CommandBufferInfo& cbInfo = fCommandBufferInfos[fCurrentCmdInfo];
    VkRect2D bounds;
    bounds.offset = { 0, 0 };
    bounds.extent = { 0, 0 };

    GrVkDrawableInfo vkInfo;
    vkInfo.fSecondaryCommandBuffer = cbInfo.currentCmdBuf()->vkCommandBuffer();
    vkInfo.fCompatibleRenderPass = cbInfo.fRenderPass->vkRenderPass();
    SkAssertResult(cbInfo.fRenderPass->colorAttachmentIndex(&vkInfo.fColorAttachmentIndex));
    vkInfo.fFormat = targetImage->imageFormat();
    vkInfo.fDrawBounds = &bounds;
#ifdef SK_BUILD_FOR_ANDROID_FRAMEWORK
    vkInfo.fImage = targetImage->image();
#else
    vkInfo.fImage = VK_NULL_HANDLE;
#endif //SK_BUILD_FOR_ANDROID_FRAMEWORK

    GrBackendDrawableInfo info(vkInfo);

    // After we draw into the command buffer via the drawable, cached state we have may be invalid.
    cbInfo.currentCmdBuf()->invalidateState();
    // Also assume that the drawable produced output.
    cbInfo.fIsEmpty = false;

    drawable->draw(info);
    fGpu->addDrawable(std::move(drawable));

    if (bounds.extent.width == 0 || bounds.extent.height == 0) {
        cbInfo.fBounds.join(target->getBoundsRect());
    } else {
        cbInfo.fBounds.join(SkRect::MakeXYWH(bounds.offset.x, bounds.offset.y,
                                             bounds.extent.width, bounds.extent.height));
    }
}

