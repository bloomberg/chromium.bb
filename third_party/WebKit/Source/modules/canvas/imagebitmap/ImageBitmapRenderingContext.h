// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageBitmapRenderingContext_h
#define ImageBitmapRenderingContext_h

#include "base/memory/scoped_refptr.h"
#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "modules/ModulesExport.h"
#include "modules/canvas/imagebitmap/ImageBitmapRenderingContextBase.h"

namespace blink {

class ImageBitmap;

class MODULES_EXPORT ImageBitmapRenderingContext final
    : public ImageBitmapRenderingContextBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class Factory : public CanvasRenderingContextFactory {
    WTF_MAKE_NONCOPYABLE(Factory);

   public:
    Factory() = default;
    ~Factory() override = default;

    CanvasRenderingContext* Create(
        CanvasRenderingContextHost*,
        const CanvasContextCreationAttributesCore&) override;
    CanvasRenderingContext::ContextType GetContextType() const override {
      return CanvasRenderingContext::kContextImageBitmap;
    }
  };

  // Script API
  void transferFromImageBitmap(ImageBitmap*, ExceptionState&);

  // CanvasRenderingContext implementation
  ContextType GetContextType() const override {
    return CanvasRenderingContext::kContextImageBitmap;
  }

  void SetCanvasGetContextResult(RenderingContext&) final;

  virtual ~ImageBitmapRenderingContext();

 private:
  ImageBitmapRenderingContext(CanvasRenderingContextHost*,
                              const CanvasContextCreationAttributesCore&);
};

DEFINE_TYPE_CASTS(ImageBitmapRenderingContext,
                  CanvasRenderingContext,
                  context,
                  context->GetContextType() ==
                      CanvasRenderingContext::kContextImageBitmap,
                  context.GetContextType() ==
                      CanvasRenderingContext::kContextImageBitmap);

}  // namespace blink

#endif
