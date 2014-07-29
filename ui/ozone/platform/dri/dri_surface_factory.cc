// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_surface_factory.h"

#include <errno.h>

#include "base/debug/trace_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_surface.h"
#include "ui/ozone/platform/dri/dri_util.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

namespace {

// TODO(dnicoara) Read the cursor plane size from the hardware.
const gfx::Size kCursorSize(64, 64);

void UpdateCursorImage(DriBuffer* cursor, const SkBitmap& image) {
  SkRect damage;
  image.getBounds(&damage);

  // Clear to transparent in case |image| is smaller than the canvas.
  SkCanvas* canvas = cursor->GetCanvas();
  canvas->clear(SK_ColorTRANSPARENT);

  SkRect clip;
  clip.set(
      0, 0, canvas->getDeviceSize().width(), canvas->getDeviceSize().height());
  canvas->clipRect(clip, SkRegion::kReplace_Op);
  canvas->drawBitmapRectToRect(image, &damage, damage);
}

}  // namespace

// static
const gfx::AcceleratedWidget DriSurfaceFactory::kDefaultWidgetHandle = 1;

DriSurfaceFactory::DriSurfaceFactory(
    DriWrapper* drm,
    ScreenManager* screen_manager)
    : drm_(drm),
      screen_manager_(screen_manager),
      state_(UNINITIALIZED),
      allocated_widgets_(0),
      cursor_frontbuffer_(0) {
}

DriSurfaceFactory::~DriSurfaceFactory() {
  if (state_ == INITIALIZED)
    ShutdownHardware();
}

DriSurfaceFactory::HardwareState DriSurfaceFactory::InitializeHardware() {
  if (state_ != UNINITIALIZED)
    return state_;

  if (drm_->get_fd() < 0) {
    LOG(ERROR) << "Failed to create DRI connection";
    state_ = FAILED;
    return state_;
  }

  SkImageInfo info = SkImageInfo::MakeN32Premul(kCursorSize.width(),
                                                kCursorSize.height());
  for (size_t i = 0; i < arraysize(cursor_buffers_); ++i) {
    cursor_buffers_[i] = new DriBuffer(drm_);
    if (!cursor_buffers_[i]->Initialize(info)) {
      LOG(ERROR) << "Failed to initialize cursor buffer";
      state_ = FAILED;
      return state_;
    }
  }

  state_ = INITIALIZED;
  return state_;
}

void DriSurfaceFactory::ShutdownHardware() {
  DCHECK(state_ == INITIALIZED);
  state_ = UNINITIALIZED;
}

scoped_ptr<ui::SurfaceOzoneCanvas> DriSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget w) {
  DCHECK(state_ == INITIALIZED);
  // Initial cursor set.
  ResetCursor(w);

  return scoped_ptr<ui::SurfaceOzoneCanvas>(
      new DriSurface(drm_, screen_manager_->GetDisplayController(w)));
}

bool DriSurfaceFactory::LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  return false;
}

gfx::AcceleratedWidget DriSurfaceFactory::GetAcceleratedWidget() {
  DCHECK(state_ != FAILED);

  // We're not using 0 since other code assumes that a 0 AcceleratedWidget is an
  // invalid widget.
  return ++allocated_widgets_;
}

gfx::Size DriSurfaceFactory::GetWidgetSize(gfx::AcceleratedWidget w) {
  base::WeakPtr<HardwareDisplayController> controller =
      screen_manager_->GetDisplayController(w);
  if (controller)
    return gfx::Size(controller->get_mode().hdisplay,
                     controller->get_mode().vdisplay);

  return gfx::Size(0, 0);
}

void DriSurfaceFactory::SetHardwareCursor(gfx::AcceleratedWidget window,
                                          const SkBitmap& image,
                                          const gfx::Point& location) {
  cursor_bitmap_ = image;
  cursor_location_ = location;

  if (state_ != INITIALIZED)
    return;

  ResetCursor(window);
}

void DriSurfaceFactory::MoveHardwareCursor(gfx::AcceleratedWidget window,
                                           const gfx::Point& location) {
  cursor_location_ = location;

  if (state_ != INITIALIZED)
    return;

  base::WeakPtr<HardwareDisplayController> controller =
      screen_manager_->GetDisplayController(window);
  if (controller)
    controller->MoveCursor(location);
}

////////////////////////////////////////////////////////////////////////////////
// DriSurfaceFactory private

void DriSurfaceFactory::ResetCursor(gfx::AcceleratedWidget w) {
  base::WeakPtr<HardwareDisplayController> controller =
      screen_manager_->GetDisplayController(w);

  if (!cursor_bitmap_.empty()) {
    // Draw new cursor into backbuffer.
    UpdateCursorImage(cursor_buffers_[cursor_frontbuffer_ ^ 1].get(),
                      cursor_bitmap_);

    // Reset location & buffer.
    if (controller) {
      controller->MoveCursor(cursor_location_);
      controller->SetCursor(cursor_buffers_[cursor_frontbuffer_ ^ 1]);
      cursor_frontbuffer_ ^= 1;
    }
  } else {
    // No cursor set.
    if (controller)
      controller->UnsetCursor();
  }
}

}  // namespace ui
