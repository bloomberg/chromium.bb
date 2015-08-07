// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_window.h"

#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/platform/drm/gpu/drm_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {

namespace {

#ifndef DRM_CAP_CURSOR_WIDTH
#define DRM_CAP_CURSOR_WIDTH 0x8
#endif

#ifndef DRM_CAP_CURSOR_HEIGHT
#define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

void EmptyFlipCallback(gfx::SwapResult) {
}

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
  canvas->drawBitmapRect(image, damage, NULL);
}

}  // namespace

DrmWindow::DrmWindow(gfx::AcceleratedWidget widget,
                     DrmDeviceManager* device_manager,
                     ScreenManager* screen_manager)
    : widget_(widget),
      device_manager_(device_manager),
      screen_manager_(screen_manager) {
}

DrmWindow::~DrmWindow() {
}

void DrmWindow::Initialize() {
  TRACE_EVENT1("drm", "DrmWindow::Initialize", "widget", widget_);

  device_manager_->UpdateDrmDevice(widget_, nullptr);
}

void DrmWindow::Shutdown() {
  TRACE_EVENT1("drm", "DrmWindow::Shutdown", "widget", widget_);
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
  if (bounds_.size() != bounds.size())
    last_submitted_planes_.clear();

  screen_manager_->UpdateControllerToWindowMapping();
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

void DrmWindow::QueueOverlayPlane(const OverlayPlane& plane) {
  pending_planes_.push_back(plane);
}

bool DrmWindow::SchedulePageFlip(bool is_sync,
                                 const SwapCompletionCallback& callback) {
  last_submitted_planes_.clear();
  last_submitted_planes_.swap(pending_planes_);
  last_swap_sync_ = is_sync;

  if (controller_) {
    return controller_->SchedulePageFlip(last_submitted_planes_, is_sync, false,
                                         callback);
  }

  callback.Run(gfx::SwapResult::SWAP_ACK);
  return true;
}

bool DrmWindow::TestPageFlip(const std::vector<OverlayCheck_Params>& overlays,
                             ScanoutBufferGenerator* buffer_generator) {
  if (!controller_)
    return true;
  for (const auto& overlay : overlays) {
    // It is possible that the cc rect we get actually falls off the edge of
    // the screen. Usually this is prevented via things like status bars
    // blocking overlaying or cc clipping it, but in case it wasn't properly
    // clipped (since GL will render this situation fine) just ignore it here.
    // This should be an extremely rare occurrance.
    if (overlay.plane_z_order != 0 && !bounds().Contains(overlay.display_rect))
      return false;
  }

  scoped_refptr<DrmDevice> drm = controller_->GetAllocationDrmDevice();
  OverlayPlaneList planes;
  for (const auto& overlay : overlays) {
    gfx::Size size =
        (overlay.plane_z_order == 0) ? bounds().size() : overlay.buffer_size;
    scoped_refptr<ScanoutBuffer> buffer = buffer_generator->Create(drm, size);
    if (!buffer)
      return false;
    planes.push_back(OverlayPlane(buffer, overlay.plane_z_order,
                                  overlay.transform, overlay.display_rect,
                                  gfx::RectF(gfx::Size(1, 1))));
  }
  return controller_->SchedulePageFlip(planes, true, true,
                                       base::Bind(&EmptyFlipCallback));
}

const OverlayPlane* DrmWindow::GetLastModesetBuffer() {
  return OverlayPlane::GetPrimaryPlane(last_submitted_planes_);
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

void DrmWindow::SetController(HardwareDisplayController* controller) {
  if (controller_ == controller)
    return;

  controller_ = controller;
  device_manager_->UpdateDrmDevice(
      widget_, controller ? controller->GetAllocationDrmDevice() : nullptr);

  UpdateCursorBuffers();
  // We changed displays, so we want to update the cursor as well.
  ResetCursor(false /* bitmap_only */);
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
