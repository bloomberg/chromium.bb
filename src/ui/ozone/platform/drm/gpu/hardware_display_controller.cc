// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"

#include <drm.h>
#include <string.h>
#include <xf86drm.h>
#include <ios>
#include <memory>
#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "base/trace_event/trace_conversion_helper.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "third_party/libdrm/src/include/drm/drm_fourcc.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_dumb_buffer.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"

// Vendor ID for downstream, interim ChromeOS specific modifiers.
#define DRM_FORMAT_MOD_VENDOR_CHROMEOS 0xf0
// TODO(gurchetansingh) Remove once DRM_FORMAT_MOD_ARM_AFBC is used by all
// kernels and allocators.
#define DRM_FORMAT_MOD_CHROMEOS_ROCKCHIP_AFBC fourcc_mod_code(CHROMEOS, 1)

namespace ui {

namespace {

void CompletePageFlip(
    base::WeakPtr<HardwareDisplayController> hardware_display_controller_,
    int modeset_sequence,
    PresentationOnceCallback callback,
    DrmOverlayPlaneList plane_list,
    const gfx::PresentationFeedback& presentation_feedback) {
  if (hardware_display_controller_) {
    hardware_display_controller_->OnPageFlipComplete(
        modeset_sequence, std::move(plane_list), presentation_feedback);
  }
  std::move(callback).Run(presentation_feedback);
}

void DrawCursor(DrmDumbBuffer* cursor, const SkBitmap& image) {
  SkRect damage;
  image.getBounds(&damage);

  // Clear to transparent in case |image| is smaller than the canvas.
  SkCanvas* canvas = cursor->GetCanvas();
  canvas->clear(SK_ColorTRANSPARENT);
  canvas->drawImageRect(image.asImage(), damage, SkSamplingOptions());
}

template <typename T>
std::string NumberToHexString(const T value) {
  static_assert(std::is_unsigned<T>::value,
                "Can only convert unsigned ints to hex");

  std::stringstream ss;
  ss << "0x" << std::hex << std::uppercase << value;
  return ss.str();
}

}  // namespace

HardwareDisplayController::HardwareDisplayController(
    std::unique_ptr<CrtcController> controller,
    const gfx::Point& origin)
    : origin_(origin) {
  AddCrtc(std::move(controller));
  AllocateCursorBuffers();
}

HardwareDisplayController::~HardwareDisplayController() = default;

void HardwareDisplayController::GetModesetProps(
    CommitRequest* commit_request,
    const DrmOverlayPlaneList& modeset_planes,
    const drmModeModeInfo& mode) {
  GetModesetPropsForCrtcs(commit_request, modeset_planes,
                          /*use_current_crtc_mode=*/false, mode);
}

void HardwareDisplayController::GetEnableProps(
    CommitRequest* commit_request,
    const DrmOverlayPlaneList& modeset_planes) {
  // TODO(markyacoub): Simplify and remove the use of empty_mode.
  drmModeModeInfo empty_mode = {};
  GetModesetPropsForCrtcs(commit_request, modeset_planes,
                          /*use_current_crtc_mode=*/true, empty_mode);
}

void HardwareDisplayController::GetModesetPropsForCrtcs(
    CommitRequest* commit_request,
    const DrmOverlayPlaneList& modeset_planes,
    bool use_current_crtc_mode,
    const drmModeModeInfo& mode) {
  DCHECK(commit_request);

  GetDrmDevice()->plane_manager()->BeginFrame(&owned_hardware_planes_);

  for (const auto& controller : crtc_controllers_) {
    drmModeModeInfo modeset_mode =
        use_current_crtc_mode ? controller->mode() : mode;

    DrmOverlayPlaneList overlays = DrmOverlayPlane::Clone(modeset_planes);

    CrtcCommitRequest request = CrtcCommitRequest::EnableCrtcRequest(
        controller->crtc(), controller->connector(), modeset_mode, origin_,
        &owned_hardware_planes_, std::move(overlays));
    commit_request->push_back(std::move(request));
  }
}

void HardwareDisplayController::GetDisableProps(CommitRequest* commit_request) {
  for (const auto& controller : crtc_controllers_) {
    CrtcCommitRequest request = CrtcCommitRequest::DisableCrtcRequest(
        controller->crtc(), controller->connector(), &owned_hardware_planes_);
    commit_request->push_back(std::move(request));
  }
}

void HardwareDisplayController::UpdateState(
    const CrtcCommitRequest& crtc_request) {
  if (crash_gpu_timer_.IsRunning()) {
    crash_gpu_timer_.AbandonAndStop();
    SYSLOG(INFO)
        << "Detected a modeset attempt after " << failed_page_flip_counter_
        << " failed page flips. Aborting GPU process self-destruct with "
        << crash_gpu_timer_.desired_run_time() - base::TimeTicks::Now()
        << " to spare.";
    failed_page_flip_counter_ = 0;
  }

  // Verify that the current state matches the requested state.
  if (crtc_request.should_enable() && IsEnabled()) {
    DCHECK(!crtc_request.overlays().empty());
    // TODO(markyacoub): This should be absorbed in the commit request.
    ResetCursor();
    OnModesetComplete(crtc_request.overlays());
  }
}

void HardwareDisplayController::SchedulePageFlip(
    DrmOverlayPlaneList plane_list,
    SwapCompletionOnceCallback submission_callback,
    PresentationOnceCallback presentation_callback) {
  DCHECK(!page_flip_request_);
  scoped_refptr<PageFlipRequest> page_flip_request =
      base::MakeRefCounted<PageFlipRequest>(GetRefreshInterval());
  gfx::GpuFenceHandle release_fence;

  bool status =
      ScheduleOrTestPageFlip(plane_list, page_flip_request, &release_fence);
  if (!status) {
    for (const auto& plane : plane_list) {
      // If the page flip failed and we see that the buffer has been allocated
      // before the latest modeset, it could mean it was an in-flight buffer
      // carrying an obsolete configuration.
      // Request a buffer reallocation to reflect the new change.
      if (plane.buffer &&
          plane.buffer->modeset_sequence_id_at_allocation() <
              plane.buffer->drm_device()->modeset_sequence_id()) {
        std::move(submission_callback)
            .Run(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS,
                 /*release_fence=*/gfx::GpuFenceHandle());
        std::move(presentation_callback)
            .Run(gfx::PresentationFeedback::Failure());
        return;
      }
    }

    // No outdated buffers detected which makes this a true page flip failure.
    // Start the GPU self-destruct timer if needed and report the failure.
    failed_page_flip_counter_++;
    if (!crash_gpu_timer_.IsRunning()) {
      DCHECK_EQ(1, failed_page_flip_counter_);
      LOG(WARNING) << "Initiating GPU process self-destruct in "
                   << kWaitForModesetTimeout
                   << " unless a modeset attempt is detected.";

      crash_gpu_timer_.Start(
          FROM_HERE, kWaitForModesetTimeout, base::BindOnce([] {
            LOG(FATAL) << "Failed to modeset within " << kWaitForModesetTimeout
                       << " of the first page flip failure. Crashing GPU "
                          "process. Goodbye.";
          }));
    }

    std::move(submission_callback)
        .Run(gfx::SwapResult::SWAP_FAILED,
             /*release_fence=*/gfx::GpuFenceHandle());
    std::move(presentation_callback).Run(gfx::PresentationFeedback::Failure());
    return;
  }
  if (page_flip_request->page_flip_count() == 0) {
    // Apparently, there was nothing to do. This probably should not be
    // able to happen but both CrtcController::AssignOverlayPlanes and
    // HardwareDisplayPlaneManagerLegacy::Commit appear to have cases
    // where we ACK without actually scheduling a page flip.
    std::move(submission_callback)
        .Run(gfx::SwapResult::SWAP_ACK,
             /*release_fence=*/gfx::GpuFenceHandle());
    std::move(presentation_callback).Run(gfx::PresentationFeedback::Failure());
    return;
  }

  std::move(submission_callback)
      .Run(gfx::SwapResult::SWAP_ACK, std::move(release_fence));

  // Everything was submitted successfully, wait for asynchronous completion.
  page_flip_request->TakeCallback(
      base::BindOnce(&CompletePageFlip, weak_ptr_factory_.GetWeakPtr(),
                     GetDrmDevice()->modeset_sequence_id(),
                     std::move(presentation_callback), std::move(plane_list)));
  page_flip_request_ = std::move(page_flip_request);
}

bool HardwareDisplayController::TestPageFlip(
    const DrmOverlayPlaneList& plane_list) {
  return ScheduleOrTestPageFlip(plane_list, nullptr, nullptr);
}

bool HardwareDisplayController::ScheduleOrTestPageFlip(
    const DrmOverlayPlaneList& plane_list,
    scoped_refptr<PageFlipRequest> page_flip_request,
    gfx::GpuFenceHandle* release_fence) {
  TRACE_EVENT0("drm", "HDC::SchedulePageFlip");
  DCHECK(IsEnabled());

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
    status &= controller->AssignOverlayPlanes(
        &owned_hardware_planes_, pending_planes, /*is_modesetting=*/false);
  }

  status &= GetDrmDevice()->plane_manager()->Commit(
      &owned_hardware_planes_, page_flip_request, release_fence);

  return status;
}

std::vector<uint64_t> HardwareDisplayController::GetFormatModifiers(
    uint32_t fourcc_format) const {
  if (crtc_controllers_.empty())
    return std::vector<uint64_t>();

  std::vector<uint64_t> modifiers =
      crtc_controllers_[0]->GetFormatModifiers(fourcc_format);

  for (size_t i = 1; i < crtc_controllers_.size(); ++i) {
    std::vector<uint64_t> other =
        crtc_controllers_[i]->GetFormatModifiers(fourcc_format);
    std::vector<uint64_t> intersection;

    std::set_intersection(modifiers.begin(), modifiers.end(), other.begin(),
                          other.end(), std::back_inserter(intersection));
    modifiers = std::move(intersection);
  }

  return modifiers;
}

std::vector<uint64_t> HardwareDisplayController::GetSupportedModifiers(
    uint32_t fourcc_format,
    bool is_modeset) const {
  if (preferred_format_modifier_.empty())
    return std::vector<uint64_t>();

  auto it = preferred_format_modifier_.find(fourcc_format);
  if (it != preferred_format_modifier_.end()) {
    uint64_t supported_modifier = it->second;
    // AFBC for modeset buffers doesn't work correctly, as we can't fill it with
    // a valid AFBC buffer (crbug.com/852675).
    // For now, don't use AFBC for modeset buffers.
    if (is_modeset &&
        supported_modifier == DRM_FORMAT_MOD_CHROMEOS_ROCKCHIP_AFBC) {
      supported_modifier = DRM_FORMAT_MOD_LINEAR;
    }
    return std::vector<uint64_t>{supported_modifier};
  }

  return GetFormatModifiers(fourcc_format);
}

std::vector<uint64_t>
HardwareDisplayController::GetFormatModifiersForTestModeset(
    uint32_t fourcc_format) {
  // If we're about to test, clear the current preferred modifier.
  preferred_format_modifier_.clear();
  return GetFormatModifiers(fourcc_format);
}

void HardwareDisplayController::UpdatePreferredModiferForFormat(
    gfx::BufferFormat buffer_format,
    uint64_t modifier) {
  uint32_t fourcc_format = GetFourCCFormatFromBufferFormat(buffer_format);
  base::InsertOrAssign(preferred_format_modifier_, fourcc_format, modifier);

  uint32_t opaque_fourcc_format =
      GetFourCCFormatForOpaqueFramebuffer(buffer_format);
  base::InsertOrAssign(preferred_format_modifier_, opaque_fourcc_format,
                       modifier);
}

void HardwareDisplayController::MoveCursor(const gfx::Point& location) {
  cursor_location_ = location;
  UpdateCursorLocation();
}

void HardwareDisplayController::SetCursor(SkBitmap bitmap) {
  if (bitmap.drawsNothing()) {
    current_cursor_ = nullptr;
  } else {
    current_cursor_ = NextCursorBuffer();
    DrawCursor(current_cursor_, bitmap);
  }

  UpdateCursorImage();
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

bool HardwareDisplayController::IsEnabled() const {
  bool is_enabled = true;

  for (const auto& controller : crtc_controllers_)
    is_enabled &= controller->is_enabled();

  return is_enabled;
}

gfx::Size HardwareDisplayController::GetModeSize() const {
  // If there are multiple CRTCs they should all have the same size.
  return gfx::Size(crtc_controllers_[0]->mode().hdisplay,
                   crtc_controllers_[0]->mode().vdisplay);
}

base::TimeDelta HardwareDisplayController::GetRefreshInterval() const {
  // If there are multiple CRTCs they should all have the same refresh rate.
  float vrefresh = ModeRefreshRate(crtc_controllers_[0]->mode());
  return vrefresh ? base::Seconds(1) / vrefresh : base::TimeDelta();
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
    int modeset_sequence,
    DrmOverlayPlaneList pending_planes,
    const gfx::PresentationFeedback& presentation_feedback) {
  if (!page_flip_request_)
    return;  // Modeset occured during this page flip.

  time_of_last_flip_ = presentation_feedback.timestamp;
  current_planes_ = std::move(pending_planes);

  for (const auto& controller : crtc_controllers_) {
    // Only reset the modeset buffer of the crtcs for pageflips that were
    // committed after the modeset.
    if (modeset_sequence == GetDrmDevice()->modeset_sequence_id()) {
      GetDrmDevice()->plane_manager()->ResetModesetStateForCrtc(
          controller->crtc());
    }
  }
  page_flip_request_ = nullptr;
}

void HardwareDisplayController::AsValueInto(
    base::trace_event::TracedValue* value) const {
  using base::trace_event::ValueToString;

  value->SetString("origin", ValueToString(origin_));
  value->SetString("cursor_location", ValueToString(cursor_location_));
  value->SetInteger("failed_page_flip_counter", failed_page_flip_counter_);
  value->SetBoolean("is_crash_timer_running", crash_gpu_timer_.IsRunning());
  value->SetBoolean("has_page_flip_request", page_flip_request_ != nullptr);

  {
    auto scoped_dict = value->BeginDictionaryScoped("owned_hardware_planes");
    owned_hardware_planes_.AsValueInto(value);
  }
  {
    auto scoped_array = value->BeginArrayScoped("crtc_controllers");
    for (const auto& crtc : crtc_controllers_) {
      auto scoped_dict = value->AppendDictionaryScoped();
      crtc->AsValueInto(value);
    }
  }
  {
    auto scoped_array = value->BeginArrayScoped("preferred_format_modifiers");
    for (const auto& format_modifier : preferred_format_modifier_) {
      auto scoped_dict = value->AppendDictionaryScoped();
      value->SetString("format", NumberToHexString(format_modifier.first));
      value->SetString("modifier", NumberToHexString(format_modifier.second));
    }
  }
}

void HardwareDisplayController::OnModesetComplete(
    const DrmOverlayPlaneList& modeset_planes) {
  // Modesetting is blocking so it has an immediate effect. We can assume that
  // the current planes have been updated. However, if a page flip is still
  // pending, set the pending planes to the same values so that the callback
  // keeps the correct state.
  page_flip_request_ = nullptr;
  owned_hardware_planes_.legacy_page_flips.clear();
  current_planes_ = DrmOverlayPlane::Clone(modeset_planes);
  time_of_last_flip_ = base::TimeTicks::Now();
}

void HardwareDisplayController::AllocateCursorBuffers() {
  TRACE_EVENT0("drm", "HDC::AllocateCursorBuffers");
  gfx::Size max_cursor_size = GetMaximumCursorSize(GetDrmDevice()->get_fd());
  SkImageInfo info = SkImageInfo::MakeN32Premul(max_cursor_size.width(),
                                                max_cursor_size.height());
  for (size_t i = 0; i < base::size(cursor_buffers_); ++i) {
    cursor_buffers_[i] = std::make_unique<DrmDumbBuffer>(GetDrmDevice());
    // Don't register a framebuffer for cursors since they are special (they
    // aren't modesetting buffers and drivers may fail to register them due to
    // their small sizes).
    if (!cursor_buffers_[i]->Initialize(info)) {
      LOG(FATAL) << "Failed to initialize cursor buffer";
      return;
    }
  }
}

DrmDumbBuffer* HardwareDisplayController::NextCursorBuffer() {
  ++cursor_frontbuffer_;
  cursor_frontbuffer_ %= base::size(cursor_buffers_);
  return cursor_buffers_[cursor_frontbuffer_].get();
}

void HardwareDisplayController::UpdateCursorImage() {
  uint32_t handle = 0;
  gfx::Size size;

  if (current_cursor_) {
    handle = current_cursor_->GetHandle();
    size = current_cursor_->GetSize();
  }

  for (const auto& controller : crtc_controllers_)
    controller->SetCursor(handle, size);
}

void HardwareDisplayController::UpdateCursorLocation() {
  for (const auto& controller : crtc_controllers_)
    controller->MoveCursor(cursor_location_);
}

void HardwareDisplayController::ResetCursor() {
  UpdateCursorLocation();
  UpdateCursorImage();
}

}  // namespace ui
