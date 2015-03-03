// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/crtc_controller.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "ui/ozone/platform/dri/drm_device.h"
#include "ui/ozone/platform/dri/page_flip_observer.h"
#include "ui/ozone/platform/dri/scanout_buffer.h"

namespace ui {

CrtcController::CrtcController(const scoped_refptr<DrmDevice>& drm,
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

  mode_ = mode;
  pending_planes_.clear();
  is_disabled_ = false;

  // drmModeSetCrtc has an immediate effect, so we can assume that the current
  // planes have been updated. However if a page flip is still pending, set the
  // pending planes to the same values so that the callback keeps the correct
  // state.
  current_planes_ = std::vector<OverlayPlane>(1, plane);
  if (page_flip_pending_)
    pending_planes_ = current_planes_;

  return true;
}

bool CrtcController::Disable() {
  if (is_disabled_)
    return true;

  is_disabled_ = true;
  return drm_->DisableCrtc(crtc_);
}

bool CrtcController::SchedulePageFlip(HardwareDisplayPlaneList* plane_list,
                                      const OverlayPlaneList& overlays) {
  DCHECK(!page_flip_pending_);
  DCHECK(!is_disabled_);
  const OverlayPlane* primary = OverlayPlane::GetPrimaryPlane(overlays);
  if (!primary) {
    LOG(ERROR) << "No primary plane to display on crtc " << crtc_;
    FOR_EACH_OBSERVER(PageFlipObserver, observers_, OnPageFlipEvent());
    return true;
  }
  DCHECK(primary->buffer.get());

  if (primary->buffer->GetSize() != gfx::Size(mode_.hdisplay, mode_.vdisplay)) {
    LOG(WARNING) << "Trying to pageflip a buffer with the wrong size. Expected "
                 << mode_.hdisplay << "x" << mode_.vdisplay << " got "
                 << primary->buffer->GetSize().ToString() << " for"
                 << " crtc=" << crtc_ << " connector=" << connector_;
    FOR_EACH_OBSERVER(PageFlipObserver, observers_, OnPageFlipEvent());
    return true;
  }

  if (!drm_->plane_manager()->AssignOverlayPlanes(plane_list, overlays, crtc_,
                                                  this)) {
    LOG(ERROR) << "Failed to assign overlay planes for crtc " << crtc_;
    return false;
  }

  page_flip_pending_ = true;
  pending_planes_ = overlays;

  return true;
}

void CrtcController::PageFlipFailed() {
  pending_planes_.clear();
  page_flip_pending_ = false;
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

  FOR_EACH_OBSERVER(PageFlipObserver, observers_, OnPageFlipEvent());
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

void CrtcController::AddObserver(PageFlipObserver* observer) {
  observers_.AddObserver(observer);
}

void CrtcController::RemoveObserver(PageFlipObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ui
