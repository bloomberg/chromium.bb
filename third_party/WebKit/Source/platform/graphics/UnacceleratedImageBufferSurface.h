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

#ifndef UnacceleratedImageBufferSurface_h
#define UnacceleratedImageBufferSurface_h

#include "platform/graphics/ImageBufferSurface.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class PLATFORM_EXPORT UnacceleratedImageBufferSurface
    : public ImageBufferSurface {
  WTF_MAKE_NONCOPYABLE(UnacceleratedImageBufferSurface);
  USING_FAST_MALLOC(UnacceleratedImageBufferSurface);

 public:
  UnacceleratedImageBufferSurface(
      const IntSize&,
      ImageInitializationMode = kInitializeImagePixels,
      const CanvasColorParams& = CanvasColorParams(kSRGBCanvasColorSpace,
                                                   kRGBA8CanvasPixelFormat,
                                                   kNonOpaque));
  ~UnacceleratedImageBufferSurface() override;

  PaintCanvas* Canvas() override;
  bool IsValid() const override;
  bool WritePixels(const SkImageInfo& orig_info,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y) override;

  scoped_refptr<StaticBitmapImage> NewImageSnapshot(AccelerationHint,
                                                    SnapshotReason) override;

 private:
  sk_sp<SkSurface> surface_;
  std::unique_ptr<PaintCanvas> canvas_;
};

}  // namespace blink

#endif
