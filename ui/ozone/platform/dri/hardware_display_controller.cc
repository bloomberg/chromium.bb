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
#include "ui/ozone/platform/dri/scanout_surface.h"
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

}  // namespace

OzoneOverlayPlane::OzoneOverlayPlane(ScanoutSurface* scanout,
                                     int z_order,
                                     gfx::OverlayTransform plane_transform,
                                     const gfx::Rect& display_bounds,
                                     const gfx::RectF& crop_rect)
    : scanout(scanout),
      z_order(z_order),
      plane_transform(plane_transform),
      display_bounds(display_bounds),
      crop_rect(crop_rect),
      overlay_plane(0) {
}

HardwareDisplayController::HardwareDisplayController(
    DriWrapper* drm,
    uint32_t connector_id,
    uint32_t crtc_id)
    : drm_(drm),
      connector_id_(connector_id),
      crtc_id_(crtc_id),
      surface_(),
      time_of_last_flip_(0),
      is_disabled_(true),
      saved_crtc_(drm_->GetCrtc(crtc_id_)) {}

HardwareDisplayController::~HardwareDisplayController() {
  if (!is_disabled_)
    drm_->SetCrtc(saved_crtc_.get(), &connector_id_);

  // Reset the cursor.
  UnsetCursor();
  UnbindSurfaceFromController();
}

bool
HardwareDisplayController::BindSurfaceToController(
    scoped_ptr<ScanoutSurface> surface, drmModeModeInfo mode) {
  CHECK(surface);

  if (!drm_->SetCrtc(crtc_id_,
                     surface->GetFramebufferId(),
                     &connector_id_,
                     &mode)) {
    LOG(ERROR) << "Failed to modeset: error='" << strerror(errno)
               << "' crtc=" << crtc_id_ << " connector=" << connector_id_
               << " framebuffer_id=" << surface->GetFramebufferId()
               << " mode=" << mode.hdisplay << "x" << mode.vdisplay << "@"
               << mode.vrefresh;
    return false;
  }

  surface_.reset(surface.release());
  mode_ = mode;
  is_disabled_ = false;
  return true;
}

void HardwareDisplayController::UnbindSurfaceFromController() {
  drm_->SetCrtc(crtc_id_, 0, 0, NULL);
  surface_.reset();
  memset(&mode_, 0, sizeof(mode_));
  is_disabled_ = true;
}

bool HardwareDisplayController::Enable() {
  CHECK(surface_);
  if (is_disabled_) {
    scoped_ptr<ScanoutSurface> surface(surface_.release());
    return BindSurfaceToController(surface.Pass(), mode_);
  }

  return true;
}

void HardwareDisplayController::Disable() {
  drm_->SetCrtc(crtc_id_, 0, 0, NULL);
  is_disabled_ = true;
}

ScanoutSurface* HardwareDisplayController::GetPrimaryPlane(
    const std::vector<OzoneOverlayPlane>& overlays) {
  ScanoutSurface* primary = surface_.get();
  for (size_t i = 0; i < overlays.size(); i++) {
    const OzoneOverlayPlane& plane = overlays[i];
    if (plane.z_order == 0) {
      return plane.scanout;
    }
  }

  return primary;
}

bool HardwareDisplayController::SchedulePageFlip(
    const std::vector<OzoneOverlayPlane>& overlays,
    NativePixmapList* references) {
  ScanoutSurface* primary = GetPrimaryPlane(overlays);
  CHECK(primary);

  primary->PreSwapBuffers();

  if (!is_disabled_ &&
      !drm_->PageFlip(crtc_id_, primary->GetFramebufferId(), this)) {
    LOG(ERROR) << "Cannot page flip: " << strerror(errno);
    return false;
  }

  current_overlay_references_.clear();
  if (references)
    current_overlay_references_.swap(*references);

  for (size_t i = 0; i < overlays.size(); i++) {
    const OzoneOverlayPlane& plane = overlays[i];
    if (!plane.overlay_plane)
      continue;
    const gfx::Size& size = plane.scanout->Size();
    gfx::RectF crop_rect = plane.crop_rect;
    crop_rect.Scale(size.width(), size.height());
    if (!drm_->PageFlipOverlay(crtc_id_,
                               plane.scanout->GetFramebufferId(),
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

  surface_->SwapBuffers();
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
