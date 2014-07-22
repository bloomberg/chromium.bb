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
#include "base/time/time.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
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
  TRACE_EVENT0("dri", "HandlePageFlipEvent");
  static_cast<HardwareDisplayController*>(controller)
      ->OnPageFlipEvent(frame, seconds, useconds);
}

const OverlayPlane& GetPrimaryPlane(const OverlayPlaneList& overlays) {
  for (size_t i = 0; i < overlays.size(); ++i) {
    if (overlays[i].z_order == 0)
      return overlays[i];
  }

  NOTREACHED();
  return overlays[0];
}

}  // namespace

OverlayPlane::OverlayPlane(scoped_refptr<ScanoutBuffer> buffer)
    : buffer(buffer),
      z_order(0),
      display_bounds(gfx::Point(), buffer->GetSize()),
      crop_rect(0, 0, 1, 1),
      overlay_plane(0) {}

OverlayPlane::OverlayPlane(scoped_refptr<ScanoutBuffer> buffer,
                           int z_order,
                           gfx::OverlayTransform plane_transform,
                           const gfx::Rect& display_bounds,
                           const gfx::RectF& crop_rect)
    : buffer(buffer),
      z_order(z_order),
      plane_transform(plane_transform),
      display_bounds(display_bounds),
      crop_rect(crop_rect),
      overlay_plane(0) {
}

OverlayPlane::~OverlayPlane() {}

HardwareDisplayController::HardwareDisplayController(
    DriWrapper* drm,
    uint32_t connector_id,
    uint32_t crtc_id)
    : drm_(drm),
      connector_id_(connector_id),
      crtc_id_(crtc_id),
      time_of_last_flip_(0),
      is_disabled_(true),
      saved_crtc_(drm_->GetCrtc(crtc_id_)) {}

HardwareDisplayController::~HardwareDisplayController() {
  if (!is_disabled_)
    drm_->SetCrtc(saved_crtc_.get(), &connector_id_);

  // Reset the cursor.
  UnsetCursor();
}

bool HardwareDisplayController::Modeset(const OverlayPlane& primary,
                                        drmModeModeInfo mode) {
  CHECK(primary.buffer);
  if (!drm_->SetCrtc(crtc_id_,
                     primary.buffer->GetFramebufferId(),
                     &connector_id_,
                     &mode)) {
    LOG(ERROR) << "Failed to modeset: error='" << strerror(errno)
               << "' crtc=" << crtc_id_ << " connector=" << connector_id_
               << " framebuffer_id=" << primary.buffer->GetFramebufferId()
               << " mode=" << mode.hdisplay << "x" << mode.vdisplay << "@"
               << mode.vrefresh;
    return false;
  }

  current_planes_ = std::vector<OverlayPlane>(1, primary);
  pending_planes_.clear();
  mode_ = mode;
  is_disabled_ = false;
  return true;
}

bool HardwareDisplayController::Enable() {
  OverlayPlane primary = GetPrimaryPlane(current_planes_);
  CHECK(primary.buffer);
  if (is_disabled_)
    return Modeset(primary, mode_);

  return true;
}

void HardwareDisplayController::Disable() {
  drm_->SetCrtc(crtc_id_, 0, 0, NULL);
  is_disabled_ = true;
}

bool HardwareDisplayController::SchedulePageFlip(
    const OverlayPlaneList& overlays) {
  CHECK_LE(1u, overlays.size());
  const OverlayPlane& primary = GetPrimaryPlane(overlays);
  CHECK(primary.buffer);

  if (!is_disabled_ &&
      !drm_->PageFlip(crtc_id_, primary.buffer->GetFramebufferId(), this)) {
    LOG(ERROR) << "Cannot page flip: " << strerror(errno);
    return false;
  }

  pending_planes_ = overlays;

  for (size_t i = 0; i < overlays.size(); i++) {
    const OverlayPlane& plane = overlays[i];
    if (!plane.overlay_plane)
      continue;
    const gfx::Size& size = plane.buffer->GetSize();
    gfx::RectF crop_rect = plane.crop_rect;
    crop_rect.Scale(size.width(), size.height());
    if (!drm_->PageFlipOverlay(crtc_id_,
                               plane.buffer->GetFramebufferId(),
                               plane.display_bounds,
                               crop_rect,
                               plane.overlay_plane)) {
      LOG(ERROR) << "Cannot display on overlay: " << strerror(errno);
      return false;
    }
  }

  return true;
}

void HardwareDisplayController::WaitForPageFlipEvent() {
  TRACE_EVENT0("dri", "WaitForPageFlipEvent");

  if (is_disabled_)
    return;

  drmEventContext drm_event;
  drm_event.version = DRM_EVENT_CONTEXT_VERSION;
  drm_event.page_flip_handler = HandlePageFlipEvent;
  drm_event.vblank_handler = NULL;

  // Wait for the page-flip to complete.
  drm_->HandleEvent(drm_event);
}

void HardwareDisplayController::OnPageFlipEvent(unsigned int frame,
                                                unsigned int seconds,
                                                unsigned int useconds) {
  time_of_last_flip_ =
      static_cast<uint64_t>(seconds) * base::Time::kMicrosecondsPerSecond +
      useconds;

  current_planes_ = pending_planes_;
  pending_planes_.clear();
}

bool HardwareDisplayController::SetCursor(scoped_refptr<ScanoutBuffer> buffer) {
  bool ret = drm_->SetCursor(crtc_id_,
                             buffer->GetHandle(),
                             buffer->GetSize());
  cursor_buffer_ = buffer;
  return ret;
}

bool HardwareDisplayController::UnsetCursor() {
  cursor_buffer_ = NULL;
  return drm_->SetCursor(crtc_id_, 0, gfx::Size());
}

bool HardwareDisplayController::MoveCursor(const gfx::Point& location) {
  if (is_disabled_)
    return true;

  return drm_->MoveCursor(crtc_id_, location);
}

}  // namespace ui
