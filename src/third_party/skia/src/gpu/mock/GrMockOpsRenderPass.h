/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrMockOpsRenderPass_DEFINED
#define GrMockOpsRenderPass_DEFINED

#include "src/gpu/GrOpsRenderPass.h"

#include "src/gpu/GrTexturePriv.h"
#include "src/gpu/mock/GrMockGpu.h"

class GrMockOpsRenderPass : public GrOpsRenderPass {
public:
    GrMockOpsRenderPass(GrMockGpu* gpu, GrRenderTarget* rt, GrSurfaceOrigin origin,
                        LoadAndStoreInfo colorInfo)
            : INHERITED(rt, origin)
            , fGpu(gpu)
            , fColorLoadOp(colorInfo.fLoadOp) {
    }

    GrGpu* gpu() override { return fGpu; }
    void inlineUpload(GrOpFlushState*, GrDeferredTextureUploadFn&) override {}

    int numDraws() const { return fNumDraws; }

private:
    void onBegin() override {
        if (GrLoadOp::kClear == fColorLoadOp) {
            this->markRenderTargetDirty();
        }
    }
    bool onBindPipeline(const GrProgramInfo&, const SkRect&) override { return true; }
    void onSetScissorRect(const SkIRect&) override {}
    bool onBindTextures(const GrPrimitiveProcessor&, const GrSurfaceProxy* const primProcTextures[],
                        const GrPipeline&) override { return true; }
    void onBindBuffers(const GrBuffer* indexBuffer, const GrBuffer* instanceBuffer,
                       const GrBuffer* vertexBuffer, GrPrimitiveRestart) override {}
    void onDraw(int, int) override { this->dummyDraw(); }
    void onDrawIndexed(int, int, uint16_t, uint16_t, int) override { this->dummyDraw(); }
    void onDrawInstanced(int, int, int, int) override { this->dummyDraw(); }
    void onDrawIndexedInstanced(int, int, int, int, int) override { this->dummyDraw(); }
    void onDrawIndirect(const GrBuffer*, size_t, int) override { this->dummyDraw(); }
    void onDrawIndexedIndirect(const GrBuffer*, size_t, int) override { this->dummyDraw(); }
    void onClear(const GrFixedClip&, const SkPMColor4f&) override { this->markRenderTargetDirty(); }
    void onClearStencilClip(const GrFixedClip&, bool insideStencilMask) override {}
    void dummyDraw() {
        this->markRenderTargetDirty();
        ++fNumDraws;
    }
    void markRenderTargetDirty() {
        if (auto* tex = fRenderTarget->asTexture()) {
            tex->texturePriv().markMipMapsDirty();
        }
    }

    GrMockGpu* fGpu;
    GrLoadOp fColorLoadOp;
    int fNumDraws = 0;

    typedef GrOpsRenderPass INHERITED;
};

#endif
