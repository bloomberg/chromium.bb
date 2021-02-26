/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrMockOpTarget_DEFINED
#define GrMockOpTarget_DEFINED

#include "include/gpu/GrDirectContext.h"
#include "src/gpu/GrDirectContextPriv.h"
#include "src/gpu/ops/GrMeshDrawOp.h"

// This is a mock GrMeshDrawOp::Target implementation that just gives back pointers into
// pre-allocated CPU buffers, rather than allocating and mapping GPU buffers.
class GrMockOpTarget : public GrMeshDrawOp::Target {
public:
    GrMockOpTarget(sk_sp<GrDirectContext> mockContext) : fMockContext(std::move(mockContext)) {}
    const GrDirectContext* mockContext() const { return fMockContext.get(); }
    const GrCaps& caps() const override { return *fMockContext->priv().caps(); }
    GrThreadSafeCache* threadSafeCache() const override {
        return fMockContext->priv().threadSafeCache();
    }
    GrResourceProvider* resourceProvider() const override {
        return fMockContext->priv().resourceProvider();
    }
    GrSmallPathAtlasMgr* smallPathAtlasManager() const override { return nullptr; }
    void resetAllocator() { fAllocator.reset(); }
    SkArenaAlloc* allocator() override { return &fAllocator; }
    void putBackVertices(int vertices, size_t vertexStride) override { /* no-op */ }
    GrAppliedClip detachAppliedClip() override { return GrAppliedClip::Disabled(); }
    const GrXferProcessor::DstProxyView& dstProxyView() const override { return fDstProxyView; }
    GrXferBarrierFlags renderPassBarriers() const override { return GrXferBarrierFlags::kNone; }

    void* makeVertexSpace(size_t vertexSize, int vertexCount, sk_sp<const GrBuffer>*,
                          int* startVertex) override {
        if (vertexSize * vertexCount > sizeof(fStaticVertexData)) {
            SK_ABORT("FATAL: wanted %zu bytes of static vertex data; only have %zu.\n",
                     vertexSize * vertexCount, sizeof(fStaticVertexData));
        }
        *startVertex = 0;
        return fStaticVertexData;
    }

    void* makeVertexSpaceAtLeast(size_t vertexSize, int minVertexCount, int fallbackVertexCount,
                                 sk_sp<const GrBuffer>*, int* startVertex,
                                 int* actualVertexCount) override {
        if (vertexSize * minVertexCount > sizeof(fStaticVertexData)) {
            SK_ABORT("FATAL: wanted %zu bytes of static vertex data; only have %zu.\n",
                     vertexSize * minVertexCount, sizeof(fStaticVertexData));
        }
        *startVertex = 0;
        *actualVertexCount = sizeof(fStaticVertexData) / vertexSize;
        return fStaticVertexData;
    }

    GrDrawIndirectCommand* makeDrawIndirectSpace(int drawCount, sk_sp<const GrBuffer>* buffer,
                                                 size_t* offsetInBytes) override {
        int staticBufferCount = (int)SK_ARRAY_COUNT(fStaticDrawIndirectData);
        if (drawCount > staticBufferCount) {
            SK_ABORT("FATAL: wanted %i static drawIndirect elements; only have %i.\n",
                     drawCount, staticBufferCount);
        }
        return fStaticDrawIndirectData;
    }

    GrDrawIndexedIndirectCommand* makeDrawIndexedIndirectSpace(
            int drawCount, sk_sp<const GrBuffer>* buffer, size_t* offsetInBytes) override {
        int staticBufferCount = (int)SK_ARRAY_COUNT(fStaticDrawIndexedIndirectData);
        if (drawCount > staticBufferCount) {
            SK_ABORT("FATAL: wanted %i static drawIndexedIndirect elements; only have %i.\n",
                     drawCount, staticBufferCount);
        }
        return fStaticDrawIndexedIndirectData;
    }

#define UNIMPL(...) __VA_ARGS__ override { SK_ABORT("unimplemented."); }
    UNIMPL(void recordDraw(const GrGeometryProcessor*, const GrSimpleMesh[], int,
                           const GrSurfaceProxy* const[], GrPrimitiveType))
    UNIMPL(uint16_t* makeIndexSpace(int, sk_sp<const GrBuffer>*, int*))
    UNIMPL(uint16_t* makeIndexSpaceAtLeast(int, int, sk_sp<const GrBuffer>*, int*, int*))
    UNIMPL(void putBackIndices(int))
    UNIMPL(GrRenderTargetProxy* proxy() const)
    UNIMPL(const GrSurfaceProxyView* writeView() const)
    UNIMPL(const GrAppliedClip* appliedClip() const)
    UNIMPL(GrStrikeCache* strikeCache() const)
    UNIMPL(GrAtlasManager* atlasManager() const)
    UNIMPL(SkTArray<GrSurfaceProxy*, true>* sampledProxyArray())
    UNIMPL(GrDeferredUploadTarget* deferredUploadTarget())
#undef UNIMPL

private:
    sk_sp<GrDirectContext> fMockContext;
    char fStaticVertexData[4 * 1024 * 1024];
    GrDrawIndirectCommand fStaticDrawIndirectData[32];
    GrDrawIndexedIndirectCommand fStaticDrawIndexedIndirectData[32];
    SkSTArenaAllocWithReset<1024 * 1024> fAllocator;
    GrXferProcessor::DstProxyView fDstProxyView;
};

#endif
