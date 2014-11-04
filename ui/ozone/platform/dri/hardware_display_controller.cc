// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/hardware_display_controller.h"

#include <drm.h>
#include <errno.h>
#include <string.h>
#include <xf86drm.h>

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/dri/crtc_controller.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/public/native_pixmap.h"

namespace ui {

namespace {

// DRM callback on page flip events. This callback is triggered after the
// page flip has happened and the backbuffer is now the new frontbuffer
// The old frontbuffer is no longer used by the hardware and can be used for
// future draw operations.
//
// |device| will contain a reference to the |ScanoutSurface| object which
// the event belongs to.
//
// TODO(dnicoara) When we have a FD handler for the DRM calls in the message
// loop, we can move this function in the handler.
void HandlePageFlipEvent(int fd,
                         unsigned int frame,
                         unsigned int seconds,
                         unsigned int useconds,
                         void* controller) {
  static_cast<CrtcController*>(controller)
      ->OnPageFlipEvent(frame, seconds, useconds);
}

}  // namespace

HardwareDisplayController::HardwareDisplayController(
    scoped_ptr<CrtcController> controller)
    : is_disabled_(true) {
  crtc_controllers_.push_back(controller.release());
}

HardwareDisplayController::~HardwareDisplayController() {
  // Reset the cursor.
  UnsetCursor();
}

bool HardwareDisplayController::Modeset(const OverlayPlane& primary,
                                        drmModeModeInfo mode) {
  TRACE_EVENT0("dri", "HDC::Modeset");
  DCHECK(primary.buffer.get());
  bool status = true;
  for (size_t i = 0; i < crtc_controllers_.size(); ++i)
    status &= crtc_controllers_[i]->Modeset(primary, mode);

  current_planes_ = std::vector<OverlayPlane>(1, primary);
  pending_planes_.clear();
  is_disabled_ = false;
  mode_ = mode;
  return status;
}

bool HardwareDisplayController::Enable() {
  TRACE_EVENT0("dri", "HDC::Enable");
  DCHECK(!current_planes_.empty());
  OverlayPlane primary = OverlayPlane::GetPrimaryPlane(current_planes_);
  DCHECK(primary.buffer.get());
  bool status = true;
  for (size_t i = 0; i < crtc_controllers_.size(); ++i)
    status &= crtc_controllers_[i]->Modeset(primary, mode_);

  return status;
}

void HardwareDisplayController::Disable() {
  TRACE_EVENT0("dri", "HDC::Disable");
  for (size_t i = 0; i < crtc_controllers_.size(); ++i)
    crtc_controllers_[i]->Disable();

  is_disabled_ = true;
}

void HardwareDisplayController::QueueOverlayPlane(const OverlayPlane& plane) {
  pending_planes_.push_back(plane);
}

bool HardwareDisplayController::SchedulePageFlip() {
  DCHECK(!pending_planes_.empty());

  if (is_disabled_)
    return true;

  bool status = true;
  for (size_t i = 0; i < crtc_controllers_.size(); ++i)
    status &= crtc_controllers_[i]->SchedulePageFlip(pending_planes_);

  return status;
}

void HardwareDisplayController::WaitForPageFlipEvent() {
  TRACE_EVENT0("dri", "HDC::WaitForPageFlipEvent");

  drmEventContext drm_event;
  drm_event.version = DRM_EVENT_CONTEXT_VERSION;
  drm_event.page_flip_handler = HandlePageFlipEvent;
  drm_event.vblank_handler = NULL;

  bool has_pending_page_flips = false;
  // Wait for the page-flips to complete.
  for (size_t i = 0; i < crtc_controllers_.size(); ++i) {
    // In mirror mode the page flip callbacks can happen in different order than
    // scheduled, so we need to make sure that the event for the current CRTC is
    // processed before moving to the next CRTC.
    while (crtc_controllers_[i]->page_flip_pending()) {
      has_pending_page_flips = true;
      crtc_controllers_[i]->drm()->HandleEvent(drm_event);
    }
  }

  // In case there are no pending pageflips do not replace the current planes
  // since they are still being used.
  if (has_pending_page_flips)
    current_planes_.swap(pending_planes_);

  pending_planes_.clear();
}

bool HardwareDisplayController::SetCursor(
    const scoped_refptr<ScanoutBuffer>& buffer) {
  bool status = true;

  if (is_disabled_)
    return true;

  for (size_t i = 0; i < crtc_controllers_.size(); ++i)
    status &= crtc_controllers_[i]->SetCursor(buffer);

  return status;
}

bool HardwareDisplayController::UnsetCursor() {
  bool status = true;
  for (size_t i = 0; i < crtc_controllers_.size(); ++i)
    status &= crtc_controllers_[i]->UnsetCursor();

  return status;
}

bool HardwareDisplayController::MoveCursor(const gfx::Point& location) {
  if (is_disabled_)
    return true;

  bool status = true;
  for (size_t i = 0; i < crtc_controllers_.size(); ++i)
    status &= crtc_controllers_[i]->MoveCursor(location);

  return status;
}

void HardwareDisplayController::AddCrtc(scoped_ptr<CrtcController> controller) {
  crtc_controllers_.push_back(controller.release());
}

scoped_ptr<CrtcController> HardwareDisplayController::RemoveCrtc(
    uint32_t crtc) {
  for (ScopedVector<CrtcController>::iterator it = crtc_controllers_.begin();
       it != crtc_controllers_.end(); ++it) {
    if ((*it)->crtc() == crtc) {
      scoped_ptr<CrtcController> controller(*it);
      crtc_controllers_.weak_erase(it);
      return controller.Pass();
    }
  }

  return scoped_ptr<CrtcController>();
}

bool HardwareDisplayController::HasCrtc(uint32_t crtc) const {
  for (size_t i = 0; i < crtc_controllers_.size(); ++i)
    if (crtc_controllers_[i]->crtc() == crtc)
      return true;

  return false;
}

bool HardwareDisplayController::IsMirrored() const {
  return crtc_controllers_.size() > 1;
}

bool HardwareDisplayController::IsDisabled() const {
  return is_disabled_;
}

gfx::Size HardwareDisplayController::GetModeSize() const {
  return gfx::Size(mode_.hdisplay, mode_.vdisplay);
}

uint64_t HardwareDisplayController::GetTimeOfLastFlip() const {
  uint64_t time = 0;
  for (size_t i = 0; i < crtc_controllers_.size(); ++i)
    if (time < crtc_controllers_[i]->time_of_last_flip())
      time = crtc_controllers_[i]->time_of_last_flip();

  return time;
}

}  // namespace ui
