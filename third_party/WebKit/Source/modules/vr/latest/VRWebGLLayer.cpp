// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/latest/VRWebGLLayer.h"

#include "modules/vr/latest/VRDevice.h"
#include "modules/vr/latest/VRSession.h"
#include "modules/vr/latest/VRView.h"
#include "modules/vr/latest/VRViewport.h"
#include "modules/webgl/WebGL2RenderingContext.h"
#include "modules/webgl/WebGLRenderingContext.h"

namespace blink {

namespace {

const double kDummyFramebufferWidth = 2048;
const double kDummyFramebufferHeight = 1024;

const double kFramebufferMinScale = 0.2;
const double kFramebufferMaxScale = 1.2;

const double kViewportMinScale = 0.2;
const double kViewportMaxScale = 1.0;

// Because including base::ClampToRange would be a dependency violation
double ClampToRange(const double value, const double min, const double max) {
  return std::min(std::max(value, min), max);
}

}  // namespace

VRWebGLLayer* VRWebGLLayer::Create(
    VRSession* session,
    const WebGLRenderingContextOrWebGL2RenderingContext& context,
    const VRWebGLLayerInit& initializer) {
  WebGLRenderingContextBase* webgl_context;
  if (context.IsWebGL2RenderingContext()) {
    webgl_context = context.GetAsWebGL2RenderingContext();
  } else {
    webgl_context = context.GetAsWebGLRenderingContext();
  }

  // TODO(bajones): Instantiate a WebGLFramebuffer here for use in the layer.

  return new VRWebGLLayer(session, webgl_context, initializer);
}

VRWebGLLayer::VRWebGLLayer(VRSession* session,
                           WebGLRenderingContextBase* webgl_context,
                           const VRWebGLLayerInit& initializer)
    : VRLayer(session, kVRWebGLLayerType),
      webgl_context_(webgl_context),
      antialias_(initializer.antialias()),
      depth_(initializer.depth()),
      stencil_(initializer.stencil()),
      alpha_(initializer.alpha()),
      multiview_(false) {  // Not supporting multiview yet.
  framebuffer_scale_ = session->DefaultFramebufferScale();

  if (initializer.hasFramebufferScaleFactor() &&
      initializer.framebufferScaleFactor() != 0.0) {
    // Clamp the developer-requested framebuffer size to ensure it's not too
    // small to see or unreasonably large.
    framebuffer_scale_ =
        ClampToRange(initializer.framebufferScaleFactor(), kFramebufferMinScale,
                     kFramebufferMaxScale);
  }

  // TODO(bajones): Will replace shortly with actual framebuffer dimensions.
  framebuffer_size_ = IntSize(kDummyFramebufferWidth * framebuffer_scale_,
                              kDummyFramebufferHeight * framebuffer_scale_);
}

void VRWebGLLayer::getVRWebGLRenderingContext(
    WebGLRenderingContextOrWebGL2RenderingContext& result) const {
  if (webgl_context_->Version() == 2) {
    result.SetWebGL2RenderingContext(
        static_cast<WebGL2RenderingContext*>(webgl_context_.Get()));
  } else {
    result.SetWebGLRenderingContext(
        static_cast<WebGLRenderingContext*>(webgl_context_.Get()));
  }
}

void VRWebGLLayer::requestViewportScaling(double scale_factor) {
  // Clamp the developer-requested viewport size to ensure it's not too
  // small to see or larger than the framebuffer.
  scale_factor =
      ClampToRange(scale_factor, kViewportMinScale, kViewportMaxScale);

  if (viewport_scale_ != scale_factor) {
    viewport_scale_ = scale_factor;
    viewports_dirty_ = true;
  }
}

VRViewport* VRWebGLLayer::GetViewport(VRView::Eye eye) {
  if (viewports_dirty_)
    UpdateViewports();

  if (eye == VRView::kEyeLeft)
    return left_viewport_;

  return right_viewport_;
}

void VRWebGLLayer::UpdateViewports() {
  long framebuffer_width = framebufferWidth();
  long framebuffer_height = framebufferHeight();

  if (session()->exclusive()) {
    left_viewport_ =
        new VRViewport(0, 0, framebuffer_width * 0.5 * viewport_scale_,
                       framebuffer_height * viewport_scale_);
    right_viewport_ =
        new VRViewport(framebuffer_width * 0.5 * viewport_scale_, 0,
                       framebuffer_width * 0.5 * viewport_scale_,
                       framebuffer_height * viewport_scale_);
  } else {
    left_viewport_ = new VRViewport(0, 0, framebuffer_width * viewport_scale_,
                                    framebuffer_height * viewport_scale_);
  }

  viewports_dirty_ = false;
}

void VRWebGLLayer::Trace(blink::Visitor* visitor) {
  visitor->Trace(left_viewport_);
  visitor->Trace(right_viewport_);
  visitor->Trace(webgl_context_);
  VRLayer::Trace(visitor);
}

}  // namespace blink
