// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/drm_surface_factory.h"

#include <errno.h>

#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/gpu/drm_surface.h"
#include "ui/ozone/platform/drm/gpu/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/drm_window_manager.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

DrmSurfaceFactory::DrmSurfaceFactory(DrmWindowManager* window_manager)
    : window_manager_(window_manager) {
}

DrmSurfaceFactory::~DrmSurfaceFactory() {
}

scoped_ptr<ui::SurfaceOzoneCanvas> DrmSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  return make_scoped_ptr(
      new DrmSurface(window_manager_->GetWindowDelegate(widget)));
}

bool DrmSurfaceFactory::LoadEGLGLES2Bindings(
    AddGLLibraryCallback add_gl_library,
    SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  return false;
}

}  // namespace ui
