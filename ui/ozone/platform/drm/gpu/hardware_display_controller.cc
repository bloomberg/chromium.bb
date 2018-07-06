// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"

#include <drm.h>
#include <string.h>
#include <xf86drm.h>
#include <utility>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "third_party/libdrm/src/include/drm/drm_fourcc.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"

namespace ui {

namespace {

void CompletePageFlip(
    base::WeakPtr<HardwareDisplayController> hardware_display_controller_,
    PresentationOnceCallback callback,
    DrmOverlayPlaneList plane_list,
    const gfx::PresentationFeedback& presentation_feedback) {
  if (hardware_display_controller_) {
    hardware_display_controller_->OnPageFlipComplete(std::move(plane_list),
                                                     presentation_feedback);
  }
  std::move(callback).Run(presentation_feedback);
}

}  // namespace

HardwareDisplayController::HardwareDisplayController(
    std::unique_ptr<CrtcController> controller,
    const gfx::Point& origin)
    : origin_(origin),
      is_disabled_(controller->is_disabled()),
      weak_ptr_factory_(this) {
  AddCrtc(std::move(controller));
}

HardwareDisplayController::~HardwareDisplayController() {
  // Reset the cursor.
  UnsetCursor();
}

bool HardwareDisplayController::Modeset(const DrmOverlayPlane& primary,
                                        drmModeModeInfo mode) {
  TRACE_EVENT0("drm", "HDC::Modeset");
  DCHECK(primary.buffer.get());
  bool status = true;
  for (const auto& controller : crtc_controllers_)
    status &= controller->Modeset(primary, mode);

  is_disabled_ = false;
  OnModesetComplete(primary);
  return status;
}

bool HardwareDisplayController::Enable(const DrmOverlayPlane& primary) {
  TRACE_EVENT0("drm", "HDC::Enable");
  DCHECK(primary.buffer.get());
  bool status = true;
  for (const auto& controller : crtc_controllers_)
    status &= controller->Modeset(primary, controller->mode());

  is_disabled_ = false;
  OnModesetComplete(primary);
  return status;
}

void HardwareDisplayController::Disable() {
  TRACE_EVENT0("drm", "HDC::Disable");
  for (const auto& controller : crtc_controllers_)
    controller->Disable();

  bool ret = GetDrmDevice()->plane_manager()->DisableOverlayPlanes(
      &owned_hardware_planes_);
  LOG_IF(ERROR, !ret) << "Can't disable overlays when disabling HDC.";

  is_disabled_ = true;
}

void HardwareDisplayController::SchedulePageFlip(
    DrmOverlayPlaneList plane_list,
    SwapCompletionOnceCallback submission_callback,
    PresentationOnceCallback presentation_callback) {
  DCHECK(!page_flip_request_);
  scoped_refptr<PageFlipRequest> page_flip_request =
      base::MakeRefCounted<PageFlipRequest>(GetRefreshInterval());
  std::unique_ptr<gfx::GpuFence> out_fence;

  bool status =
      ScheduleOrTestPageFlip(plane_list, page_flip_request, &out_fence);
  CHECK(status) << "SchedulePageFlip failed";

  if (page_flip_request->page_flip_count() == 0) {
    // Apparently, there was nothing to do. This probably should not be
    // able to happen but both CrtcController::AssignOverlayPlanes and
    // HardwareDisplayPlaneManagerLegacy::Commit appear to have cases
    // where we ACK without actually scheduling a page flip.
    std::move(submission_callback).Run(gfx::SwapResult::SWAP_ACK, nullptr);
    std::move(presentation_callback).Run(gfx::PresentationFeedback::Failure());
    return;
  }

  std::move(submission_callback)
      .Run(gfx::SwapResult::SWAP_ACK, std::move(out_fence));

  // Everything was submitted successfully, wait for asynchronous completion.
  page_flip_request->TakeCallback(base::BindOnce(
      &CompletePageFlip, weak_ptr_factory_.GetWeakPtr(),
      base::Passed(&presentation_callback), base::Passed(&plane_list)));
  page_flip_request_ = std::move(page_flip_request);
}

bool HardwareDisplayController::TestPageFlip(
    const DrmOverlayPlaneList& plane_list) {
  return ScheduleOrTestPageFlip(plane_list, nullptr, nullptr);
}

bool HardwareDisplayController::ScheduleOrTestPageFlip(
    const DrmOverlayPlaneList& plane_list,
    scoped_refptr<PageFlipRequest> page_flip_request,
    std::unique_ptr<gfx::GpuFence>* out_fence) {
  TRACE_EVENT0("drm", "HDC::SchedulePageFlip");
  DCHECK(!is_disabled_);

  // Ignore requests with no planes to schedule.
  if (plane_list.empty())
    return true;

  DrmOverlayPlaneList pending_planes = DrmOverlayPlane::Clone(plane_list);
  std::sort(pending_planes.begin(), pending_planes.end(),
            [](const DrmOverlayPlane& l, const DrmOverlayPlane& r) {
              return l.z_order < r.z_order;
            });
  GetDrmDevice()->plane_manager()->BeginFrame(&owned_hardware_planes_);

  bool status = true;
  for (const auto& controller : crtc_controllers_) {
    status &= controller->AssignOverlayPlanes(&owned_hardware_planes_,
                                              pending_planes);
  }

  status &= GetDrmDevice()->plane_manager()->Commit(
      &owned_hardware_planes_, page_flip_request, out_fence);

  return status;
}

std::vector<uint64_t> HardwareDisplayController::GetFormatModifiers(
    uint32_t format) {
  std::vector<uint64_t> modifiers;

  if (crtc_controllers_.empty())
    return modifiers;

  modifiers = crtc_controllers_[0]->GetFormatModifiers(format);

  for (size_t i = 1; i < crtc_controllers_.size(); ++i) {
    std::vector<uint64_t> other =
        crtc_controllers_[i]->GetFormatModifiers(format);
    std::vector<uint64_t> intersection;

    std::set_intersection(modifiers.begin(), modifiers.end(), other.begin(),
                          other.end(), std::back_inserter(intersection));
    modifiers = std::move(intersection);
  }

  return modifiers;
}

std::vector<uint64_t>
HardwareDisplayController::GetFormatModifiersForModesetting(
    uint32_t fourcc_format) {
  const auto& modifiers = GetFormatModifiers(fourcc_format);
  std::vector<uint64_t> filtered_modifiers;
  for (auto modifier : modifiers) {
    // AFBC for modeset buffers doesn't work correctly, as we can't fill it with
    // a valid AFBC buffer. For now, don't use AFBC for modeset buffers.
    // TODO: Use AFBC for modeset buffers if it is available.
    // See https://crbug.com/852675.
    if (modifier != DRM_FORMAT_MOD_CHROMEOS_ROCKCHIP_AFBC) {
      filtered_modifiers.push_back(modifier);
    }
  }
  return filtered_modifiers;
}

bool HardwareDisplayController::SetCursor(
    const scoped_refptr<ScanoutBuffer>& buffer) {
  bool status = true;

  if (is_disabled_)
    return true;

  for (const auto& controller : crtc_controllers_)
    status &= controller->SetCursor(buffer);

  return status;
}

bool HardwareDisplayController::UnsetCursor() {
  bool status = true;
  for (const auto& controller : crtc_controllers_)
    status &= controller->SetCursor(nullptr);

  return status;
}

bool HardwareDisplayController::MoveCursor(const gfx::Point& location) {
  if (is_disabled_)
    return true;

  bool status = true;
  for (const auto& controller : crtc_controllers_)
    status &= controller->MoveCursor(location);

  return status;
}

void HardwareDisplayController::AddCrtc(
    std::unique_ptr<CrtcController> controller) {
  scoped_refptr<DrmDevice> drm = controller->drm();
  DCHECK(crtc_controllers_.empty() || drm == GetDrmDevice());

  // Check if this controller owns any planes and ensure we keep track of them.
  const std::vector<std::unique_ptr<HardwareDisplayPlane>>& all_planes =
      drm->plane_manager()->planes();
  uint32_t crtc = controller->crtc();
  for (const auto& plane : all_planes) {
    if (plane->in_use() && (plane->owning_crtc() == crtc))
      owned_hardware_planes_.old_plane_list.push_back(plane.get());
  }

  crtc_controllers_.push_back(std::move(controller));
}

std::unique_ptr<CrtcController> HardwareDisplayController::RemoveCrtc(
    const scoped_refptr<DrmDevice>& drm,
    uint32_t crtc) {
  auto controller_it = std::find_if(
      crtc_controllers_.begin(), crtc_controllers_.end(),
      [drm, crtc](const std::unique_ptr<CrtcController>& crtc_controller) {
        return crtc_controller->drm() == drm && crtc_controller->crtc() == crtc;
      });
  if (controller_it == crtc_controllers_.end())
    return nullptr;

  std::unique_ptr<CrtcController> controller(std::move(*controller_it));
  crtc_controllers_.erase(controller_it);

  // Move all the planes that have been committed in the last pageflip for this
  // CRTC at the end of the collection.
  auto first_plane_to_disable_it =
      std::partition(owned_hardware_planes_.old_plane_list.begin(),
                     owned_hardware_planes_.old_plane_list.end(),
                     [crtc](const HardwareDisplayPlane* plane) {
                       return plane->owning_crtc() != crtc;
                     });

  // Disable the planes enabled with the last commit on |crtc|, otherwise
  // the planes will be visible if the crtc is reassigned to another connector.
  HardwareDisplayPlaneList hardware_plane_list;
  std::copy(first_plane_to_disable_it,
            owned_hardware_planes_.old_plane_list.end(),
            std::back_inserter(hardware_plane_list.old_plane_list));
  drm->plane_manager()->DisableOverlayPlanes(&hardware_plane_list);

  // Remove the planes assigned to |crtc|.
  owned_hardware_planes_.old_plane_list.erase(
      first_plane_to_disable_it, owned_hardware_planes_.old_plane_list.end());

  return controller;
}

bool HardwareDisplayController::HasCrtc(const scoped_refptr<DrmDevice>& drm,
                                        uint32_t crtc) const {
  for (const auto& controller : crtc_controllers_) {
    if (controller->drm() == drm && controller->crtc() == crtc)
      return true;
  }

  return false;
}

bool HardwareDisplayController::IsMirrored() const {
  return crtc_controllers_.size() > 1;
}

bool HardwareDisplayController::IsDisabled() const {
  return is_disabled_;
}

gfx::Size HardwareDisplayController::GetModeSize() const {
  // If there are multiple CRTCs they should all have the same size.
  return gfx::Size(crtc_controllers_[0]->mode().hdisplay,
                   crtc_controllers_[0]->mode().vdisplay);
}

uint32_t HardwareDisplayController::GetRefreshRate() const {
  // If there are multiple CRTCs they should all have the same size.
  return crtc_controllers_[0]->mode().vrefresh;
}

base::TimeDelta HardwareDisplayController::GetRefreshInterval() const {
  uint32_t vrefresh = GetRefreshRate();
  return vrefresh ? base::TimeDelta::FromSeconds(1) / vrefresh
                  : base::TimeDelta();
}

base::TimeTicks HardwareDisplayController::GetTimeOfLastFlip() const {
  return time_of_last_flip_;
}

scoped_refptr<DrmDevice> HardwareDisplayController::GetDrmDevice() const {
  DCHECK(!crtc_controllers_.empty());
  // TODO(dnicoara) When we support mirroring across DRM devices, figure out
  // which device should be used for allocations.
  return crtc_controllers_[0]->drm();
}

void HardwareDisplayController::OnPageFlipComplete(
    DrmOverlayPlaneList pending_planes,
    const gfx::PresentationFeedback& presentation_feedback) {
  if (!page_flip_request_)
    return;  // Modeset occured during this page flip.
  time_of_last_flip_ = presentation_feedback.timestamp;
  current_planes_ = std::move(pending_planes);
  page_flip_request_ = nullptr;
}

void HardwareDisplayController::OnModesetComplete(
    const DrmOverlayPlane& primary) {
  // drmModeSetCrtc has an immediate effect, so we can assume that the current
  // planes have been updated. However if a page flip is still pending, set the
  // pending planes to the same values so that the callback keeps the correct
  // state.
  page_flip_request_ = nullptr;
  current_planes_.clear();
  current_planes_.push_back(primary.Clone());
  time_of_last_flip_ = base::TimeTicks::Now();
}

}  // namespace ui
