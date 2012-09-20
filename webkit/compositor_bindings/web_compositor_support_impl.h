// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_IMPL_H_
#define WEBKIT_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_IMPL_H_

#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCompositorSupport.h"

namespace webkit {

class WebCompositorSupportImpl : public WebKit::WebCompositorSupport {
 public:
  WebCompositorSupportImpl();
  virtual ~WebCompositorSupportImpl();

  virtual void initialize(WebKit::WebThread* thread);
  virtual bool isThreadingEnabled();
  virtual void shutdown();
  virtual void setPerTilePaintingEnabled(bool enabled);
  virtual void setPartialSwapEnabled(bool enabled);
  virtual void setAcceleratedAnimationEnabled(bool enabled);
  virtual WebKit::WebLayerTreeView* createLayerTreeView(
      WebKit::WebLayerTreeViewClient* client, const WebKit::WebLayer& root,
      const WebKit::WebLayerTreeView::Settings& settings);
  virtual WebKit::WebLayer* createLayer();
  virtual WebKit::WebContentLayer* createContentLayer(
      WebKit::WebContentLayerClient* client);
  virtual WebKit::WebExternalTextureLayer*
    createExternalTextureLayer(WebKit::WebExternalTextureLayerClient* client);
  virtual WebKit::WebIOSurfaceLayer*
    createIOSurfaceLayer();
  virtual WebKit::WebImageLayer* createImageLayer();
  virtual WebKit::WebSolidColorLayer*
    createSolidColorLayer();
  virtual WebKit::WebVideoLayer*
    createVideoLayer(WebKit::WebVideoFrameProvider*);
  virtual WebKit::WebScrollbarLayer* createScrollbarLayer(
      WebKit::WebScrollbar* scrollbar,
      WebKit::WebScrollbarThemePainter painter,
      WebKit::WebScrollbarThemeGeometry*);
  virtual WebKit::WebAnimation* createAnimation(
      const WebKit::WebAnimationCurve& curve,
      WebKit::WebAnimation::TargetProperty target,
      int animationId);
  virtual WebKit::WebFloatAnimationCurve*
    createFloatAnimationCurve();
  virtual WebKit::WebTransformAnimationCurve*
    createTransformAnimationCurve();
};

}  // namespace webkit

#endif  // WEBKIT_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_IMPL_H_
