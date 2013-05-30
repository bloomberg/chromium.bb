// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_SOLID_COLOR_LAYER_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_SOLID_COLOR_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebColor.h"
#include "third_party/WebKit/public/platform/WebSolidColorLayer.h"
#include "webkit/renderer/compositor_bindings/webkit_compositor_bindings_export.h"

namespace webkit {
class WebLayerImpl;

class WebSolidColorLayerImpl : public WebKit::WebSolidColorLayer {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebSolidColorLayerImpl();
  virtual ~WebSolidColorLayerImpl();

  // WebKit::WebSolidColorLayer implementation.
  virtual WebKit::WebLayer* layer();
  virtual void setBackgroundColor(WebKit::WebColor);

 private:
  scoped_ptr<WebLayerImpl> layer_;

  DISALLOW_COPY_AND_ASSIGN(WebSolidColorLayerImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_SOLID_COLOR_LAYER_IMPL_H_

