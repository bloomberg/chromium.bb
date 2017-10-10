/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/ImageBufferSurface.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"


namespace blink {

ImageBufferSurface::ImageBufferSurface(const IntSize& size,
                                       OpacityMode opacity_mode,
                                       const CanvasColorParams& color_params)
    : opacity_mode_(opacity_mode), size_(size), color_params_(color_params) {
  SetIsHidden(false);
}

ImageBufferSurface::~ImageBufferSurface() {}

sk_sp<PaintRecord> ImageBufferSurface::GetRecord() {
  return nullptr;
}

void ImageBufferSurface::Clear() {
  // Clear the background transparent or opaque, as required. It would be nice
  // if this wasn't required, but the canvas is currently filled with the magic
  // transparency color. Can we have another way to manage this?
  if (IsValid()) {
    if (opacity_mode_ == kOpaque) {
      Canvas()->clear(SK_ColorBLACK);
    } else {
      Canvas()->clear(SK_ColorTRANSPARENT);
    }
    DidDraw(FloatRect(FloatPoint(0, 0), FloatSize(Size())));
  }
}

void ImageBufferSurface::Draw(GraphicsContext& context,
                              const FloatRect& dest_rect,
                              const FloatRect& src_rect,
                              SkBlendMode op) {
  RefPtr<StaticBitmapImage> snapshot =
      NewImageSnapshot(kPreferAcceleration, kSnapshotReasonPaint);
  if (!snapshot)
    return;

  // GraphicsContext cannot handle gpu resource serialization.
  snapshot = snapshot->MakeUnaccelerated();

  DCHECK(!snapshot->IsTextureBacked());
  context.DrawImage(snapshot.get(), dest_rect, &src_rect, op);
}

void ImageBufferSurface::Flush(FlushReason) {
  Canvas()->flush();
}

}  // namespace blink
