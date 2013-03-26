// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_IMPL_H_
#define WEBKIT_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCompositorSupport.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebTransformOperations.h"

namespace WebKit {
class WebGraphicsContext3D;
}

namespace webkit {

class WebCompositorSupportImpl : public WebKit::WebCompositorSupport {
 public:
  WebCompositorSupportImpl();
  virtual ~WebCompositorSupportImpl();

  virtual WebKit::WebLayer* createLayer();
  virtual WebKit::WebContentLayer* createContentLayer(
      WebKit::WebContentLayerClient* client);
  virtual WebKit::WebExternalTextureLayer* createExternalTextureLayer(
      WebKit::WebExternalTextureLayerClient* client);
  virtual WebKit::WebImageLayer* createImageLayer();
  virtual WebKit::WebSolidColorLayer* createSolidColorLayer();
  virtual WebKit::WebScrollbarLayer* createScrollbarLayer(
      WebKit::WebScrollbar* scrollbar,
      WebKit::WebScrollbarThemePainter painter,
      WebKit::WebScrollbarThemeGeometry*);
  virtual WebKit::WebAnimation* createAnimation(
      const WebKit::WebAnimationCurve& curve,
      WebKit::WebAnimation::TargetProperty target,
      int animation_id);
  virtual WebKit::WebFloatAnimationCurve* createFloatAnimationCurve();
  virtual WebKit::WebTransformAnimationCurve* createTransformAnimationCurve();
  virtual WebKit::WebTransformOperations* createTransformOperations();

 private:
  DISALLOW_COPY_AND_ASSIGN(WebCompositorSupportImpl);
};

}  // namespace webkit

#endif  // WEBKIT_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_IMPL_H_
