// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNinePatchLayerImpl_h
#define WebNinePatchLayerImpl_h

#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/compositor_bindings/web_layer_impl.h"

namespace WebKit {

class WebLayerImpl;

class WebNinePatchLayerImpl {
 public:
  WebNinePatchLayerImpl();
  virtual ~WebNinePatchLayerImpl();

  WebLayer* layer();

  void setBitmap(const SkBitmap& bitmap, const WebRect& aperture);

 private:
  scoped_ptr<WebLayerImpl> layer_;
};

}  // namespace WebKit

#endif
