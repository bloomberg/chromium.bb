// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_SCROLLBAR_LAYER_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_SCROLLBAR_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebScrollbarLayer.h"
#include "webkit/renderer/compositor_bindings/webkit_compositor_bindings_export.h"

namespace WebKit {
class WebScrollbar;
class WebScrollbarThemeGeometry;
class WebScrollbarThemePainter;
}

namespace webkit {

class WebLayerImpl;

class WebScrollbarLayerImpl : public WebKit::WebScrollbarLayer {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebScrollbarLayerImpl(
      WebKit::WebScrollbar* scrollbar,
      WebKit::WebScrollbarThemePainter painter,
      WebKit::WebScrollbarThemeGeometry* geometry);
  virtual ~WebScrollbarLayerImpl();

  // WebKit::WebScrollbarLayer implementation.
  virtual WebKit::WebLayer* layer();
  virtual void setScrollLayer(WebKit::WebLayer* layer);

 private:
  scoped_ptr<WebLayerImpl> layer_;

  DISALLOW_COPY_AND_ASSIGN(WebScrollbarLayerImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_SCROLLBAR_LAYER_IMPL_H_
