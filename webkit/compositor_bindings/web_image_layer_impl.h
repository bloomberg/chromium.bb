// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMPOSITOR_BINDINGS_WEB_IMAGE_LAYER_IMPL_H_
#define WEBKIT_COMPOSITOR_BINDINGS_WEB_IMAGE_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebImageLayer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace webkit {

class WebLayerImpl;

class WebImageLayerImpl : public WebKit::WebImageLayer {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebImageLayerImpl();
  virtual ~WebImageLayerImpl();

  // WebKit::WebImageLayer implementation.
  virtual WebKit::WebLayer* layer();
  virtual void setBitmap(SkBitmap);

 private:
  scoped_ptr<WebLayerImpl> layer_;

  DISALLOW_COPY_AND_ASSIGN(WebImageLayerImpl);
};

}  // namespace webkit

#endif  // WEBKIT_COMPOSITOR_BINDINGS_WEB_IMAGE_LAYER_IMPL_H_
