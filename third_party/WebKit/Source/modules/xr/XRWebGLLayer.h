// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRWebGLLayer_h
#define XRWebGLLayer_h

#include "bindings/modules/v8/webgl_rendering_context_or_webgl2_rendering_context.h"
#include "modules/xr/XRLayer.h"
#include "modules/xr/XRWebGLDrawingBuffer.h"
#include "modules/xr/XRWebGLLayerInit.h"

namespace blink {

class WebGLFramebuffer;
class WebGLRenderingContextBase;
class XRSession;

class XRWebGLLayer final : public XRLayer {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static XRWebGLLayer* Create(
      XRSession*,
      const WebGLRenderingContextOrWebGL2RenderingContext&,
      const XRWebGLLayerInit&);

  WebGLRenderingContextBase* context() const {
    return drawing_buffer_->webgl_context();
  }
  void getXRWebGLRenderingContext(
      WebGLRenderingContextOrWebGL2RenderingContext&) const;

  WebGLFramebuffer* framebuffer() const {
    return drawing_buffer_->framebuffer();
  }
  unsigned long framebufferWidth() const {
    return drawing_buffer_->size().Width();
  }
  unsigned long framebufferHeight() const {
    return drawing_buffer_->size().Height();
  }

  bool antialias() const { return drawing_buffer_->antialias(); }
  bool depth() const { return drawing_buffer_->depth(); }
  bool stencil() const { return drawing_buffer_->stencil(); }
  bool alpha() const { return drawing_buffer_->alpha(); }
  bool multiview() const { return drawing_buffer_->multiview(); }

  void requestViewportScaling(double scale_factor);

  XRViewport* GetViewport(XRView::Eye) override;

  void UpdateViewports();

  virtual void OnFrameStart();
  virtual void OnFrameEnd();

  virtual void Trace(blink::Visitor*);

 private:
  XRWebGLLayer(XRSession*, XRWebGLDrawingBuffer*);

  Member<XRViewport> left_viewport_;
  Member<XRViewport> right_viewport_;
  Member<XRWebGLDrawingBuffer> drawing_buffer_;

  double viewport_scale_ = 1.0;
  bool viewports_dirty_ = true;
};

}  // namespace blink

#endif  // XRWebGLLayer_h
