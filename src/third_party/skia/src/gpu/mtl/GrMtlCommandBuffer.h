/*
 * Copyright 2019 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrMtlCommandBuffer_DEFINED
#define GrMtlCommandBuffer_DEFINED

#import <Metal/Metal.h>

#include "include/core/SkRefCnt.h"
#include "include/gpu/GrTypes.h"
#include "src/gpu/GrBuffer.h"
#include "src/gpu/mtl/GrMtlRenderCommandEncoder.h"
#include "src/gpu/mtl/GrMtlUtil.h"

class GrMtlGpu;
class GrMtlPipelineState;
class GrMtlOpsRenderPass;
class GrMtlRenderCommandEncoder;

GR_NORETAIN_BEGIN

class GrMtlCommandBuffer : public SkRefCnt {
public:
    static sk_sp<GrMtlCommandBuffer> Make(id<MTLCommandQueue> queue);
    ~GrMtlCommandBuffer() override;

    bool commit(bool waitUntilCompleted);
    bool hasWork() { return fHasWork; }

    void addFinishedCallback(sk_sp<GrRefCntedCallback> callback) {
        fFinishedCallbacks.push_back(std::move(callback));
    }

    id<MTLBlitCommandEncoder> getBlitCommandEncoder();
    GrMtlRenderCommandEncoder* getRenderCommandEncoder(MTLRenderPassDescriptor*,
                                                       const GrMtlPipelineState*,
                                                       GrMtlOpsRenderPass* opsRenderPass);

    void addCompletedHandler(MTLCommandBufferHandler block) {
        [fCmdBuffer addCompletedHandler:block];
    }

    void addGrBuffer(sk_sp<const GrBuffer> buffer) {
        fTrackedGrBuffers.push_back(std::move(buffer));
    }

    void encodeSignalEvent(id<MTLEvent>, uint64_t value) SK_API_AVAILABLE(macos(10.14), ios(12.0));
    void encodeWaitForEvent(id<MTLEvent>, uint64_t value) SK_API_AVAILABLE(macos(10.14), ios(12.0));

    void waitUntilCompleted() {
        [fCmdBuffer waitUntilCompleted];
    }
    bool isCompleted() {
        return fCmdBuffer.status == MTLCommandBufferStatusCompleted ||
               fCmdBuffer.status == MTLCommandBufferStatusError;
    }
    void callFinishedCallbacks() { fFinishedCallbacks.reset(); }

    void pushDebugGroup(NSString* string) {
        if (@available(macOS 10.13, iOS 11.0, *)) {
            [fCmdBuffer pushDebugGroup:string];
        }
    }

    void popDebugGroup() {
        if (@available(macOS 10.13, iOS 11.0, *)) {
            [fCmdBuffer popDebugGroup];
        }
    }

private:
    static const int kInitialTrackedResourcesCount = 32;

    GrMtlCommandBuffer(id<MTLCommandBuffer> cmdBuffer)
        : fCmdBuffer(cmdBuffer)
        , fActiveBlitCommandEncoder(nil)
        , fActiveRenderCommandEncoder(nil)
        , fPreviousRenderPassDescriptor(nil)
        , fHasWork(false) {}

    void endAllEncoding();

    id<MTLCommandBuffer>        fCmdBuffer;
    id<MTLBlitCommandEncoder>   fActiveBlitCommandEncoder;
    std::unique_ptr<GrMtlRenderCommandEncoder> fActiveRenderCommandEncoder;
    MTLRenderPassDescriptor*    fPreviousRenderPassDescriptor;
    bool                        fHasWork;

    SkTArray<sk_sp<GrRefCntedCallback>> fFinishedCallbacks;

    SkSTArray<kInitialTrackedResourcesCount, sk_sp<const GrBuffer>> fTrackedGrBuffers;
};

GR_NORETAIN_END

#endif
