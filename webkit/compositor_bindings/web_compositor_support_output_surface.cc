// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "webkit/compositor_bindings/web_compositor_support_output_surface.h"

namespace webkit {

WebCompositorSupportOutputSurface::WebCompositorSupportOutputSurface(
    scoped_ptr<WebKit::WebGraphicsContext3D> context3d) {
  context3d_ = context3d.Pass();
}

WebCompositorSupportOutputSurface::WebCompositorSupportOutputSurface(
    scoped_ptr<cc::SoftwareOutputDevice> software_device) {
  software_device_ = software_device.Pass();
}

WebCompositorSupportOutputSurface::~WebCompositorSupportOutputSurface() {}

bool WebCompositorSupportOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  if (!context3d_)
    return true;
  DCHECK(client);
  return context3d_->makeContextCurrent();
}

const struct cc::OutputSurface::Capabilities&
    WebCompositorSupportOutputSurface::Capabilities() const {
  return capabilities_;
}

WebKit::WebGraphicsContext3D* WebCompositorSupportOutputSurface::Context3D() const {
  return context3d_.get();
}

cc::SoftwareOutputDevice* WebCompositorSupportOutputSurface::SoftwareDevice() const {
  return software_device_.get();
}

void WebCompositorSupportOutputSurface::SendFrameToParentCompositor(
    cc::CompositorFrame*) {
  // No support for delegated renderers in DumpRenderTree.
  NOTREACHED();
}

}  // namespace webkit
