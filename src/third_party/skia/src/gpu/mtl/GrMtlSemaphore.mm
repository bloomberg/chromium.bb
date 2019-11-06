/*
 * Copyright 2019 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/mtl/GrMtlGpu.h"
#include "src/gpu/mtl/GrMtlSemaphore.h"

#if !__has_feature(objc_arc)
#error This file must be compiled with Arc. Use -fobjc-arc flag
#endif

#ifdef GR_METAL_SDK_SUPPORTS_EVENTS
sk_sp<GrMtlSemaphore> GrMtlSemaphore::Make(GrMtlGpu* gpu, bool isOwned) {
    id<MTLEvent> event = [gpu->device() newEvent];
    uint64_t value = 1; // seems like a reasonable starting point
    return sk_sp<GrMtlSemaphore>(new GrMtlSemaphore(gpu, event, value, isOwned));
}

sk_sp<GrMtlSemaphore> GrMtlSemaphore::MakeWrapped(GrMtlGpu* gpu,
                                                  GrMTLHandle event,
                                                  uint64_t value,
                                                  GrWrapOwnership ownership) {
    // The GrMtlSemaphore will have strong ownership at this point.
    // The GrMTLHandle will subsequently only have weak ownership.
    id<MTLEvent> mtlEvent = (__bridge_transfer id<MTLEvent>)event;
    auto sema = sk_sp<GrMtlSemaphore>(new GrMtlSemaphore(gpu, mtlEvent, value,
                                                         kBorrow_GrWrapOwnership != ownership));
    return sema;
}

GrMtlSemaphore::GrMtlSemaphore(GrMtlGpu* gpu, id<MTLEvent> event, uint64_t value, bool isOwned)
        : INHERITED(gpu), fEvent(event), fValue(value) {
    isOwned ? this->registerWithCache(SkBudgeted::kNo)
            : this->registerWithCacheWrapped(GrWrapCacheable::kNo);
}

void GrMtlSemaphore::onRelease() {
    fEvent = nil;
    INHERITED::onRelease();
}

void GrMtlSemaphore::onAbandon() {
    fEvent = nil;
    INHERITED::onAbandon();
}

GrBackendSemaphore GrMtlSemaphore::backendSemaphore() const {
    GrBackendSemaphore backendSemaphore;
    // The GrMtlSemaphore and the GrBackendSemaphore will have strong ownership at this point.
    // Whoever uses the GrBackendSemaphore will subsquently steal this ref (see MakeWrapped, above).
    GrMTLHandle handle = (__bridge_retained GrMTLHandle)(fEvent);
    backendSemaphore.initMetal(handle, fValue);
    return backendSemaphore;
}
#endif
