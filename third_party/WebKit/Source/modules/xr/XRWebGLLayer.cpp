// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/xr/XRWebGLLayer.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "modules/webgl/WebGL2RenderingContext.h"
#include "modules/webgl/WebGLFramebuffer.h"
#include "modules/webgl/WebGLRenderingContext.h"
#include "modules/xr/XRDevice.h"
#include "modules/xr/XRFrameProvider.h"
#include "modules/xr/XRPresentationContext.h"
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
    const XRWebGLLayerInit& initializer,
    ExceptionState& exception_state) {
  if (session->ended()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Cannot create an XRWebGLLayer for an "
                                      "XRSession which has already ended.");
    return nullptr;
  }

  WebGLRenderingContextBase* webgl_context;
  if (context.IsWebGL2RenderingContext()) {
    webgl_context = context.GetAsWebGL2RenderingContext();
  } else {
    webgl_context = context.GetAsWebGLRenderingContext();
  }

  if (webgl_context->isContextLost()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "Cannot create an XRWebGLLayer with a "
                                      "lost WebGL context.");
    return nullptr;
  }

  if (!webgl_context->IsXRDeviceCompatible(session->device())) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        "The session's device is not the compatible device for this context.");
    return nullptr;
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

  // Create an opaque WebGL Framebuffer
  WebGLFramebuffer* framebuffer = WebGLFramebuffer::CreateOpaque(webgl_context);

  scoped_refptr<XRWebGLDrawingBuffer> drawing_buffer =
      XRWebGLDrawingBuffer::Create(
          webgl_context->GetDrawingBuffer(), framebuffer->Object(),
          desired_size, want_alpha_channel, want_depth_buffer,
          want_stencil_buffer, want_antialiasing, want_multiview);

  if (!drawing_buffer) {
    exception_state.ThrowDOMException(kOperationError,
                                      "Unable to create a framebuffer.");
    return nullptr;
  }

  return new XRWebGLLayer(session, webgl_context, std::move(drawing_buffer),
                          framebuffer, framebuffer_scale);
}

XRWebGLLayer::XRWebGLLayer(XRSession* session,
                           WebGLRenderingContextBase* webgl_context,
                           scoped_refptr<XRWebGLDrawingBuffer> drawing_buffer,
                           WebGLFramebuffer* framebuffer,
                           double framebuffer_scale)
    : XRLayer(session, kXRWebGLLayerType),
      webgl_context_(webgl_context),
      drawing_buffer_(drawing_buffer),
      framebuffer_(framebuffer),
      framebuffer_scale_(framebuffer_scale) {
  DCHECK(drawing_buffer);
  UpdateViewports();
}

XRWebGLLayer::~XRWebGLLayer() {}

void XRWebGLLayer::getXRWebGLRenderingContext(
    WebGLRenderingContextOrWebGL2RenderingContext& result) const {
  if (webgl_context_->Version() == 2) {
    result.SetWebGL2RenderingContext(
        static_cast<WebGL2RenderingContext*>(webgl_context_.Get()));
  } else {
    result.SetWebGLRenderingContext(
        static_cast<WebGLRenderingContext*>(webgl_context_.Get()));
  }
}

void XRWebGLLayer::requestViewportScaling(double scale_factor) {
  if (!session()->exclusive()) {
    // TODO(bajones): For the moment we're just going to ignore viewport changes
    // in non-exclusive mode. This is legal, but probably not what developers
    // would like to see. Look into making viewport scale apply properly.
    scale_factor = 1.0;
  } else {
    // Clamp the developer-requested viewport size to ensure it's not too
    // small to see or larger than the framebuffer.
    scale_factor =
        ClampToRange(scale_factor, kViewportMinScale, kViewportMaxScale);
  }

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

  viewports_dirty_ = false;

  if (session()->exclusive()) {
    left_viewport_ =
        new XRViewport(0, 0, framebuffer_width * 0.5 * viewport_scale_,
                       framebuffer_height * viewport_scale_);
    right_viewport_ =
        new XRViewport(framebuffer_width * 0.5 * viewport_scale_, 0,
                       framebuffer_width * 0.5 * viewport_scale_,
                       framebuffer_height * viewport_scale_);

    session()->device()->frameProvider()->UpdateWebGLLayerViewports(this);
  } else {
    left_viewport_ = new XRViewport(0, 0, framebuffer_width * viewport_scale_,
                                    framebuffer_height * viewport_scale_);
  }
}

void XRWebGLLayer::OnFrameStart() {
  framebuffer_->MarkOpaqueBufferComplete(true);
  framebuffer_->SetContentsChanged(false);
}

void XRWebGLLayer::OnFrameEnd() {
  framebuffer_->MarkOpaqueBufferComplete(false);
  // Exit early if the framebuffer contents have not changed.
  if (!framebuffer_->HaveContentsChanged())
    return;

  // Submit the frame to the XR compositor.
  if (session()->exclusive()) {
    session()->device()->frameProvider()->SubmitWebGLLayer(this);
  } else if (session()->outputContext()) {
    ImageBitmap* image_bitmap =
        ImageBitmap::Create(TransferToStaticBitmapImage(nullptr));
    session()->outputContext()->SetImage(image_bitmap);
  }
}

void XRWebGLLayer::OnResize() {
  DoubleSize framebuffers_size = session()->IdealFramebufferSize();

  IntSize desired_size(framebuffers_size.Width() * framebuffer_scale_,
                       framebuffers_size.Height() * framebuffer_scale_);
  drawing_buffer_->Resize(desired_size);
  viewports_dirty_ = true;
}

scoped_refptr<StaticBitmapImage> XRWebGLLayer::TransferToStaticBitmapImage(
    std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback) {
  return drawing_buffer_->TransferToStaticBitmapImage(out_release_callback);
}

void XRWebGLLayer::Trace(blink::Visitor* visitor) {
  visitor->Trace(left_viewport_);
  visitor->Trace(right_viewport_);
  visitor->Trace(webgl_context_);
  visitor->Trace(framebuffer_);
  XRLayer::Trace(visitor);
}

void XRWebGLLayer::TraceWrappers(const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(webgl_context_);
  XRLayer::TraceWrappers(visitor);
}

}  // namespace blink
