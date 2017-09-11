// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintRenderingContext2D_h
#define PaintRenderingContext2D_h

#include <memory>
#include "modules/ModulesExport.h"
#include "modules/canvas2d/BaseRenderingContext2D.h"
#include "modules/csspaint/PaintRenderingContext2DSettings.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/graphics/ImageBuffer.h"

namespace blink {

class CanvasImageSource;
class Color;

class MODULES_EXPORT PaintRenderingContext2D
    : public BaseRenderingContext2D,
      public GarbageCollectedFinalized<PaintRenderingContext2D>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PaintRenderingContext2D);
  WTF_MAKE_NONCOPYABLE(PaintRenderingContext2D);

 public:
  static PaintRenderingContext2D* Create(
      std::unique_ptr<ImageBuffer> image_buffer,
      const PaintRenderingContext2DSettings& context_settings,
      float zoom) {
    return new PaintRenderingContext2D(std::move(image_buffer),
                                       context_settings, zoom);
  }

  // BaseRenderingContext2D

  // PaintRenderingContext2D doesn't have any pixel readback so the origin
  // is always clean, and unable to taint it.
  bool OriginClean() const final { return true; }
  void SetOriginTainted() final {}
  bool WouldTaintOrigin(CanvasImageSource*, ExecutionContext*) final {
    return false;
  }

  int Width() const final;
  int Height() const final;

  bool HasImageBuffer() const final { return image_buffer_.get(); }
  ImageBuffer* GetImageBuffer() const final { return image_buffer_.get(); }

  bool ParseColorOrCurrentColor(Color&, const String& color_string) const final;

  PaintCanvas* DrawingCanvas() const final;
  PaintCanvas* ExistingDrawingCanvas() const final;
  void DisableDeferral(DisableDeferralReason) final {}

  AffineTransform BaseTransform() const final;

  void DidDraw(const SkIRect& dirty_rect) final;

  bool StateHasFilter() final;
  sk_sp<SkImageFilter> StateGetFilter() final;
  void SnapshotStateForFilter() final {}

  void ValidateStateStack() const final;

  bool HasAlpha() const final { return context_settings_.alpha(); }

  // PaintRenderingContext2D cannot lose it's context.
  bool isContextLost() const final { return false; }

 private:
  PaintRenderingContext2D(std::unique_ptr<ImageBuffer>,
                          const PaintRenderingContext2DSettings&,
                          float zoom);

  std::unique_ptr<ImageBuffer> image_buffer_;
  PaintRenderingContext2DSettings context_settings_;
};

}  // namespace blink

#endif  // PaintRenderingContext2D_h
