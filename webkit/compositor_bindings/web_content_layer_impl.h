// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebContentLayerImpl_h
#define WebContentLayerImpl_h

#include "base/memory/scoped_ptr.h"
#include "cc/content_layer_client.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebContentLayer.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace cc {
class IntRect;
class FloatRect;
}

namespace WebKit {
class WebContentLayerClient;

class WebContentLayerImpl : public WebContentLayer,
                            public cc::ContentLayerClient {
 public:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebContentLayerImpl(
      WebContentLayerClient*);

  // WebContentLayer implementation.
  virtual WebLayer* layer() OVERRIDE;
  virtual void setDoubleSided(bool) OVERRIDE;
  virtual void setBoundsContainPageScale(bool) OVERRIDE;
  virtual bool boundsContainPageScale() const OVERRIDE;
  virtual void setUseLCDText(bool) OVERRIDE;
  virtual void setDrawCheckerboardForMissingTiles(bool) OVERRIDE;
  virtual void setAutomaticallyComputeRasterScale(bool);

 protected:
  virtual ~WebContentLayerImpl();

  // ContentLayerClient implementation.
  virtual void paintContents(SkCanvas*,
                             const gfx::Rect& clip,
                             gfx::RectF& opaque) OVERRIDE;

  scoped_ptr<WebLayerImpl> layer_;
  WebContentLayerClient* client_;
  bool draws_content_;
};

}  // namespace WebKit

#endif  // WebContentLayerImpl_h
