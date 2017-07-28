// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageBitmapRenderingContext_h
#define ImageBitmapRenderingContext_h

#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "modules/ModulesExport.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class ImageBitmap;
class ImageLayerBridge;

class MODULES_EXPORT ImageBitmapRenderingContext final
    : public CanvasRenderingContext {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class Factory : public CanvasRenderingContextFactory {
    WTF_MAKE_NONCOPYABLE(Factory);

   public:
    Factory() {}
    ~Factory() override {}

    CanvasRenderingContext* Create(
        CanvasRenderingContextHost*,
        const CanvasContextCreationAttributes&) override;
    CanvasRenderingContext::ContextType GetContextType() const override {
      return CanvasRenderingContext::kContextImageBitmap;
    }
  };

  DECLARE_TRACE();

  // Script API
  void transferFromImageBitmap(ImageBitmap*, ExceptionState&);

  HTMLCanvasElement* canvas() {
    DCHECK(!host() || !host()->IsOffscreenCanvas());
    return static_cast<HTMLCanvasElement*>(host());
  }

  // CanvasRenderingContext implementation
  ContextType GetContextType() const override {
    return CanvasRenderingContext::kContextImageBitmap;
  }
  void SetIsHidden(bool) override {}
  bool isContextLost() const override { return false; }
  void SetCanvasGetContextResult(RenderingContext&) final;
  RefPtr<StaticBitmapImage> GetImage(AccelerationHint,
                                     SnapshotReason) const final;
  bool IsComposited() const final { return true; }
  bool IsAccelerated() const final;

  WebLayer* PlatformLayer() const final;
  // TODO(junov): handle lost contexts when content is GPU-backed
  void LoseContext(LostContextMode) override {}

  void Stop() override;

  bool IsPaintable() const final;

  virtual ~ImageBitmapRenderingContext();

 private:
  ImageBitmapRenderingContext(CanvasRenderingContextHost*,
                              const CanvasContextCreationAttributes&);

  Member<ImageLayerBridge> image_layer_bridge_;
};

DEFINE_TYPE_CASTS(ImageBitmapRenderingContext,
                  CanvasRenderingContext,
                  context,
                  context->GetContextType() ==
                      CanvasRenderingContext::kContextImageBitmap,
                  context.GetContextType() ==
                      CanvasRenderingContext::kContextImageBitmap);

}  // blink

#endif
