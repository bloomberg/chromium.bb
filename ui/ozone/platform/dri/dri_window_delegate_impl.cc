// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_window_delegate_impl.h"

#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/drm_device_manager.h"
#include "ui/ozone/platform/dri/screen_manager.h"

namespace ui {

namespace {

#ifndef DRM_CAP_CURSOR_WIDTH
#define DRM_CAP_CURSOR_WIDTH 0x8
#endif

#ifndef DRM_CAP_CURSOR_HEIGHT
#define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

void UpdateCursorImage(DriBuffer* cursor, const SkBitmap& image) {
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

DriWindowDelegateImpl::DriWindowDelegateImpl(
    gfx::AcceleratedWidget widget,
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

DriWindowDelegateImpl::~DriWindowDelegateImpl() {
}

void DriWindowDelegateImpl::Initialize() {
  TRACE_EVENT1("dri", "DriWindowDelegateImpl::Initialize", "widget", widget_);

  device_manager_->UpdateDrmDevice(widget_, nullptr);
  screen_manager_->AddObserver(this);
  scoped_refptr<DriWrapper> drm =
      device_manager_->GetDrmDevice(gfx::kNullAcceleratedWidget);

  uint64_t cursor_width = 64;
  uint64_t cursor_height = 64;
  drm->GetCapability(DRM_CAP_CURSOR_WIDTH, &cursor_width);
  drm->GetCapability(DRM_CAP_CURSOR_HEIGHT, &cursor_height);

  SkImageInfo info = SkImageInfo::MakeN32Premul(cursor_width, cursor_height);
  for (size_t i = 0; i < arraysize(cursor_buffers_); ++i) {
    cursor_buffers_[i] = new DriBuffer(drm);
    if (!cursor_buffers_[i]->Initialize(info)) {
      LOG(ERROR) << "Failed to initialize cursor buffer";
      return;
    }
  }
}

void DriWindowDelegateImpl::Shutdown() {
  TRACE_EVENT1("dri", "DriWindowDelegateImpl::Shutdown", "widget", widget_);
  screen_manager_->RemoveObserver(this);
  device_manager_->RemoveDrmDevice(widget_);
}

gfx::AcceleratedWidget DriWindowDelegateImpl::GetAcceleratedWidget() {
  return widget_;
}

HardwareDisplayController* DriWindowDelegateImpl::GetController() {
  return controller_;
}

void DriWindowDelegateImpl::OnBoundsChanged(const gfx::Rect& bounds) {
  TRACE_EVENT2("dri", "DriWindowDelegateImpl::OnBoundsChanged", "widget",
               widget_, "bounds", bounds.ToString());
  bounds_ = bounds;
  controller_ = screen_manager_->GetDisplayController(bounds);
  UpdateWidgetToDrmDeviceMapping();
}

void DriWindowDelegateImpl::SetCursor(const std::vector<SkBitmap>& bitmaps,
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
        this, &DriWindowDelegateImpl::OnCursorAnimationTimeout);

  ResetCursor(false);
}

void DriWindowDelegateImpl::SetCursorWithoutAnimations(
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& location) {
  cursor_bitmaps_ = bitmaps;
  cursor_location_ = location;
  cursor_frame_ = 0;
  cursor_frame_delay_ms_ = 0;
  ResetCursor(false);
}

void DriWindowDelegateImpl::MoveCursor(const gfx::Point& location) {
  cursor_location_ = location;

  if (controller_)
    controller_->MoveCursor(location);
}

void DriWindowDelegateImpl::OnDisplayChanged(
    HardwareDisplayController* controller) {
  DCHECK(controller);

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
}

void DriWindowDelegateImpl::OnDisplayRemoved(
    HardwareDisplayController* controller) {
  if (controller_ == controller)
    controller_ = nullptr;
}

void DriWindowDelegateImpl::ResetCursor(bool bitmap_only) {
  if (cursor_bitmaps_.size()) {
    // Draw new cursor into backbuffer.
    UpdateCursorImage(cursor_buffers_[cursor_frontbuffer_ ^ 1].get(),
                      cursor_bitmaps_[cursor_frame_]);

    // Reset location & buffer.
    if (controller_) {
      if (!bitmap_only)
        controller_->MoveCursor(cursor_location_);
      controller_->SetCursor(cursor_buffers_[cursor_frontbuffer_ ^ 1]);
      cursor_frontbuffer_ ^= 1;
    }
  } else {
    // No cursor set.
    if (controller_)
      controller_->UnsetCursor();
  }
}

void DriWindowDelegateImpl::OnCursorAnimationTimeout() {
  cursor_frame_++;
  cursor_frame_ %= cursor_bitmaps_.size();

  ResetCursor(true);
}

void DriWindowDelegateImpl::UpdateWidgetToDrmDeviceMapping() {
  scoped_refptr<DriWrapper> drm = nullptr;
  if (controller_)
    drm = controller_->GetAllocationDriWrapper();

  device_manager_->UpdateDrmDevice(widget_, drm);
}

}  // namespace ui
