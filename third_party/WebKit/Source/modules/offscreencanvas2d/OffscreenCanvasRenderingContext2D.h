// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OffscreenCanvasRenderingContext2D_h
#define OffscreenCanvasRenderingContext2D_h

#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "modules/canvas2d/BaseRenderingContext2D.h"
#include <memory>

namespace blink {

class MODULES_EXPORT OffscreenCanvasRenderingContext2D final
    : public CanvasRenderingContext,
      public BaseRenderingContext2D {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(OffscreenCanvasRenderingContext2D);

 public:
  class Factory : public CanvasRenderingContextFactory {
   public:
    Factory() {}
    ~Factory() override {}

    CanvasRenderingContext* Create(
        ScriptState* script_state,
        OffscreenCanvas* canvas,
        const CanvasContextCreationAttributes& attrs) override {
      return new OffscreenCanvasRenderingContext2D(script_state, canvas, attrs);
    }

    CanvasRenderingContext::ContextType GetContextType() const override {
      return CanvasRenderingContext::kContext2d;
    }
  };

  ScriptPromise commit(ScriptState*, ExceptionState&);

  // CanvasRenderingContext implementation
  ~OffscreenCanvasRenderingContext2D() override;
  ContextType GetContextType() const override { return kContext2d; }
  bool Is2d() const override { return true; }
  bool IsComposited() const override { return false; }
  bool IsAccelerated() const override;
  void SetOffscreenCanvasGetContextResult(OffscreenRenderingContext&) final;
  void SetIsHidden(bool) final { NOTREACHED(); }
  void Stop() final { NOTREACHED(); }
  void SetCanvasGetContextResult(RenderingContext&) final {}
  void clearRect(double x, double y, double width, double height) override {
    BaseRenderingContext2D::clearRect(x, y, width, height);
  }
  PassRefPtr<Image> GetImage(AccelerationHint, SnapshotReason) const final;
  ImageData* ToImageData(SnapshotReason) override;
  void Reset() override;

  // BaseRenderingContext2D implementation
  bool OriginClean() const final;
  void SetOriginTainted() final;
  bool WouldTaintOrigin(CanvasImageSource*, ExecutionContext*) final;

  int Width() const final;
  int Height() const final;

  bool HasImageBuffer() const final;
  ImageBuffer* GetImageBuffer() const final;

  bool ParseColorOrCurrentColor(Color&, const String& color_string) const final;

  PaintCanvas* DrawingCanvas() const final;
  PaintCanvas* ExistingDrawingCanvas() const final;
  void DisableDeferral(DisableDeferralReason) final;

  AffineTransform BaseTransform() const final;
  void DidDraw(const SkIRect& dirty_rect) final;  // overrides
                                                  // BaseRenderingContext2D and
                                                  // CanvasRenderingContext

  bool StateHasFilter() final;
  sk_sp<SkImageFilter> StateGetFilter() final;
  void SnapshotStateForFilter() final {}

  void ValidateStateStack() const final;

  bool HasAlpha() const final { return CreationAttributes().alpha(); }
  bool isContextLost() const override;

  ImageBitmap* TransferToImageBitmap(ScriptState*) final;

  ColorBehavior DrawImageColorBehavior() const final;

 protected:
  OffscreenCanvasRenderingContext2D(
      ScriptState*,
      OffscreenCanvas*,
      const CanvasContextCreationAttributes& attrs);
  DECLARE_VIRTUAL_TRACE();

  virtual void NeedsFinalizeFrame() {
    CanvasRenderingContext::NeedsFinalizeFrame();
  }

 private:
  bool needs_matrix_clip_restore_ = false;
  std::unique_ptr<ImageBuffer> image_buffer_;

  bool IsPaintable() const final;

  RefPtr<StaticBitmapImage> TransferToStaticBitmapImage();
};

DEFINE_TYPE_CASTS(OffscreenCanvasRenderingContext2D,
                  CanvasRenderingContext,
                  context,
                  context->Is2d() && context->offscreenCanvas(),
                  context.Is2d() && context.offscreenCanvas());

}  // namespace blink

#endif  // OffscreenCanvasRenderingContext2D_h
