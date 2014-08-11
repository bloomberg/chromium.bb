// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/surface_factory_ozone.h"

#include <stdlib.h>

#include "base/command_line.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {

// static
SurfaceFactoryOzone* SurfaceFactoryOzone::impl_ = NULL;

SurfaceFactoryOzone::SurfaceFactoryOzone() {
  DCHECK(!impl_) << "There should only be a single SurfaceFactoryOzone.";
  impl_ = this;
}

SurfaceFactoryOzone::~SurfaceFactoryOzone() {
  DCHECK_EQ(impl_, this);
  impl_ = NULL;
}

SurfaceFactoryOzone* SurfaceFactoryOzone::GetInstance() {
  DCHECK(impl_) << "No SurfaceFactoryOzone implementation set.";
  return impl_;
}

intptr_t SurfaceFactoryOzone::GetNativeDisplay() {
  return 0;
}

scoped_ptr<SurfaceOzoneEGL> SurfaceFactoryOzone::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  NOTIMPLEMENTED();
  return scoped_ptr<SurfaceOzoneEGL>();
}

scoped_ptr<SurfaceOzoneCanvas> SurfaceFactoryOzone::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  NOTIMPLEMENTED();
  return scoped_ptr<SurfaceOzoneCanvas>();
}

const int32* SurfaceFactoryOzone::GetEGLSurfaceProperties(
    const int32* desired_attributes) {
  return desired_attributes;
}

ui::OverlayCandidatesOzone* SurfaceFactoryOzone::GetOverlayCandidates(
    gfx::AcceleratedWidget w) {
  return NULL;
}

scoped_refptr<ui::NativePixmap> SurfaceFactoryOzone::CreateNativePixmap(
    gfx::Size size,
    BufferFormat format) {
  return NULL;
}

bool SurfaceFactoryOzone::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    scoped_refptr<NativePixmap> buffer,
    const gfx::Rect& display_bounds,
    const gfx::RectF& crop_rect) {
  return false;
}

bool SurfaceFactoryOzone::CanShowPrimaryPlaneAsOverlay() {
  return false;
}
}  // namespace ui
