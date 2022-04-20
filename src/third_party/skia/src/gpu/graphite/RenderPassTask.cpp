/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/RenderPassTask.h"

#include "src/gpu/graphite/CommandBuffer.h"
#include "src/gpu/graphite/ContextPriv.h"
#include "src/gpu/graphite/DrawPass.h"
#include "src/gpu/graphite/Log.h"
#include "src/gpu/graphite/ResourceProvider.h"
#include "src/gpu/graphite/Texture.h"
#include "src/gpu/graphite/TextureProxy.h"

namespace skgpu::graphite {

sk_sp<RenderPassTask> RenderPassTask::Make(std::vector<std::unique_ptr<DrawPass>> passes,
                                           const RenderPassDesc& desc,
                                           sk_sp<TextureProxy> target) {
    // For now we have one DrawPass per RenderPassTask
    SkASSERT(passes.size() == 1);

    return sk_sp<RenderPassTask>(new RenderPassTask(std::move(passes), desc, target));
}

RenderPassTask::RenderPassTask(std::vector<std::unique_ptr<DrawPass>> passes,
                               const RenderPassDesc& desc,
                               sk_sp<TextureProxy> target)
        : fDrawPasses(std::move(passes))
        , fRenderPassDesc(desc)
        , fTarget(std::move(target)) {}

RenderPassTask::~RenderPassTask() = default;

bool RenderPassTask::addCommands(ResourceProvider* resourceProvider, CommandBuffer* commandBuffer) {
    // TBD: Expose the surfaces that will need to be attached within the renderpass?

    // TODO: for task execution, start the render pass, then iterate passes and
    // possibly(?) start each subpass, and call DrawPass::addCommands() on the command buffer
    // provided to the task. Then close the render pass and we should have pixels..

    // Instantiate the target
    if (fTarget) {
        if (!fTarget->instantiate(resourceProvider)) {
            SKGPU_LOG_W("Given invalid texture proxy. Will not create renderpass!");
            SKGPU_LOG_W("Dimensions are (%d, %d).",
                        fTarget->dimensions().width(), fTarget->dimensions().height());
            return false;
        }
    }

    sk_sp<Texture> depthStencilTexture;
    if (fRenderPassDesc.fDepthStencilAttachment.fTextureInfo.isValid()) {
        // TODO: ensure this is a scratch/recycled texture
        depthStencilTexture = resourceProvider->findOrCreateDepthStencilAttachment(
                fTarget->dimensions(), fRenderPassDesc.fDepthStencilAttachment.fTextureInfo);
        if (!depthStencilTexture) {
            SKGPU_LOG_W("Could not get DepthStencil attachment for RenderPassTask");
            return false;
        }
    }

    if (commandBuffer->beginRenderPass(fRenderPassDesc, fTarget->refTexture(), nullptr,
                                       std::move(depthStencilTexture))) {
        // Assuming one draw pass per renderpasstask for now
        SkASSERT(fDrawPasses.size() == 1);
        for (const auto& drawPass: fDrawPasses) {
            if (!drawPass->addCommands(resourceProvider, commandBuffer, fRenderPassDesc)) {
                commandBuffer->endRenderPass();
                return false;
            }
        }

        commandBuffer->endRenderPass();
    }

    return true;
}

} // namespace skgpu::graphite
