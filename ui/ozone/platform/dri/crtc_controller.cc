// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/crtc_controller.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/scanout_buffer.h"

namespace ui {

CrtcController::CrtcController(DriWrapper* drm,
                               uint32_t crtc,
                               uint32_t connector)
    : drm_(drm),
      crtc_(crtc),
      connector_(connector),
      saved_crtc_(drm->GetCrtc(crtc)),
      is_disabled_(true),
      page_flip_pending_(false),
      time_of_last_flip_(0) {
}

CrtcController::~CrtcController() {
  if (!is_disabled_) {
    drm_->SetCrtc(saved_crtc_.get(), std::vector<uint32_t>(1, connector_));
    UnsetCursor();
  }
}

bool CrtcController::Modeset(const OverlayPlane& plane, drmModeModeInfo mode) {
  if (!drm_->SetCrtc(crtc_, plane.buffer->GetFramebufferId(),
                     std::vector<uint32_t>(1, connector_), &mode)) {
    LOG(ERROR) << "Failed to modeset: error='" << strerror(errno)
               << "' crtc=" << crtc_ << " connector=" << connector_
               << " framebuffer_id=" << plane.buffer->GetFramebufferId()
               << " mode=" << mode.hdisplay << "x" << mode.vdisplay << "@"
               << mode.vrefresh;
    return false;
  }

  current_planes_ = std::vector<OverlayPlane>(1, plane);
  mode_ = mode;
  pending_planes_.clear();
  is_disabled_ = false;
  page_flip_pending_ = false;

  return true;
}

bool CrtcController::Disable() {
  if (is_disabled_)
    return true;

  is_disabled_ = true;
  page_flip_pending_ = false;
  return drm_->DisableCrtc(crtc_);
}

bool CrtcController::SchedulePageFlip(const OverlayPlaneList& overlays) {
  DCHECK(!page_flip_pending_);
  DCHECK(!is_disabled_);
  const OverlayPlane& primary = OverlayPlane::GetPrimaryPlane(overlays);
  DCHECK(primary.buffer.get());

  if (primary.buffer->GetSize() != gfx::Size(mode_.hdisplay, mode_.vdisplay)) {
    LOG(WARNING) << "Trying to pageflip a buffer with the wrong size. Expected "
                 << mode_.hdisplay << "x" << mode_.vdisplay << " got "
                 << primary.buffer->GetSize().ToString() << " for"
                 << " crtc=" << crtc_ << " connector=" << connector_;
    return true;
  }

  if (!drm_->PageFlip(crtc_, primary.buffer->GetFramebufferId(), this)) {
    // Permission Denied is a legitimate error
    if (errno == EACCES)
      return true;
    LOG(ERROR) << "Cannot page flip: error='" << strerror(errno) << "'"
               << " crtc=" << crtc_
               << " framebuffer=" << primary.buffer->GetFramebufferId()
               << " size=" << primary.buffer->GetSize().ToString();
    return false;
  }

  page_flip_pending_ = true;
  pending_planes_ = overlays;

  for (size_t i = 0; i < overlays.size(); i++) {
    const OverlayPlane& plane = overlays[i];
    if (!plane.overlay_plane)
      continue;

    const gfx::Size& size = plane.buffer->GetSize();
    gfx::RectF crop_rect = plane.crop_rect;
    crop_rect.Scale(size.width(), size.height());
    if (!drm_->PageFlipOverlay(crtc_, plane.buffer->GetFramebufferId(),
                               plane.display_bounds, crop_rect,
                               plane.overlay_plane)) {
      LOG(ERROR) << "Cannot display on overlay: " << strerror(errno);
      return false;
    }
  }

  return true;
}

void CrtcController::OnPageFlipEvent(unsigned int frame,
                                     unsigned int seconds,
                                     unsigned int useconds) {
  page_flip_pending_ = false;
  time_of_last_flip_ =
      static_cast<uint64_t>(seconds) * base::Time::kMicrosecondsPerSecond +
      useconds;

  current_planes_.clear();
  current_planes_.swap(pending_planes_);
}

bool CrtcController::SetCursor(const scoped_refptr<ScanoutBuffer>& buffer) {
  DCHECK(!is_disabled_);
  cursor_buffer_ = buffer;
  return drm_->SetCursor(crtc_, buffer->GetHandle(), buffer->GetSize());
}

bool CrtcController::UnsetCursor() {
  bool state = drm_->SetCursor(crtc_, 0, gfx::Size());
  cursor_buffer_ = NULL;
  return state;
}

bool CrtcController::MoveCursor(const gfx::Point& location) {
  DCHECK(!is_disabled_);
  return drm_->MoveCursor(crtc_, location);
}

}  // namespace ui
