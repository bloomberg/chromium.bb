// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRPresentationContext_h
#define XRPresentationContext_h

#include "base/memory/scoped_refptr.h"
#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "modules/ModulesExport.h"
#include "modules/canvas/imagebitmap/ImageBitmapRenderingContextBase.h"

namespace blink {

class MODULES_EXPORT XRPresentationContext final
    : public ImageBitmapRenderingContextBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class Factory : public CanvasRenderingContextFactory {
    WTF_MAKE_NONCOPYABLE(Factory);

   public:
    Factory() {}
    ~Factory() override {}

    CanvasRenderingContext* Create(
        CanvasRenderingContextHost*,
        const CanvasContextCreationAttributesCore&) override;
    CanvasRenderingContext::ContextType GetContextType() const override {
      return CanvasRenderingContext::kContextXRPresent;
    }
  };

  // CanvasRenderingContext implementation
  ContextType GetContextType() const override {
    return CanvasRenderingContext::kContextXRPresent;
  }
  void SetCanvasGetContextResult(RenderingContext&) final;

  virtual ~XRPresentationContext();

 private:
  XRPresentationContext(CanvasRenderingContextHost*,
                        const CanvasContextCreationAttributesCore&);
};

DEFINE_TYPE_CASTS(XRPresentationContext,
                  CanvasRenderingContext,
                  context,
                  context->GetContextType() ==
                      CanvasRenderingContext::kContextXRPresent,
                  context.GetContextType() ==
                      CanvasRenderingContext::kContextXRPresent);

}  // namespace blink

#endif  // XRPresentationContext_h
