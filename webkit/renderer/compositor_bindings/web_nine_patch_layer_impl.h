// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_NINE_PATCH_LAYER_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_NINE_PATCH_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebNinePatchLayer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/renderer/compositor_bindings/webkit_compositor_bindings_export.h"

namespace webkit {

class WebLayerImpl;

class WebNinePatchLayerImpl : public blink::WebNinePatchLayer {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebNinePatchLayerImpl();
  virtual ~WebNinePatchLayerImpl();

  // blink::WebNinePatchLayer implementation.
  virtual blink::WebLayer* layer();

  // TODO(ccameron): Remove setBitmap(SkBitmap, blink::WebRect) in favor of
  // setBitmap(), setAperture(), and setBorder();
  virtual void setBitmap(SkBitmap bitmap, const blink::WebRect& aperture);
  virtual void setBitmap(SkBitmap bitmap);
  virtual void setAperture(const blink::WebRect& aperture);
  virtual void setBorder(const blink::WebRect& border);
  virtual void setFillCenter(bool fill_center);

 private:
  scoped_ptr<WebLayerImpl> layer_;

  DISALLOW_COPY_AND_ASSIGN(WebNinePatchLayerImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_NINE_PATCH_LAYER_IMPL_H_
