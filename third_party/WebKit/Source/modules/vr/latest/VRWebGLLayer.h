// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRWebGLLayer_h
#define VRWebGLLayer_h

#include "bindings/modules/v8/webgl_rendering_context_or_webgl2_rendering_context.h"
#include "modules/vr/latest/VRLayer.h"
#include "modules/vr/latest/VRWebGLDrawingBuffer.h"
#include "modules/vr/latest/VRWebGLLayerInit.h"

namespace blink {

class WebGLFramebuffer;
class WebGLRenderingContextBase;
class VRSession;

class VRWebGLLayer final : public VRLayer {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VRWebGLLayer* Create(
      VRSession*,
      const WebGLRenderingContextOrWebGL2RenderingContext&,
      const VRWebGLLayerInit&);

  WebGLRenderingContextBase* context() const {
    return drawing_buffer_->webgl_context();
  }
  void getVRWebGLRenderingContext(
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

  VRViewport* GetViewport(VRView::Eye) override;

  void UpdateViewports();

  virtual void OnFrameStart();
  virtual void OnFrameEnd();

  virtual void Trace(blink::Visitor*);

 private:
  VRWebGLLayer(VRSession*, VRWebGLDrawingBuffer*);

  Member<VRViewport> left_viewport_;
  Member<VRViewport> right_viewport_;
  Member<VRWebGLDrawingBuffer> drawing_buffer_;

  double viewport_scale_ = 1.0;
  bool viewports_dirty_ = true;
};

}  // namespace blink

#endif  // VRWebGLLayer_h
