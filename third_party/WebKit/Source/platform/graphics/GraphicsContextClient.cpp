// Copyright 2015 The Chromium Authors. All rights reserved.
//
// The Chromium Authors can be found at
// http://src.chromium.org/svn/trunk/src/AUTHORS
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "platform/graphics/GraphicsContextClient.h"

#include "platform/graphics/GraphicsContext.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkShader.h"

namespace blink {

void GraphicsContextClient::willDrawRect(GraphicsContext* context, const SkRect& rect, const SkPaint* paint, ImageType imageType, DrawType drawType)
{
    if (context->isDrawingToLayer())
        return;

    SkRect deviceRect;
    if (drawType == UntransformedUnclippedFill) {
        deviceRect = rect;
    } else {
        // FIXME: Early out if rotate/skew. To be thorough, we could compute
        // an enclsed rect, but that would probably be overkill.
        const SkMatrix& ctm = context->getTotalMatrix();
        if (!ctm.rectStaysRect())
            return;

        if (context->hasComplexClip())
            return;

        ctm.mapRect(&deviceRect, rect);
        SkIRect skIBounds;
        if (!context->canvas()->getClipDeviceBounds(&skIBounds))
            return;
        SkRect skBounds = SkRect::Make(skIBounds);
        if (!deviceRect.intersect(skBounds))
            return;
    }

    const SkImageInfo& imageInfo = context->canvas()->imageInfo();
    if (!deviceRect.contains(SkRect::MakeWH(imageInfo.width(), imageInfo.height())))
        return;

    bool isSourceOver = true;
    unsigned alpha = 0xFF;
    if (paint) {
        if (drawType == FillOrStroke && paint->getStyle() == SkPaint::kStroke_Style)
            return;
        if (paint->getLooper() || paint->getImageFilter() || paint->getMaskFilter())
            return;

        SkXfermode* xfermode = paint->getXfermode();
        if (xfermode) {
            SkXfermode::Mode mode;
            if (xfermode->asMode(&mode)) {
                isSourceOver = mode == SkXfermode::kSrcOver_Mode;
                if (!isSourceOver && mode != SkXfermode::kSrc_Mode && mode != SkXfermode::kClear_Mode)
                    return; // The code below only knows how to handle Src, SrcOver, and Clear
            } else {
                // unknown xfermode
                return;
            }
        }

        if (isSourceOver) {
            // With source over, we need to certify that alpha == 0xFF for all pixels
            SkColorFilter* colorFilter = paint->getColorFilter();
            if (colorFilter && !(colorFilter->getFlags() & SkColorFilter::kAlphaUnchanged_Flag))
                return;

            SkShader* shader = paint->getShader();
            if (shader) {
                // Shader overrides bitmap and paint color, so we can end here
                if (shader->isOpaque())
                    willOverwriteCanvas();
                return;
            }
        }

        alpha = paint->getAlpha();
    }

    if (isSourceOver) {
        // With source over, we need to certify that alpha == 0xFF for all pixels
        if (imageType == NonOpaqueImage)
            return;
        if (imageType == NoImage && alpha < 0xFF)
            return;
    }

    willOverwriteCanvas();
}

} // blink
