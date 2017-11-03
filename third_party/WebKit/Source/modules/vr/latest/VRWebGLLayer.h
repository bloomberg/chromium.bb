// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRWebGLLayer_h
#define VRWebGLLayer_h

#include "bindings/modules/v8/webgl_rendering_context_or_webgl2_rendering_context.h"
#include "modules/vr/latest/VRLayer.h"
#include "modules/vr/latest/VRWebGLLayerInit.h"
#include "modules/webgl/WebGLRenderingContextBase.h"
#include "platform/geometry/IntSize.h"

namespace blink {

class VRSession;

class VRWebGLLayer final : public VRLayer {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VRWebGLLayer* Create(
      VRSession*,
      const WebGLRenderingContextOrWebGL2RenderingContext&,
      const VRWebGLLayerInit&);

  WebGLRenderingContextBase* context() const { return webgl_context_; }
  void getVRWebGLRenderingContext(
      WebGLRenderingContextOrWebGL2RenderingContext&) const;

  bool antialias() const { return antialias_; }
  bool depth() const { return depth_; }
  bool stencil() const { return stencil_; }
  bool alpha() const { return alpha_; }
  bool multiview() const { return multiview_; }

  unsigned long framebufferWidth() const { return framebuffer_size_.Width(); }
  unsigned long framebufferHeight() const { return framebuffer_size_.Height(); }

  void requestViewportScaling(double scale_factor);

  VRViewport* GetViewport(VRView::Eye) override;

  void UpdateViewports();

  virtual void Trace(blink::Visitor*);

 private:
  VRWebGLLayer(VRSession*, WebGLRenderingContextBase*, const VRWebGLLayerInit&);

  Member<VRViewport> left_viewport_;
  Member<VRViewport> right_viewport_;

  Member<WebGLRenderingContextBase> webgl_context_;

  bool antialias_;
  bool depth_;
  bool stencil_;
  bool alpha_;
  bool multiview_;

  IntSize framebuffer_size_;
  double framebuffer_scale_;
  double viewport_scale_ = 1.0;
  bool viewports_dirty_ = true;
};

}  // namespace blink

#endif  // VRWebGLLayer_h
