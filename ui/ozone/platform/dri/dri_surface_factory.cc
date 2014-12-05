// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_surface_factory.h"

#include <errno.h>

#include "base/debug/trace_event.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/dri/dri_surface.h"
#include "ui/ozone/platform/dri/dri_util.h"
#include "ui/ozone/platform/dri/dri_window_delegate_impl.h"
#include "ui/ozone/platform/dri/dri_window_delegate_manager.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

DriSurfaceFactory::DriSurfaceFactory(DriWrapper* drm,
                                     DriWindowDelegateManager* window_manager)
    : drm_(drm), window_manager_(window_manager) {
}

DriSurfaceFactory::~DriSurfaceFactory() {
}

scoped_ptr<ui::SurfaceOzoneCanvas> DriSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  return scoped_ptr<ui::SurfaceOzoneCanvas>(
      new DriSurface(window_manager_->GetWindowDelegate(widget), drm_));
}

bool DriSurfaceFactory::LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  return false;
}

}  // namespace ui
