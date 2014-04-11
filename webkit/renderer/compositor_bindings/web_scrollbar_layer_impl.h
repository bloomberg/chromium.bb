// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_SCROLLBAR_LAYER_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_SCROLLBAR_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebScrollbar.h"
#include "third_party/WebKit/public/platform/WebScrollbarLayer.h"
#include "webkit/renderer/compositor_bindings/webkit_compositor_bindings_export.h"

namespace blink {
class WebScrollbarThemeGeometry;
class WebScrollbarThemePainter;
}

namespace webkit {

class WebLayerImpl;

class WebScrollbarLayerImpl : public blink::WebScrollbarLayer {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebScrollbarLayerImpl(
      blink::WebScrollbar* scrollbar,
      blink::WebScrollbarThemePainter painter,
      blink::WebScrollbarThemeGeometry* geometry);
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT WebScrollbarLayerImpl(
      blink::WebScrollbar::Orientation orientation,
      int thumb_thickness,
      int track_start,
      bool is_left_side_vertical_scrollbar);
  virtual ~WebScrollbarLayerImpl();

  // blink::WebScrollbarLayer implementation.
  virtual blink::WebLayer* layer();
  virtual void setScrollLayer(blink::WebLayer* layer);
  virtual void setClipLayer(blink::WebLayer* layer);

 private:
  scoped_ptr<WebLayerImpl> layer_;

  DISALLOW_COPY_AND_ASSIGN(WebScrollbarLayerImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_SCROLLBAR_LAYER_IMPL_H_
