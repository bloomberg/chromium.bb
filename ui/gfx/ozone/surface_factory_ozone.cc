// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ozone/surface_factory_ozone.h"

#include <stdlib.h>

#include "base/command_line.h"
#include "ui/gfx/ozone/surface_ozone.h"

namespace gfx {

// static
SurfaceFactoryOzone* SurfaceFactoryOzone::impl_ = NULL;

SurfaceFactoryOzone::SurfaceFactoryOzone() {
}

SurfaceFactoryOzone::~SurfaceFactoryOzone() {
}

SurfaceFactoryOzone* SurfaceFactoryOzone::GetInstance() {
  CHECK(impl_) << "No SurfaceFactoryOzone implementation set.";
  return impl_;
}

void SurfaceFactoryOzone::SetInstance(SurfaceFactoryOzone* impl) {
  impl_ = impl;
}

intptr_t SurfaceFactoryOzone::GetNativeDisplay() {
  return 0;
}

scoped_ptr<SurfaceOzone> SurfaceFactoryOzone::CreateSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  NOTIMPLEMENTED();
  return scoped_ptr<SurfaceOzone>();
}

const int32* SurfaceFactoryOzone::GetEGLSurfaceProperties(
    const int32* desired_attributes) {
  return desired_attributes;
}


gfx::OverlayCandidatesOzone* SurfaceFactoryOzone::GetOverlayCandidates(
    gfx::AcceleratedWidget w) {
  return NULL;
}

void SurfaceFactoryOzone::ScheduleOverlayPlane(
    gfx::AcceleratedWidget w,
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    gfx::NativeBufferOzone buffer,
    const gfx::Rect& display_bounds,
    gfx::RectF crop_rect) {
  NOTREACHED();
}

gfx::NativeBufferOzone SurfaceFactoryOzone::CreateNativeBuffer(
    gfx::Size size,
    BufferFormat format) {
  return 0;
}

}  // namespace gfx
