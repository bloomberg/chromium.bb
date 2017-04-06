// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageBitmapRenderingContext_h
#define ImageBitmapRenderingContext_h

#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "modules/ModulesExport.h"
#include "wtf/RefPtr.h"

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

    CanvasRenderingContext* create(HTMLCanvasElement*,
                                   const CanvasContextCreationAttributes&,
                                   Document&) override;
    CanvasRenderingContext::ContextType getContextType() const override {
      return CanvasRenderingContext::ContextImageBitmap;
    }
  };

  DECLARE_TRACE();

  // Script API
  void transferFromImageBitmap(ImageBitmap*, ExceptionState&);

  // CanvasRenderingContext implementation
  ContextType getContextType() const override {
    return CanvasRenderingContext::ContextImageBitmap;
  }
  void setIsHidden(bool) override {}
  bool isContextLost() const override { return false; }
  void setCanvasGetContextResult(RenderingContext&) final;
  PassRefPtr<Image> getImage(AccelerationHint, SnapshotReason) const final;
  bool isComposited() const final { return true; }
  bool isAccelerated() const final;

  WebLayer* platformLayer() const final;
  // TODO(junov): handle lost contexts when content is GPU-backed
  void loseContext(LostContextMode) override {}

  void stop() override;

  bool isPaintable() const final;

  virtual ~ImageBitmapRenderingContext();

 private:
  ImageBitmapRenderingContext(HTMLCanvasElement*,
                              const CanvasContextCreationAttributes&,
                              Document&);

  Member<ImageLayerBridge> m_imageLayerBridge;
};

DEFINE_TYPE_CASTS(ImageBitmapRenderingContext,
                  CanvasRenderingContext,
                  context,
                  context->getContextType() ==
                      CanvasRenderingContext::ContextImageBitmap,
                  context.getContextType() ==
                      CanvasRenderingContext::ContextImageBitmap);

}  // blink

#endif
