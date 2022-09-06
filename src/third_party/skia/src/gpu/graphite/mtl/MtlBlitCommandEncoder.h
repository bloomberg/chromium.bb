/*
 * Copyright 2021 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_MtlBlitCommandEncoder_DEFINED
#define skgpu_graphite_MtlBlitCommandEncoder_DEFINED

#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "include/ports/SkCFObject.h"
#include "src/gpu/graphite/Resource.h"

#import <Metal/Metal.h>

namespace skgpu::graphite {

/**
 * Wraps a MTLMtlBlitCommandEncoder object
 */
class MtlBlitCommandEncoder : public Resource {
public:
    static sk_sp<MtlBlitCommandEncoder> Make(const skgpu::graphite::Gpu* gpu,
                                             id<MTLCommandBuffer> commandBuffer) {
        // Adding a retain here to keep our own ref separate from the autorelease pool
        sk_cfp<id<MTLBlitCommandEncoder>> encoder =
                sk_ret_cfp<id<MTLBlitCommandEncoder>>([commandBuffer blitCommandEncoder]);
        return sk_sp<MtlBlitCommandEncoder>(new MtlBlitCommandEncoder(gpu, std::move(encoder)));
    }

    void pushDebugGroup(NSString* string) {
        [(*fCommandEncoder) pushDebugGroup:string];
    }
    void popDebugGroup() {
        [(*fCommandEncoder) popDebugGroup];
    }
#ifdef SK_BUILD_FOR_MAC
    void synchronizeResource(id<MTLBuffer> buffer) {
        [(*fCommandEncoder) synchronizeResource: buffer];
    }
#endif

    void copyFromTexture(id<MTLTexture> texture,
                         SkIRect srcRect,
                         id<MTLBuffer> buffer,
                         size_t bufferOffset,
                         size_t bufferRowBytes) {
        [(*fCommandEncoder) copyFromTexture: texture
                                sourceSlice: 0
                                sourceLevel: 0
                               sourceOrigin: MTLOriginMake(srcRect.left(), srcRect.top(), 0)
                                 sourceSize: MTLSizeMake(srcRect.width(), srcRect.height(), 1)
                                   toBuffer: buffer
                          destinationOffset: bufferOffset
                     destinationBytesPerRow: bufferRowBytes
                   destinationBytesPerImage: bufferRowBytes * srcRect.height()];
    }

    void copyFromBuffer(id<MTLBuffer> buffer,
                        size_t bufferOffset,
                        size_t bufferRowBytes,
                        id<MTLTexture> texture,
                        SkIRect dstRect,
                        unsigned int dstLevel) {
        [(*fCommandEncoder) copyFromBuffer: buffer
                              sourceOffset: bufferOffset
                         sourceBytesPerRow: bufferRowBytes
                       sourceBytesPerImage: bufferRowBytes * dstRect.height()
                                sourceSize: MTLSizeMake(dstRect.width(), dstRect.height(), 1)
                                 toTexture: texture
                          destinationSlice: 0
                          destinationLevel: dstLevel
                         destinationOrigin: MTLOriginMake(dstRect.left(), dstRect.top(), 0)];
    }

    void endEncoding() {
        [(*fCommandEncoder) endEncoding];
    }

private:
    MtlBlitCommandEncoder(const skgpu::graphite::Gpu* gpu,
                          sk_cfp<id<MTLBlitCommandEncoder>> encoder)
        : Resource(gpu, Ownership::kOwned, SkBudgeted::kYes)
        , fCommandEncoder(std::move(encoder)) {}

    void freeGpuData() override {
        fCommandEncoder.reset();
    }

    sk_cfp<id<MTLBlitCommandEncoder>> fCommandEncoder;
};

} // namespace skgpu::graphite

#endif // skgpu_graphite_MtlBlitCommandEncoder_DEFINED
