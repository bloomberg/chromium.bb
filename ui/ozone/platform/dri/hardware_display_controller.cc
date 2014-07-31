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
#include "ui/ozone/platform/dri/crtc_state.h"
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
    scoped_ptr<CrtcState> state)
    : drm_(drm),
      time_of_last_flip_(0),
      pending_page_flips_(0) {
  crtc_states_.push_back(state.release());
}

HardwareDisplayController::~HardwareDisplayController() {
  // Reset the cursor.
  UnsetCursor();
}

bool HardwareDisplayController::Modeset(const OverlayPlane& primary,
                                        drmModeModeInfo mode) {
  TRACE_EVENT0("dri", "HDC::Modeset");
  DCHECK(primary.buffer);
  pending_page_flips_ = 0;
  bool status = true;
  for (size_t i = 0; i < crtc_states_.size(); ++i)
    status &= ModesetCrtc(primary.buffer, mode, crtc_states_[i]);

  // Since a subset of controllers may be actively using |primary|, just keep
  // track of it.
  current_planes_ = std::vector<OverlayPlane>(1, primary);
  pending_planes_.clear();
  mode_ = mode;
  return status;
}

bool HardwareDisplayController::Enable() {
  TRACE_EVENT0("dri", "HDC::Enable");
  OverlayPlane primary = GetPrimaryPlane(current_planes_);
  DCHECK(primary.buffer);
  pending_page_flips_ = 0;
  bool status = true;
  for (size_t i = 0; i < crtc_states_.size(); ++i)
    status &= ModesetCrtc(primary.buffer, mode_, crtc_states_[i]);

  return status;
}

void HardwareDisplayController::Disable() {
  TRACE_EVENT0("dri", "HDC::Disable");
  pending_page_flips_ = 0;
  for (size_t i = 0; i < crtc_states_.size(); ++i) {
    drm_->DisableCrtc(crtc_states_[i]->crtc());
    crtc_states_[i]->set_is_disabled(true);
  }
}

bool HardwareDisplayController::SchedulePageFlip(
    const OverlayPlaneList& overlays) {
  DCHECK_LE(1u, overlays.size());
  DCHECK_EQ(0u, pending_page_flips_);

  pending_planes_ = overlays;
  bool status = true;
  for (size_t i = 0; i < crtc_states_.size(); ++i) {
    if (crtc_states_[i]->is_disabled())
      continue;

    status &= SchedulePageFlipOnCrtc(overlays, crtc_states_[i]);
  }

  return status;
}

void HardwareDisplayController::WaitForPageFlipEvent() {
  TRACE_EVENT1("dri", "HDC::WaitForPageFlipEvent",
               "pending_pageflips", pending_page_flips_);

  bool has_pending_page_flips = pending_page_flips_ != 0;
  drmEventContext drm_event;
  drm_event.version = DRM_EVENT_CONTEXT_VERSION;
  drm_event.page_flip_handler = HandlePageFlipEvent;
  drm_event.vblank_handler = NULL;

  // Wait for the page-flips to complete.
  while (pending_page_flips_ > 0)
    drm_->HandleEvent(drm_event);

  // In case there are no pending pageflips do not replace the current planes
  // since they are still being used.
  if (has_pending_page_flips)
    current_planes_.swap(pending_planes_);

  pending_planes_.clear();
}

void HardwareDisplayController::OnPageFlipEvent(unsigned int frame,
                                                unsigned int seconds,
                                                unsigned int useconds) {
  TRACE_EVENT0("dri", "HDC::OnPageFlipEvent");

  --pending_page_flips_;
  time_of_last_flip_ =
      static_cast<uint64_t>(seconds) * base::Time::kMicrosecondsPerSecond +
      useconds;
}

bool HardwareDisplayController::SetCursor(scoped_refptr<ScanoutBuffer> buffer) {
  bool status = true;
  cursor_buffer_ = buffer;
  for (size_t i = 0; i < crtc_states_.size(); ++i) {
    if (!crtc_states_[i]->is_disabled())
      status &= drm_->SetCursor(crtc_states_[i]->crtc(),
                                buffer->GetHandle(),
                                buffer->GetSize());
  }

  return status;
}

bool HardwareDisplayController::UnsetCursor() {
  bool status = true;
  cursor_buffer_ = NULL;
  for (size_t i = 0; i < crtc_states_.size(); ++i)
    status &= drm_->SetCursor(crtc_states_[i]->crtc(), 0, gfx::Size());

  return status;
}

bool HardwareDisplayController::MoveCursor(const gfx::Point& location) {
  bool status = true;
  for (size_t i = 0; i < crtc_states_.size(); ++i)
    if (!crtc_states_[i]->is_disabled())
      status &= drm_->MoveCursor(crtc_states_[i]->crtc(), location);

  return status;
}

void HardwareDisplayController::AddCrtc(
    scoped_ptr<CrtcState> state) {
  crtc_states_.push_back(state.release());
}

scoped_ptr<CrtcState> HardwareDisplayController::RemoveCrtc(uint32_t crtc) {
  for (ScopedVector<CrtcState>::iterator it = crtc_states_.begin();
       it != crtc_states_.end();
       ++it) {
    if ((*it)->crtc() == crtc) {
      scoped_ptr<CrtcState> controller(*it);
      crtc_states_.weak_erase(it);
      return controller.Pass();
    }
  }

  return scoped_ptr<CrtcState>();
}

bool HardwareDisplayController::HasCrtc(uint32_t crtc) const {
  for (size_t i = 0; i < crtc_states_.size(); ++i)
    if (crtc_states_[i]->crtc() == crtc)
      return true;

  return false;
}

bool HardwareDisplayController::HasCrtcs() const {
  return !crtc_states_.empty();
}

void HardwareDisplayController::RemoveMirroredCrtcs() {
  if (crtc_states_.size() > 1)
    crtc_states_.erase(crtc_states_.begin() + 1, crtc_states_.end());
}

bool HardwareDisplayController::ModesetCrtc(
    const scoped_refptr<ScanoutBuffer>& buffer,
    drmModeModeInfo mode,
    CrtcState* state) {
  if (!drm_->SetCrtc(state->crtc(),
                     buffer->GetFramebufferId(),
                     std::vector<uint32_t>(1, state->connector()),
                     &mode)) {
    LOG(ERROR) << "Failed to modeset: error='" << strerror(errno)
               << "' crtc=" << state->crtc()
               << " connector=" << state->connector()
               << " framebuffer_id=" << buffer->GetFramebufferId()
               << " mode=" << mode.hdisplay << "x" << mode.vdisplay << "@"
               << mode.vrefresh;
    return false;
  }

  state->set_is_disabled(false);
  return true;
}

bool HardwareDisplayController::SchedulePageFlipOnCrtc(
    const OverlayPlaneList& overlays,
    CrtcState* state) {
  const OverlayPlane& primary = GetPrimaryPlane(overlays);
  DCHECK(primary.buffer);

  if (primary.buffer->GetSize() != gfx::Size(mode_.hdisplay, mode_.vdisplay)) {
    LOG(WARNING) << "Trying to pageflip a buffer with the wrong size. Expected "
                 << mode_.hdisplay << "x" << mode_.vdisplay
                 << " got " << primary.buffer->GetSize().ToString() << " for"
                 << " crtc=" << state->crtc()
                 << " connector=" << state->connector();
    return true;
  }

  if (!drm_->PageFlip(state->crtc(),
                      primary.buffer->GetFramebufferId(),
                      this)) {
    LOG(ERROR) << "Cannot page flip: error='" << strerror(errno) << "'"
               << " crtc=" << state->crtc()
               << " framebuffer=" << primary.buffer->GetFramebufferId()
               << " size=" << primary.buffer->GetSize().ToString();
    return false;
  }

  ++pending_page_flips_;

  for (size_t i = 0; i < overlays.size(); i++) {
    const OverlayPlane& plane = overlays[i];
    if (!plane.overlay_plane)
      continue;

    const gfx::Size& size = plane.buffer->GetSize();
    gfx::RectF crop_rect = plane.crop_rect;
    crop_rect.Scale(size.width(), size.height());
    if (!drm_->PageFlipOverlay(state->crtc(),
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

}  // namespace ui
