// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_CONTENT_LAYER_IMPL_H_
#define WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_CONTENT_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/layers/content_layer_client.h"
#include "third_party/WebKit/public/platform/WebContentLayer.h"
#include "webkit/renderer/compositor_bindings/web_layer_impl.h"
#include "webkit/renderer/compositor_bindings/webkit_compositor_bindings_export.h"

namespace cc {
class IntRect;
class FloatRect;
}

namespace WebKit { class WebContentLayerClient; }

namespace webkit {

class WebContentLayerImpl : public WebKit::WebContentLayer,
                            public cc::ContentLayerClient {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebContentLayerImpl(
      WebKit::WebContentLayerClient*);

  // WebContentLayer implementation.
  virtual WebKit::WebLayer* layer();
  virtual void setDoubleSided(bool double_sided);
  virtual void setDrawCheckerboardForMissingTiles(bool checkerboard);

 protected:
  virtual ~WebContentLayerImpl();

  // ContentLayerClient implementation.
  virtual void PaintContents(SkCanvas* canvas,
                             gfx::Rect clip,
                             gfx::RectF* opaque) OVERRIDE;
  virtual void DidChangeLayerCanUseLCDText() OVERRIDE;

  scoped_ptr<WebLayerImpl> layer_;
  WebKit::WebContentLayerClient* client_;
  bool draws_content_;

 private:
  bool can_use_lcd_text_;
  bool ignore_lcd_text_change_;

  DISALLOW_COPY_AND_ASSIGN(WebContentLayerImpl);
};

}  // namespace webkit

#endif  // WEBKIT_RENDERER_COMPOSITOR_BINDINGS_WEB_CONTENT_LAYER_IMPL_H_
