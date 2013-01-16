// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_OUTPUT_SURFACE
#define WEBKIT_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_OUTPUT_SURFACE

#include "base/memory/scoped_ptr.h"
#include "cc/output_surface.h"
#include "webkit/compositor_bindings/web_compositor_support_software_output_device.h"
#include "webkit/compositor_bindings/webkit_compositor_bindings_export.h"

namespace webkit {

class WebCompositorSupportOutputSurface : public cc::OutputSurface {
 public:

  static inline scoped_ptr<WebCompositorSupportOutputSurface> Create3d(
      scoped_ptr<WebKit::WebGraphicsContext3D> context3d) {
    return make_scoped_ptr(
        new WebCompositorSupportOutputSurface(context3d.Pass()));
  }

  static inline scoped_ptr<WebCompositorSupportOutputSurface> CreateSoftware(
      scoped_ptr<cc::SoftwareOutputDevice> software_device) {
    return make_scoped_ptr(
        new WebCompositorSupportOutputSurface(software_device.Pass()));
  }

  virtual ~WebCompositorSupportOutputSurface();

  virtual bool BindToClient(cc::OutputSurfaceClient*) OVERRIDE;

  virtual const struct Capabilities& Capabilities() const OVERRIDE;

  virtual WebKit::WebGraphicsContext3D* Context3D() const OVERRIDE;
  virtual cc::SoftwareOutputDevice* SoftwareDevice() const OVERRIDE;

  virtual void SendFrameToParentCompositor(cc::CompositorFrame*) OVERRIDE;

private:
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebCompositorSupportOutputSurface(
      scoped_ptr<WebKit::WebGraphicsContext3D> context3d);
  WEBKIT_COMPOSITOR_BINDINGS_EXPORT explicit WebCompositorSupportOutputSurface(
      scoped_ptr<cc::SoftwareOutputDevice> software_device);

  struct cc::OutputSurface::Capabilities capabilities_;
  scoped_ptr<WebKit::WebGraphicsContext3D> context3d_;
  scoped_ptr<cc::SoftwareOutputDevice> software_device_;
};

}  // namespace webkit

#endif  // WEBKIT_COMPOSITOR_BINDINGS_WEB_COMPOSITOR_SUPPORT_OUTPUT_SURFACE
