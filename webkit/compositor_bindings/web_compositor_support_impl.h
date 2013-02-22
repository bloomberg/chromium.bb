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
class WebCompositorOutputSurface;
class WebGraphicsContext3D;
}

namespace webkit {

class WebCompositorSupportImpl : public WebKit::WebCompositorSupport {
 public:
  WebCompositorSupportImpl();
  virtual ~WebCompositorSupportImpl();

  virtual void initialize(WebKit::WebThread* implThread);
  virtual bool isThreadingEnabled();
  virtual void shutdown();
  virtual WebKit::WebCompositorOutputSurface* createOutputSurfaceFor3D(
      WebKit::WebGraphicsContext3D* context);
  virtual WebKit::WebCompositorOutputSurface* createOutputSurfaceForSoftware();
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
  virtual WebKit::WebTransformOperations*
    createTransformOperations();

  scoped_refptr<base::MessageLoopProxy> impl_thread_message_loop_proxy() {
    return impl_thread_message_loop_proxy_;
  }
 private:
  scoped_refptr<base::MessageLoopProxy> impl_thread_message_loop_proxy_;
  bool initialized_;
};

}  // namespace webkit

#endif  // WEBKIT_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_IMPL_H_
