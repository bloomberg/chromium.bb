// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"

#include <drm.h>
#include <errno.h>
#include <string.h>
#include <xf86drm.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/public/native_pixmap.h"

namespace ui {

HardwareDisplayController::PageFlipRequest::PageFlipRequest(
    const OverlayPlaneList& planes,
    bool is_sync,
    const base::Closure& callback)
    : planes(planes), is_sync(is_sync), callback(callback) {
}

HardwareDisplayController::PageFlipRequest::~PageFlipRequest() {
}

HardwareDisplayController::HardwareDisplayController(
    scoped_ptr<CrtcController> controller)
    : is_disabled_(true) {
  memset(&mode_, 0, sizeof(mode_));
  AddCrtc(controller.Pass());
}

HardwareDisplayController::~HardwareDisplayController() {
  // Reset the cursor.
  UnsetCursor();
  ClearPendingRequests();
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

  current_planes_ = std::vector<OverlayPlane>(1, primary);
  pending_planes_.clear();
  ClearPendingRequests();

  // Because a page flip is pending we need to leave some state for the
  // callback. We use the modeset state since it is the only valid state.
  if (HasPendingPageFlips())
    requests_.push_back(
        PageFlipRequest(current_planes_, false, base::Bind(&base::DoNothing)));

  return status;
}

bool HardwareDisplayController::Enable() {
  TRACE_EVENT0("drm", "HDC::Enable");
  DCHECK(!current_planes_.empty());
  const OverlayPlane* primary = OverlayPlane::GetPrimaryPlane(current_planes_);

  return Modeset(*primary, mode_);
}

void HardwareDisplayController::Disable() {
  TRACE_EVENT0("drm", "HDC::Disable");
  for (size_t i = 0; i < crtc_controllers_.size(); ++i)
    crtc_controllers_[i]->Disable();

  is_disabled_ = true;
}

void HardwareDisplayController::QueueOverlayPlane(const OverlayPlane& plane) {
  pending_planes_.push_back(plane);
}

bool HardwareDisplayController::SchedulePageFlip(
    bool is_sync,
    const base::Closure& callback) {
  TRACE_EVENT0("drm", "HDC::SchedulePageFlip");

  // Ignore requests with no planes to schedule.
  if (pending_planes_.empty()) {
    callback.Run();
    return true;
  }

  requests_.push_back(PageFlipRequest(pending_planes_, is_sync, callback));
  pending_planes_.clear();

  // A request is being serviced right now.
  if (HasPendingPageFlips())
    return true;

  bool status = ActualSchedulePageFlip();

  // No page flip event on failure so discard failed request.
  if (!status)
    requests_.pop_front();

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
  owned_hardware_planes_.add(
      controller->drm().get(),
      scoped_ptr<HardwareDisplayPlaneList>(new HardwareDisplayPlaneList()));
  controller->AddObserver(this);
  crtc_controllers_.push_back(controller.release());
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

      controller->RemoveObserver(this);
      // If a display configuration happens mid page flip we want to make sure
      // the HDC won't wait for an event from a CRTC that is no longer
      // associated with it.
      if (controller->page_flip_pending())
        OnPageFlipEvent();

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

void HardwareDisplayController::OnPageFlipEvent() {
  TRACE_EVENT0("drm", "HDC::OnPageFlipEvent");
  // OnPageFlipEvent() needs to handle 2 cases:
  //  1) Normal page flips in which case:
  //    a) HasPendingPageFlips() may return false if we're in mirror mode and
  //       one of the CRTCs hasn't finished page flipping. In this case we want
  //       to wait for all the CRTCs.
  //    b) HasPendingPageFlips() returns true in which case all CRTCs are ready
  //       for the next request. In this case we expect that |requests_| isn't
  //       empty.
  //  2) A CRTC was added while it was page flipping. In this case a modeset
  //     must be performed. Modesetting clears all pending requests, however the
  //     CRTCs will honor the scheduled page flip. Thus we need to handle page
  //     flip events with no requests.

  if (HasPendingPageFlips())
    return;

  if (!requests_.empty())
    ProcessPageFlipRequest();

  // ProcessPageFlipRequest() consumes a request.
  if (requests_.empty())
    return;

  // At this point we still have requests pending, so schedule the next request.
  bool status = ActualSchedulePageFlip();
  if (!status) {
    PageFlipRequest request = requests_.front();
    requests_.pop_front();

    // Normally the caller would handle the error call, but because we're in a
    // delayed schedule the initial SchedulePageFlip() already returned true,
    // thus we need to run the callback.
    request.callback.Run();
  }
}

scoped_refptr<DrmDevice> HardwareDisplayController::GetAllocationDrmDevice()
    const {
  DCHECK(!crtc_controllers_.empty());
  // TODO(dnicoara) When we support mirroring across DRM devices, figure out
  // which device should be used for allocations.
  return crtc_controllers_[0]->drm();
}

bool HardwareDisplayController::HasPendingPageFlips() const {
  for (size_t i = 0; i < crtc_controllers_.size(); ++i)
    if (crtc_controllers_[i]->page_flip_pending())
      return true;

  return false;
}

bool HardwareDisplayController::ActualSchedulePageFlip() {
  TRACE_EVENT0("drm", "HDC::ActualSchedulePageFlip");
  DCHECK(!requests_.empty());

  if (is_disabled_) {
    ProcessPageFlipRequest();
    return true;
  }

  OverlayPlaneList pending_planes = requests_.front().planes;
  std::sort(pending_planes.begin(), pending_planes.end(),
            [](const OverlayPlane& l, const OverlayPlane& r) {
              return l.z_order < r.z_order;
            });

  bool status = true;
  for (size_t i = 0; i < crtc_controllers_.size(); ++i) {
    status &= crtc_controllers_[i]->SchedulePageFlip(
        owned_hardware_planes_.get(crtc_controllers_[i]->drm().get()),
        pending_planes);
  }

  bool is_sync = requests_.front().is_sync;
  for (const auto& planes : owned_hardware_planes_) {
    if (!planes.first->plane_manager()->Commit(planes.second, is_sync)) {
      status = false;
    }
  }

  return status;
}

void HardwareDisplayController::ProcessPageFlipRequest() {
  DCHECK(!requests_.empty());
  PageFlipRequest request = requests_.front();
  requests_.pop_front();

  current_planes_.swap(request.planes);
  request.callback.Run();
}

void HardwareDisplayController::ClearPendingRequests() {
  while (!requests_.empty()) {
    PageFlipRequest request = requests_.front();
    requests_.pop_front();
    request.callback.Run();
  }
}

}  // namespace ui
