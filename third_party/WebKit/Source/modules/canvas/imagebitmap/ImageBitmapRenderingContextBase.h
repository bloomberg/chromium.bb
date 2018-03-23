// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageBitmapRenderingContextBase_h
#define ImageBitmapRenderingContextBase_h

#include "base/memory/scoped_refptr.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "modules/ModulesExport.h"
#include "platform/geometry/FloatPoint.h"

namespace blink {

class ImageBitmap;
class ImageLayerBridge;

class MODULES_EXPORT ImageBitmapRenderingContextBase
    : public CanvasRenderingContext {
 public:
  ImageBitmapRenderingContextBase(CanvasRenderingContextHost*,
                                  const CanvasContextCreationAttributesCore&);
  virtual ~ImageBitmapRenderingContextBase();

  void Trace(blink::Visitor*);

  HTMLCanvasElement* canvas() {
    DCHECK(!Host() || !Host()->IsOffscreenCanvas());
    return static_cast<HTMLCanvasElement*>(Host());
  }

  void SetIsHidden(bool) override {}
  bool isContextLost() const override { return false; }
  void SetImage(ImageBitmap*);
  scoped_refptr<StaticBitmapImage> GetImage(AccelerationHint) const final;
  void SetUV(const FloatPoint left_top, const FloatPoint right_bottom);
  bool IsComposited() const final { return true; }
  bool IsAccelerated() const final;

  WebLayer* PlatformLayer() const final;
  // TODO(junov): handle lost contexts when content is GPU-backed
  void LoseContext(LostContextMode) override {}

  void Stop() override;

  bool IsPaintable() const final;

 protected:
  Member<ImageLayerBridge> image_layer_bridge_;
};

}  // namespace blink

#endif
