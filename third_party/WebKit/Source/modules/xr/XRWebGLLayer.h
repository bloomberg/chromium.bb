// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XRWebGLLayer_h
#define XRWebGLLayer_h

#include "bindings/modules/v8/webgl_rendering_context_or_webgl2_rendering_context.h"
#include "modules/webgl/WebGL2RenderingContext.h"
#include "modules/webgl/WebGLRenderingContext.h"
#include "modules/xr/XRLayer.h"
#include "modules/xr/XRView.h"
#include "modules/xr/XRWebGLLayerInit.h"
#include "platform/graphics/gpu/XRWebGLDrawingBuffer.h"
#include "platform/wtf/RefCounted.h"

namespace viz {
class SingleReleaseCallback;
}

namespace blink {

class ExceptionState;
class WebGLFramebuffer;
class WebGLRenderingContextBase;
class XRSession;
class XRViewport;

class XRWebGLLayer final : public XRLayer,
                           public XRWebGLDrawingBuffer::MirrorClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  virtual ~XRWebGLLayer();

  static XRWebGLLayer* Create(
      XRSession*,
      const WebGLRenderingContextOrWebGL2RenderingContext&,
      const XRWebGLLayerInit&,
      ExceptionState&);

  WebGLRenderingContextBase* context() const { return webgl_context_; }
  void getXRWebGLRenderingContext(
      WebGLRenderingContextOrWebGL2RenderingContext&) const;

  WebGLFramebuffer* framebuffer() const { return framebuffer_; }
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

  XRViewport* getViewport(XRView*);
  void requestViewportScaling(double scale_factor);

  XRViewport* GetViewportForEye(XRView::Eye);

  void UpdateViewports();

  void OnFrameStart() override;
  void OnFrameEnd() override;
  void OnResize() override;

  scoped_refptr<StaticBitmapImage> TransferToStaticBitmapImage(
      std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback);

  // XRWebGLDrawingBuffer::MirrorClient impementation
  void OnMirrorImageAvailable(
      scoped_refptr<StaticBitmapImage>,
      std::unique_ptr<viz::SingleReleaseCallback>) override;

  virtual void Trace(blink::Visitor*);
  virtual void TraceWrappers(const ScriptWrappableVisitor*) const;

 private:
  XRWebGLLayer(XRSession*,
               WebGLRenderingContextBase*,
               scoped_refptr<XRWebGLDrawingBuffer>,
               WebGLFramebuffer*,
               double framebuffer_scale);

  Member<XRViewport> left_viewport_;
  Member<XRViewport> right_viewport_;

  TraceWrapperMember<WebGLRenderingContextBase> webgl_context_;
  scoped_refptr<XRWebGLDrawingBuffer> drawing_buffer_;
  Member<WebGLFramebuffer> framebuffer_;

  std::unique_ptr<viz::SingleReleaseCallback> mirror_release_callback_;

  double framebuffer_scale_ = 1.0;
  double requested_viewport_scale_ = 1.0;
  double viewport_scale_ = 1.0;
  bool viewports_dirty_ = true;
  bool mirroring_ = false;
};

}  // namespace blink

#endif  // XRWebGLLayer_h
