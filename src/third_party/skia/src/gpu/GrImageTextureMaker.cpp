/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrImageTextureMaker.h"
#include "GrContext.h"
#include "GrContextPriv.h"
#include "GrGpuResourcePriv.h"
#include "SkGr.h"
#include "SkImageCacherator.h"
#include "SkImage_Base.h"
#include "SkPixelRef.h"

GrImageTextureMaker::GrImageTextureMaker(GrContext* context, const SkImage* client,
                                         SkImage::CachingHint chint)
        : INHERITED(context, client->width(), client->height(), client->isAlphaOnly())
        , fCacher(as_IB(client)->peekCacherator())
        , fClient(client)
        , fCachingHint(chint) {
    SkASSERT(fCacher);
    GrMakeKeyFromImageID(&fOriginalKey, client->uniqueID(),
                         SkIRect::MakeWH(this->width(), this->height()));
}

sk_sp<GrTextureProxy> GrImageTextureMaker::refOriginalTextureProxy(bool willBeMipped,
                                                                   SkColorSpace* dstColorSpace,
                                                                   AllowedTexGenType onlyIfFast) {
    return fCacher->lockTextureProxy(this->context(), fOriginalKey, fCachingHint,
                                     willBeMipped, dstColorSpace, onlyIfFast);
}

void GrImageTextureMaker::makeCopyKey(const CopyParams& stretch, GrUniqueKey* paramsCopyKey,
                                      SkColorSpace* dstColorSpace) {
    if (fOriginalKey.isValid() && SkImage::kAllow_CachingHint == fCachingHint) {
        GrUniqueKey cacheKey;
        fCacher->makeCacheKeyFromOrigKey(fOriginalKey, &cacheKey);
        MakeCopyKeyFromOrigKey(cacheKey, stretch, paramsCopyKey);
    }
}

SkAlphaType GrImageTextureMaker::alphaType() const {
    return fClient->alphaType();
}
sk_sp<SkColorSpace> GrImageTextureMaker::getColorSpace(SkColorSpace* dstColorSpace) {
    return fCacher->getColorSpace(this->context(), dstColorSpace);
}
