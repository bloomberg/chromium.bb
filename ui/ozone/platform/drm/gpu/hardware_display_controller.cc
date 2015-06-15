// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"

#include <drm.h>
#include <string.h>
#include <xf86drm.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"
#include "ui/ozone/public/native_pixmap.h"

namespace ui {

HardwareDisplayController::HardwareDisplayController(
    scoped_ptr<CrtcController> controller,
    const gfx::Point& origin)
    : origin_(origin),
      mode_(controller->mode()),
      is_disabled_(controller->is_disabled()) {
  AddCrtc(controller.Pass());
}

HardwareDisplayController::~HardwareDisplayController() {
  // Reset the cursor.
  UnsetCursor();
}

bool HardwareDisplayController::Modeset(const OverlayPlane& primary,
                                        drmModeModeInfo mode) {
  TRACE_EVENT0("drm", "HDC::Modeset");
  DCHECK(primary.buffer.get());
  bool status = true;
  for (size_t i = 0; i < crtc_controllers_.size(); ++i)
    status &= crtc_controllers_[i]->Modeset(primary, mode);

  is_disabled_ = false;
  mode_ = mode;

  return status;
}

void HardwareDisplayController::Disable() {
  TRACE_EVENT0("drm", "HDC::Disable");
  for (size_t i = 0; i < crtc_controllers_.size(); ++i)
    crtc_controllers_[i]->Disable();


  is_disabled_ = true;
}

bool HardwareDisplayController::SchedulePageFlip(
    const OverlayPlaneList& plane_list,
    bool is_sync,
    bool test_only,
    const PageFlipCallback& callback) {
  TRACE_EVENT0("drm", "HDC::SchedulePageFlip");

  DCHECK(!is_disabled_);

  // Ignore requests with no planes to schedule.
  if (plane_list.empty()) {
    callback.Run(gfx::SwapResult::SWAP_ACK);
    return true;
  }

  scoped_refptr<PageFlipRequest> page_flip_request =
      new PageFlipRequest(crtc_controllers_.size(), callback);

  OverlayPlaneList pending_planes = plane_list;
  std::sort(pending_planes.begin(), pending_planes.end(),
            [](const OverlayPlane& l, const OverlayPlane& r) {
              return l.z_order < r.z_order;
            });
  if (pending_planes.front().z_order != 0)
    return false;

  for (const auto& planes : owned_hardware_planes_)
    planes.first->plane_manager()->BeginFrame(planes.second);

  bool status = true;
  for (size_t i = 0; i < crtc_controllers_.size(); ++i) {
    status &= crtc_controllers_[i]->SchedulePageFlip(
        owned_hardware_planes_.get(crtc_controllers_[i]->drm().get()),
        pending_planes, test_only, page_flip_request);
  }

  for (const auto& planes : owned_hardware_planes_) {
    if (!planes.first->plane_manager()->Commit(planes.second, is_sync,
                                               test_only)) {
      status = false;
    }
  }

  return status;
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
    status &= crtc_controllers_[i]->SetCursor(nullptr);

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
  owned_hardware_planes_.add(
      controller->drm().get(),
      scoped_ptr<HardwareDisplayPlaneList>(new HardwareDisplayPlaneList()));
  crtc_controllers_.push_back(controller.Pass());
}

scoped_ptr<CrtcController> HardwareDisplayController::RemoveCrtc(
    const scoped_refptr<DrmDevice>& drm,
    uint32_t crtc) {
  for (ScopedVector<CrtcController>::iterator it = crtc_controllers_.begin();
       it != crtc_controllers_.end(); ++it) {
    if ((*it)->drm() == drm && (*it)->crtc() == crtc) {
      scoped_ptr<CrtcController> controller(*it);
      crtc_controllers_.weak_erase(it);
      // Remove entry from |owned_hardware_planes_| iff no other crtcs share it.
      bool found = false;
      for (ScopedVector<CrtcController>::iterator it =
               crtc_controllers_.begin();
           it != crtc_controllers_.end(); ++it) {
        if ((*it)->drm() == controller->drm()) {
          found = true;
          break;
        }
      }
      if (!found)
        owned_hardware_planes_.erase(controller->drm().get());

      return controller.Pass();
    }
  }

  return nullptr;
}

bool HardwareDisplayController::HasCrtc(const scoped_refptr<DrmDevice>& drm,
                                        uint32_t crtc) const {
  for (size_t i = 0; i < crtc_controllers_.size(); ++i)
    if (crtc_controllers_[i]->drm() == drm &&
        crtc_controllers_[i]->crtc() == crtc)
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

scoped_refptr<DrmDevice> HardwareDisplayController::GetAllocationDrmDevice()
    const {
  DCHECK(!crtc_controllers_.empty());
  // TODO(dnicoara) When we support mirroring across DRM devices, figure out
  // which device should be used for allocations.
  return crtc_controllers_[0]->drm();
}

}  // namespace ui
