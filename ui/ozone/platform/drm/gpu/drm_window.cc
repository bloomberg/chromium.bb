// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_window.h"

#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/platform/drm/gpu/drm_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {

namespace {

#ifndef DRM_CAP_CURSOR_WIDTH
#define DRM_CAP_CURSOR_WIDTH 0x8
#endif

#ifndef DRM_CAP_CURSOR_HEIGHT
#define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

void UpdateCursorImage(DrmBuffer* cursor, const SkBitmap& image) {
  SkRect damage;
  image.getBounds(&damage);

  // Clear to transparent in case |image| is smaller than the canvas.
  SkCanvas* canvas = cursor->GetCanvas();
  canvas->clear(SK_ColorTRANSPARENT);

  SkRect clip;
  clip.set(0, 0, canvas->getDeviceSize().width(),
           canvas->getDeviceSize().height());
  canvas->clipRect(clip, SkRegion::kReplace_Op);
  canvas->drawBitmapRectToRect(image, &damage, damage);
}

}  // namespace

DrmWindow::DrmWindow(gfx::AcceleratedWidget widget,
                     DrmDeviceManager* device_manager,
                     ScreenManager* screen_manager)
    : widget_(widget),
      device_manager_(device_manager),
      screen_manager_(screen_manager),
      controller_(NULL),
      cursor_frontbuffer_(0),
      cursor_frame_(0),
      cursor_frame_delay_ms_(0) {
}

DrmWindow::~DrmWindow() {
}

void DrmWindow::Initialize() {
  TRACE_EVENT1("drm", "DrmWindow::Initialize", "widget", widget_);

  device_manager_->UpdateDrmDevice(widget_, nullptr);
  screen_manager_->AddObserver(this);
}

void DrmWindow::Shutdown() {
  TRACE_EVENT1("drm", "DrmWindow::Shutdown", "widget", widget_);
  screen_manager_->RemoveObserver(this);
  device_manager_->RemoveDrmDevice(widget_);
}

gfx::AcceleratedWidget DrmWindow::GetAcceleratedWidget() {
  return widget_;
}

HardwareDisplayController* DrmWindow::GetController() {
  return controller_;
}

void DrmWindow::OnBoundsChanged(const gfx::Rect& bounds) {
  TRACE_EVENT2("drm", "DrmWindow::OnBoundsChanged", "widget", widget_, "bounds",
               bounds.ToString());
  bounds_ = bounds;
  controller_ = screen_manager_->GetDisplayController(bounds);
  UpdateWidgetToDrmDeviceMapping();
  UpdateCursorBuffers();
}

void DrmWindow::SetCursor(const std::vector<SkBitmap>& bitmaps,
                          const gfx::Point& location,
                          int frame_delay_ms) {
  cursor_bitmaps_ = bitmaps;
  cursor_location_ = location;
  cursor_frame_ = 0;
  cursor_frame_delay_ms_ = frame_delay_ms;
  cursor_timer_.Stop();

  if (cursor_frame_delay_ms_)
    cursor_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(cursor_frame_delay_ms_),
        this, &DrmWindow::OnCursorAnimationTimeout);

  ResetCursor(false);
}

void DrmWindow::SetCursorWithoutAnimations(const std::vector<SkBitmap>& bitmaps,
                                           const gfx::Point& location) {
  cursor_bitmaps_ = bitmaps;
  cursor_location_ = location;
  cursor_frame_ = 0;
  cursor_frame_delay_ms_ = 0;
  ResetCursor(false);
}

void DrmWindow::MoveCursor(const gfx::Point& location) {
  cursor_location_ = location;

  if (controller_)
    controller_->MoveCursor(location);
}

void DrmWindow::OnDisplayChanged(HardwareDisplayController* controller) {
  DCHECK(controller);

  // If we have a new controller we need to re-allocate the buffers.
  bool should_allocate_cursor_buffers = controller_ != controller;

  gfx::Rect controller_bounds =
      gfx::Rect(controller->origin(), controller->GetModeSize());
  if (controller_) {
    if (controller_ != controller)
      return;

    if (controller->IsDisabled() || bounds_ != controller_bounds)
      controller_ = nullptr;
  } else {
    if (bounds_ == controller_bounds && !controller->IsDisabled())
      controller_ = controller;
  }

  UpdateWidgetToDrmDeviceMapping();
  if (should_allocate_cursor_buffers)
    UpdateCursorBuffers();
}

void DrmWindow::OnDisplayRemoved(HardwareDisplayController* controller) {
  if (controller_ == controller)
    controller_ = nullptr;
}

void DrmWindow::ResetCursor(bool bitmap_only) {
  if (!controller_)
    return;

  if (cursor_bitmaps_.size()) {
    // Draw new cursor into backbuffer.
    UpdateCursorImage(cursor_buffers_[cursor_frontbuffer_ ^ 1].get(),
                      cursor_bitmaps_[cursor_frame_]);

    // Reset location & buffer.
    if (!bitmap_only)
      controller_->MoveCursor(cursor_location_);
    controller_->SetCursor(cursor_buffers_[cursor_frontbuffer_ ^ 1]);
    cursor_frontbuffer_ ^= 1;
  } else {
    // No cursor set.
    controller_->UnsetCursor();
  }
}

void DrmWindow::OnCursorAnimationTimeout() {
  cursor_frame_++;
  cursor_frame_ %= cursor_bitmaps_.size();

  ResetCursor(true);
}

void DrmWindow::UpdateWidgetToDrmDeviceMapping() {
  scoped_refptr<DrmDevice> drm = nullptr;
  if (controller_)
    drm = controller_->GetAllocationDrmDevice();

  device_manager_->UpdateDrmDevice(widget_, drm);
}

void DrmWindow::UpdateCursorBuffers() {
  if (!controller_) {
    for (size_t i = 0; i < arraysize(cursor_buffers_); ++i) {
      cursor_buffers_[i] = nullptr;
    }
  } else {
    scoped_refptr<DrmDevice> drm = controller_->GetAllocationDrmDevice();

    uint64_t cursor_width = 64;
    uint64_t cursor_height = 64;
    drm->GetCapability(DRM_CAP_CURSOR_WIDTH, &cursor_width);
    drm->GetCapability(DRM_CAP_CURSOR_HEIGHT, &cursor_height);

    SkImageInfo info = SkImageInfo::MakeN32Premul(cursor_width, cursor_height);
    for (size_t i = 0; i < arraysize(cursor_buffers_); ++i) {
      cursor_buffers_[i] = new DrmBuffer(drm);
      // Don't register a framebuffer for cursors since they are special (they
      // aren't modesetting buffers and drivers may fail to register them due to
      // their small sizes).
      if (!cursor_buffers_[i]->Initialize(
              info, false /* should_register_framebuffer */)) {
        LOG(FATAL) << "Failed to initialize cursor buffer";
        return;
      }
    }
  }
}

}  // namespace ui
