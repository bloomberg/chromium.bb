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

#ifndef ImageBufferSurface_h
#define ImageBufferSurface_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/CanvasColorParams.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"

struct SkImageInfo;

namespace blink {

class FloatRect;
class GraphicsContext;
class ImageBuffer;
class StaticBitmapImage;
class WebLayer;

class PLATFORM_EXPORT ImageBufferSurface {
  WTF_MAKE_NONCOPYABLE(ImageBufferSurface);
  USING_FAST_MALLOC(ImageBufferSurface);

 public:
  virtual ~ImageBufferSurface();

  virtual PaintCanvas* Canvas() = 0;
  virtual void DisableDeferral(DisableDeferralReason) {}
  virtual void WillOverwriteCanvas() {}
  virtual void DidDraw(const FloatRect& rect) {}
  virtual bool IsValid() const = 0;
  virtual bool Restore() { return false; }
  virtual WebLayer* Layer() const { return 0; }
  virtual bool IsAccelerated() const { return false; }
  virtual bool IsRecording() const { return false; }
  virtual bool IsExpensiveToPaint() { return false; }
  virtual void SetFilterQuality(SkFilterQuality) {}
  virtual void SetIsHidden(bool) {}
  virtual void SetImageBuffer(ImageBuffer*) {}
  virtual sk_sp<PaintRecord> GetRecord();
  virtual void FinalizeFrame() {}
  virtual void DoPaintInvalidation(const FloatRect& dirty_rect) {}
  virtual void Draw(GraphicsContext&,
                    const FloatRect& dest_rect,
                    const FloatRect& src_rect,
                    SkBlendMode);
  virtual void SetHasExpensiveOp() {}
  virtual GLuint GetBackingTextureHandleForOverwrite() { return 0; }

  // Executes all deferred rendering immediately.
  virtual void Flush(FlushReason);

  // Like flush, but flushes all the way down to the GPU context if the surface
  // uses the GPU.
  virtual void FlushGpu(FlushReason reason) { Flush(reason); }

  virtual void PrepareSurfaceForPaintingIfNeeded() {}
  virtual bool WritePixels(const SkImageInfo& orig_info,
                           const void* pixels,
                           size_t row_bytes,
                           int x,
                           int y) = 0;

  // May return nullptr if the surface is GPU-backed and the GPU context was
  // lost.
  virtual RefPtr<StaticBitmapImage> NewImageSnapshot(AccelerationHint,
                                                     SnapshotReason) = 0;

  OpacityMode GetOpacityMode() const { return opacity_mode_; }
  const IntSize& Size() const { return size_; }
  const CanvasColorParams& ColorParams() const { return color_params_; }
  void NotifyIsValidChanged(bool is_valid) const;

 protected:
  ImageBufferSurface(const IntSize&, OpacityMode, const CanvasColorParams&);
  void Clear();

 private:
  OpacityMode opacity_mode_;
  IntSize size_;
  CanvasColorParams color_params_;
};

}  // namespace blink

#endif
