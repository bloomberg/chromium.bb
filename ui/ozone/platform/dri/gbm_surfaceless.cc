// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gbm_surfaceless.h"

#include "ui/ozone/platform/dri/dri_vsync_provider.h"
#include "ui/ozone/platform/dri/gbm_buffer.h"

namespace ui {

GbmSurfaceless::GbmSurfaceless(
    const base::WeakPtr<HardwareDisplayController>& controller)
    : controller_(controller) {}

GbmSurfaceless::~GbmSurfaceless() {}

intptr_t GbmSurfaceless::GetNativeWindow() {
  NOTREACHED();
  return 0;
}

bool GbmSurfaceless::ResizeNativeWindow(const gfx::Size& viewport_size) {
  NOTIMPLEMENTED();
  return false;
}

bool GbmSurfaceless::OnSwapBuffers() {
  if (!controller_)
    return false;

  bool success = controller_->SchedulePageFlip(queued_planes_);
  queued_planes_.clear();
  if (success)
    controller_->WaitForPageFlipEvent();

  return success;
}

scoped_ptr<gfx::VSyncProvider> GbmSurfaceless::CreateVSyncProvider() {
  return scoped_ptr<gfx::VSyncProvider>(new DriVSyncProvider(controller_));
}

bool GbmSurfaceless::ScheduleOverlayPlane(
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    scoped_refptr<ui::NativePixmap> buffer,
    const gfx::Rect& display_bounds,
    const gfx::RectF& crop_rect) {
  scoped_refptr<GbmPixmap> pixmap =
      static_cast<GbmPixmap*>(buffer.get());
  if (!pixmap) {
    LOG(ERROR) << "ScheduleOverlayPlane passed NULL buffer.";
    return false;
  }

  queued_planes_.push_back(OverlayPlane(pixmap->buffer(),
                                        plane_z_order,
                                        plane_transform,
                                        display_bounds,
                                        crop_rect));
  return true;
}

}  // namespace ui
