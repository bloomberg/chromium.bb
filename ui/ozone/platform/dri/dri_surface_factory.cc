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
#include "ui/ozone/platform/dri/dri_window_delegate_impl.h"
#include "ui/ozone/platform/dri/dri_window_delegate_manager.h"
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

DriSurfaceFactory::DriSurfaceFactory(DriWrapper* drm,
                                     ScreenManager* screen_manager,
                                     DriWindowDelegateManager* window_manager)
    : drm_(drm),
      screen_manager_(screen_manager),
      window_manager_(window_manager),
      state_(UNINITIALIZED),
      cursor_frontbuffer_(0),
      cursor_widget_(0),
      cursor_frame_(0),
      cursor_frame_delay_ms_(0) {
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
    gfx::AcceleratedWidget widget) {
  DCHECK(state_ == INITIALIZED);

  return scoped_ptr<ui::SurfaceOzoneCanvas>(
      new DriSurface(window_manager_->GetWindowDelegate(widget), drm_));
}

bool DriSurfaceFactory::LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  return false;
}

void DriSurfaceFactory::SetHardwareCursor(gfx::AcceleratedWidget widget,
                                          const std::vector<SkBitmap>& bitmaps,
                                          const gfx::Point& location,
                                          int frame_delay_ms) {
  cursor_widget_ = widget;
  cursor_bitmaps_ = bitmaps;
  cursor_location_ = location;
  cursor_frame_ = 0;
  cursor_frame_delay_ms_ = frame_delay_ms;
  cursor_timer_.Stop();

  if (cursor_frame_delay_ms_)
    cursor_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(cursor_frame_delay_ms_),
        this,
        &DriSurfaceFactory::OnCursorAnimationTimeout);

  if (state_ != INITIALIZED)
    return;

  ResetCursor();
}

void DriSurfaceFactory::MoveHardwareCursor(gfx::AcceleratedWidget widget,
                                           const gfx::Point& location) {
  cursor_location_ = location;

  if (state_ != INITIALIZED)
    return;

  HardwareDisplayController* controller =
      window_manager_->GetWindowDelegate(widget)->GetController();
  if (controller)
    controller->MoveCursor(location);
}

////////////////////////////////////////////////////////////////////////////////
// DriSurfaceFactory private

void DriSurfaceFactory::ResetCursor() {
  if (!cursor_widget_)
    return;

  HardwareDisplayController* controller =
      window_manager_->GetWindowDelegate(cursor_widget_)->GetController();
  if (cursor_bitmaps_.size()) {
    // Draw new cursor into backbuffer.
    UpdateCursorImage(cursor_buffers_[cursor_frontbuffer_ ^ 1].get(),
                      cursor_bitmaps_[cursor_frame_]);

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

void DriSurfaceFactory::OnCursorAnimationTimeout() {
  cursor_frame_++;
  cursor_frame_ %= cursor_bitmaps_.size();

  ResetCursor();
}

}  // namespace ui
