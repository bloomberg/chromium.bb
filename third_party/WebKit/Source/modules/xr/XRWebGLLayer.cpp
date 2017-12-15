// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/xr/XRWebGLLayer.h"

#include "modules/webgl/WebGL2RenderingContext.h"
#include "modules/webgl/WebGLFramebuffer.h"
#include "modules/webgl/WebGLRenderingContext.h"
#include "modules/xr/XRDevice.h"
#include "modules/xr/XRFrameProvider.h"
#include "modules/xr/XRSession.h"
#include "modules/xr/XRView.h"
#include "modules/xr/XRViewport.h"
#include "platform/geometry/DoubleSize.h"
#include "platform/geometry/IntSize.h"

namespace blink {

namespace {

const double kFramebufferMinScale = 0.2;
const double kFramebufferMaxScale = 1.2;

const double kViewportMinScale = 0.2;
const double kViewportMaxScale = 1.0;

// Because including base::ClampToRange would be a dependency violation
double ClampToRange(const double value, const double min, const double max) {
  return std::min(std::max(value, min), max);
}

}  // namespace

XRWebGLLayer* XRWebGLLayer::Create(
    XRSession* session,
    const WebGLRenderingContextOrWebGL2RenderingContext& context,
    const XRWebGLLayerInit& initializer) {
  WebGLRenderingContextBase* webgl_context;
  if (context.IsWebGL2RenderingContext()) {
    webgl_context = context.GetAsWebGL2RenderingContext();
  } else {
    webgl_context = context.GetAsWebGLRenderingContext();
  }

  bool want_antialiasing = initializer.antialias();
  bool want_depth_buffer = initializer.depth();
  bool want_stencil_buffer = initializer.stencil();
  bool want_alpha_channel = initializer.alpha();
  bool want_multiview = initializer.multiview();

  double framebuffer_scale = session->DefaultFramebufferScale();

  if (initializer.hasFramebufferScaleFactor() &&
      initializer.framebufferScaleFactor() != 0.0) {
    // Clamp the developer-requested framebuffer size to ensure it's not too
    // small to see or unreasonably large.
    framebuffer_scale =
        ClampToRange(initializer.framebufferScaleFactor(), kFramebufferMinScale,
                     kFramebufferMaxScale);
  }

  DoubleSize framebuffers_size = session->IdealFramebufferSize();

  IntSize desired_size(framebuffers_size.Width() * framebuffer_scale,
                       framebuffers_size.Height() * framebuffer_scale);

  XRWebGLDrawingBuffer* drawing_buffer = XRWebGLDrawingBuffer::Create(
      webgl_context, desired_size, want_alpha_channel, want_depth_buffer,
      want_stencil_buffer, want_antialiasing, want_multiview);

  if (!drawing_buffer) {
    return nullptr;
  }

  return new XRWebGLLayer(session, drawing_buffer);
}

XRWebGLLayer::XRWebGLLayer(XRSession* session,
                           XRWebGLDrawingBuffer* drawing_buffer)
    : XRLayer(session, kXRWebGLLayerType), drawing_buffer_(drawing_buffer) {
  DCHECK(drawing_buffer);
}

void XRWebGLLayer::getXRWebGLRenderingContext(
    WebGLRenderingContextOrWebGL2RenderingContext& result) const {
  WebGLRenderingContextBase* webgl_context = drawing_buffer_->webgl_context();
  if (webgl_context->Version() == 2) {
    result.SetWebGL2RenderingContext(
        static_cast<WebGL2RenderingContext*>(webgl_context));
  } else {
    result.SetWebGLRenderingContext(
        static_cast<WebGLRenderingContext*>(webgl_context));
  }
}

void XRWebGLLayer::requestViewportScaling(double scale_factor) {
  // Clamp the developer-requested viewport size to ensure it's not too
  // small to see or larger than the framebuffer.
  scale_factor =
      ClampToRange(scale_factor, kViewportMinScale, kViewportMaxScale);

  if (viewport_scale_ != scale_factor) {
    viewport_scale_ = scale_factor;
    viewports_dirty_ = true;
  }
}

XRViewport* XRWebGLLayer::GetViewport(XRView::Eye eye) {
  if (viewports_dirty_)
    UpdateViewports();

  if (eye == XRView::kEyeLeft)
    return left_viewport_;

  return right_viewport_;
}

void XRWebGLLayer::UpdateViewports() {
  long framebuffer_width = framebufferWidth();
  long framebuffer_height = framebufferHeight();

  if (session()->exclusive()) {
    left_viewport_ =
        new XRViewport(0, 0, framebuffer_width * 0.5 * viewport_scale_,
                       framebuffer_height * viewport_scale_);
    right_viewport_ =
        new XRViewport(framebuffer_width * 0.5 * viewport_scale_, 0,
                       framebuffer_width * 0.5 * viewport_scale_,
                       framebuffer_height * viewport_scale_);
  } else {
    left_viewport_ = new XRViewport(0, 0, framebuffer_width * viewport_scale_,
                                    framebuffer_height * viewport_scale_);
  }

  viewports_dirty_ = false;
}

void XRWebGLLayer::OnFrameStart() {
  drawing_buffer_->MarkFramebufferComplete(true);
}

void XRWebGLLayer::OnFrameEnd() {
  drawing_buffer_->MarkFramebufferComplete(false);
}

void XRWebGLLayer::Trace(blink::Visitor* visitor) {
  visitor->Trace(left_viewport_);
  visitor->Trace(right_viewport_);
  visitor->Trace(drawing_buffer_);
  XRLayer::Trace(visitor);
}

}  // namespace blink
