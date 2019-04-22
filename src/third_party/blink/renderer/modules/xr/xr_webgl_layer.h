// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_LAYER_H_

#include "third_party/blink/renderer/bindings/modules/v8/webgl_rendering_context_or_webgl2_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/modules/xr/xr_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_layer_init.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_webgl_drawing_buffer.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace viz {
class SingleReleaseCallback;
}

namespace blink {

class ExceptionState;
class WebGLFramebuffer;
class WebGLRenderingContextBase;
class XRSession;
class XRViewport;

class XRWebGLLayer final : public XRLayer {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRWebGLLayer(XRSession*,
               WebGLRenderingContextBase*,
               scoped_refptr<XRWebGLDrawingBuffer>,
               WebGLFramebuffer*,
               double framebuffer_scale,
               bool ignore_depth_values);
  ~XRWebGLLayer() override;

  static XRWebGLLayer* Create(
      XRSession*,
      const WebGLRenderingContextOrWebGL2RenderingContext&,
      const XRWebGLLayerInit*,
      ExceptionState&);

  WebGLRenderingContextBase* context() const { return webgl_context_; }
  void getXRWebGLRenderingContext(
      WebGLRenderingContextOrWebGL2RenderingContext&) const;

  WebGLFramebuffer* framebuffer() const { return framebuffer_; }
  uint32_t framebufferWidth() const { return drawing_buffer_->size().Width(); }
  uint32_t framebufferHeight() const {
    return drawing_buffer_->size().Height();
  }

  bool antialias() const { return drawing_buffer_->antialias(); }
  bool depth() const { return drawing_buffer_->depth(); }
  bool stencil() const { return drawing_buffer_->stencil(); }
  bool alpha() const { return drawing_buffer_->alpha(); }
  bool ignoreDepthValues() const { return ignore_depth_values_; }

  XRViewport* getViewport(XRView*);
  void requestViewportScaling(double scale_factor);

  static double getNativeFramebufferScaleFactor(XRSession* session);

  XRViewport* GetViewportForEye(XRView::XREye);

  void UpdateViewports();

  void OnFrameStart(const base::Optional<gpu::MailboxHolder>&) override;
  void OnFrameEnd() override;
  void OnResize() override;
  void HandleBackgroundImage(const gpu::MailboxHolder&,
                             const IntSize&) override;

  void OverwriteColorBufferFromMailboxTexture(const gpu::MailboxHolder&,
                                              const IntSize& size);

  void UpdateWebXRMirror();

  scoped_refptr<StaticBitmapImage> TransferToStaticBitmapImage(
      std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback);

  void Trace(blink::Visitor*) override;

 private:
  Member<XRViewport> left_viewport_;
  Member<XRViewport> right_viewport_;

  scoped_refptr<XRWebGLDrawingBuffer::MirrorClient> mirror_client_;

  Member<WebGLRenderingContextBase> webgl_context_;
  scoped_refptr<XRWebGLDrawingBuffer> drawing_buffer_;
  Member<WebGLFramebuffer> framebuffer_;

  double framebuffer_scale_ = 1.0;
  double requested_viewport_scale_ = 1.0;
  double viewport_scale_ = 1.0;
  bool viewports_dirty_ = true;
  bool can_mirror_ = false;
  bool is_direct_draw_frame = false;
  bool ignore_depth_values_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_LAYER_H_
