/*
 * Copyright 2019 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/GrTextureResolveRenderTask.h"

#include "src/gpu/GrGpu.h"
#include "src/gpu/GrMemoryPool.h"
#include "src/gpu/GrOpFlushState.h"
#include "src/gpu/GrRenderTarget.h"
#include "src/gpu/GrResourceAllocator.h"
#include "src/gpu/GrTexturePriv.h"

void GrTextureResolveRenderTask::init(const GrCaps& caps) {
    if (GrSurfaceProxy::ResolveFlags::kMSAA & fResolveFlags) {
        GrRenderTargetProxy* renderTargetProxy = fTarget->asRenderTargetProxy();
        SkASSERT(renderTargetProxy);
        SkASSERT(renderTargetProxy->isMSAADirty());
        renderTargetProxy->markMSAAResolved();
    }

    if (GrSurfaceProxy::ResolveFlags::kMipMaps & fResolveFlags) {
        GrTextureProxy* textureProxy = fTarget->asTextureProxy();
        SkASSERT(GrMipMapped::kYes == textureProxy->mipMapped());
        SkASSERT(textureProxy->mipMapsAreDirty());
        textureProxy->markMipMapsClean();
    }

    // Add the target as a dependency: We will read the existing contents of this texture while
    // generating mipmap levels and/or resolving MSAA.
    //
    // NOTE: This must be called before makeClosed.
    this->addDependency(fTarget.get(), GrMipMapped::kNo, GrTextureResolveManager(nullptr), caps);
    fTarget->setLastRenderTask(this);

    // We only resolve the texture; nobody should try to do anything else with this opsTask.
    this->makeClosed(caps);
}

void GrTextureResolveRenderTask::gatherProxyIntervals(GrResourceAllocator* alloc) const {
    // This renderTask doesn't have "normal" ops. In this case we still need to add an interval (so
    // fEndOfOpsTaskOpIndices will remain in sync), so we create a fake op# to capture the fact that
    // we manipulate fTarget.
    alloc->addInterval(fTarget.get(), alloc->curOp(), alloc->curOp(),
                       GrResourceAllocator::ActualUse::kYes);
    alloc->incOps();
}

bool GrTextureResolveRenderTask::onExecute(GrOpFlushState* flushState) {
    // Resolve msaa before regenerating mipmaps.
    if (GrSurfaceProxy::ResolveFlags::kMSAA & fResolveFlags) {
        GrRenderTarget* renderTarget = fTarget->peekRenderTarget();
        SkASSERT(renderTarget);
        if (renderTarget->needsResolve()) {
            flushState->gpu()->resolveRenderTarget(renderTarget);
        }
    }

    if (GrSurfaceProxy::ResolveFlags::kMipMaps & fResolveFlags) {
        GrTexture* texture = fTarget->peekTexture();
        SkASSERT(texture);
        if (texture->texturePriv().mipMapsAreDirty()) {
            flushState->gpu()->regenerateMipMapLevels(texture);
        }
    }

    return true;
}
