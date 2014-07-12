// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/scoped_drm_types.h"

#include <xf86drmMode.h>

namespace ui {

void DrmResourcesDeleter::operator()(drmModeRes* resources) const {
  drmModeFreeResources(resources);
}

void DrmConnectorDeleter::operator()(drmModeConnector* connector) const {
  drmModeFreeConnector(connector);
}

void DrmCrtcDeleter::operator()(drmModeCrtc* crtc) const {
  drmModeFreeCrtc(crtc);
}

void DrmEncoderDeleter::operator()(drmModeEncoder* encoder) const {
  drmModeFreeEncoder(encoder);
}

void DrmObjectPropertiesDeleter::operator()(
    drmModeObjectProperties* properties) const {
  drmModeFreeObjectProperties(properties);
}

void DrmPlaneDeleter::operator()(drmModePlane* plane) const {
  drmModeFreePlane(plane);
}

void DrmPropertyDeleter::operator()(drmModePropertyRes* property) const {
  drmModeFreeProperty(property);
}

void DrmPropertyBlobDeleter::operator()(
    drmModePropertyBlobRes* property) const {
  drmModeFreePropertyBlob(property);
}

void DrmFramebufferDeleter::operator()(drmModeFB* framebuffer) const {
  drmModeFreeFB(framebuffer);
}

}  // namespace ui
