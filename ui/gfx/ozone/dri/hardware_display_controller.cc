// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ozone/dri/hardware_display_controller.h"

#include <errno.h>
#include <string.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/ozone/dri/dri_buffer.h"
#include "ui/gfx/ozone/dri/dri_surface.h"
#include "ui/gfx/ozone/dri/dri_wrapper.h"

namespace gfx {

HardwareDisplayController::HardwareDisplayController()
    : drm_(NULL),
      connector_id_(0),
      crtc_id_(0),
      mode_(),
      saved_crtc_(NULL),
      state_(UNASSOCIATED),
      surface_(),
      time_of_last_flip_(0) {}

void HardwareDisplayController::SetControllerInfo(
    DriWrapper* drm,
    uint32_t connector_id,
    uint32_t crtc_id,
    uint32_t dpms_property_id,
    drmModeModeInfo mode) {
  drm_ = drm;
  connector_id_ = connector_id;
  crtc_id_ = crtc_id;
  dpms_property_id_ = dpms_property_id;
  mode_ = mode;
  saved_crtc_ = drm_->GetCrtc(crtc_id_);
  state_ = UNINITIALIZED;
}

HardwareDisplayController::~HardwareDisplayController() {
  if (state_ == UNASSOCIATED)
    return;

  // Reset the cursor.
  UnsetCursor();

  if (saved_crtc_) {
    if (!drm_->SetCrtc(saved_crtc_, &connector_id_))
      DLOG(ERROR) << "Failed to restore CRTC state: " << strerror(errno);
    drm_->FreeCrtc(saved_crtc_);
  }

  if (surface_.get()) {
    // Unregister the buffers.
    for (size_t i = 0; i < arraysize(surface_->bitmaps_); ++i) {
      if (!drm_->RemoveFramebuffer(surface_->bitmaps_[i]->framebuffer()))
        DLOG(ERROR) << "Failed to remove FB: " << strerror(errno);
    }
  }
}

bool
HardwareDisplayController::BindSurfaceToController(
    scoped_ptr<DriSurface> surface) {
  CHECK(state_ == UNINITIALIZED);

  // Register the buffers.
  for (size_t i = 0; i < arraysize(surface->bitmaps_); ++i) {
    uint32_t fb_id;
    if (!drm_->AddFramebuffer(
            mode_,
            surface->bitmaps_[i]->GetColorDepth(),
            surface->bitmaps_[i]->canvas()->imageInfo().bytesPerPixel() << 3,
            surface->bitmaps_[i]->stride(),
            surface->bitmaps_[i]->handle(),
            &fb_id)) {
      DLOG(ERROR) << "Failed to register framebuffer: " << strerror(errno);
      state_ = FAILED;
      return false;
    }
    surface->bitmaps_[i]->set_framebuffer(fb_id);
  }

  surface_.reset(surface.release());
  state_ = SURFACE_INITIALIZED;
  return true;
}

bool HardwareDisplayController::SchedulePageFlip() {
  CHECK(state_ == SURFACE_INITIALIZED || state_ == INITIALIZED);

  if (state_ == SURFACE_INITIALIZED) {
    // Perform the initial modeset.
    if (!drm_->SetCrtc(crtc_id_,
                       surface_->GetFramebufferId(),
                       &connector_id_,
                       &mode_)) {
      DLOG(ERROR) << "Cannot set CRTC: " << strerror(errno);
      state_ = FAILED;
      return false;
    } else {
      state_ = INITIALIZED;
    }

    if (dpms_property_id_)
      drm_->ConnectorSetProperty(connector_id_,
                                 dpms_property_id_,
                                 DRM_MODE_DPMS_ON);
  }

  if (!drm_->PageFlip(crtc_id_,
                      surface_->GetFramebufferId(),
                      this)) {
    state_ = FAILED;
    LOG(ERROR) << "Cannot page flip: " << strerror(errno);
    return false;
  }

  return true;
}

void HardwareDisplayController::OnPageFlipEvent(unsigned int frame,
                                                unsigned int seconds,
                                                unsigned int useconds) {
  time_of_last_flip_ =
      static_cast<uint64_t>(seconds) * base::Time::kMicrosecondsPerSecond +
      useconds;

  surface_->SwapBuffers();
}

bool HardwareDisplayController::SetCursor(DriSurface* surface) {
  CHECK(state_ != UNASSOCIATED);
  bool ret = drm_->SetCursor(crtc_id_,
                         surface->GetHandle(),
                         surface->size().width(),
                         surface->size().height());
  surface->SwapBuffers();
  return ret;
}

bool HardwareDisplayController::UnsetCursor() {
  CHECK(state_ != UNASSOCIATED);
  return drm_->SetCursor(crtc_id_, 0, 0, 0);
}

bool HardwareDisplayController::MoveCursor(const gfx::Point& location) {
  CHECK(state_ != UNASSOCIATED);
  return drm_->MoveCursor(crtc_id_, location.x(), location.y());
}

}  // namespace gfx
