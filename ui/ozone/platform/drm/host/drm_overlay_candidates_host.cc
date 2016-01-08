// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_overlay_candidates_host.h"

#include <stddef.h>

#include <algorithm>

#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/drm/host/drm_gpu_platform_support_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host.h"

namespace ui {

namespace {
const size_t kMaxCacheSize = 200;
}  // namespace

DrmOverlayCandidatesHost::DrmOverlayCandidatesHost(
    DrmGpuPlatformSupportHost* platform_support,
    DrmWindowHost* window)
    : platform_support_(platform_support),
      window_(window),
      cache_(kMaxCacheSize) {
  platform_support_->RegisterHandler(this);
  window_->SetOverlayCandidatesHost(this);
}

DrmOverlayCandidatesHost::~DrmOverlayCandidatesHost() {
  platform_support_->UnregisterHandler(this);
  window_->SetOverlayCandidatesHost(nullptr);
}

void DrmOverlayCandidatesHost::CheckOverlaySupport(
    OverlaySurfaceCandidateList* candidates) {
  std::vector<OverlayCheck_Params> overlay_params;
  for (auto& candidate : *candidates) {
    // Compositor doesn't have information about the total size of primary
    // candidate. We get this information from display rect.
    if (candidate.plane_z_order == 0)
      candidate.buffer_size = gfx::ToNearestRect(candidate.display_rect).size();

    overlay_params.push_back(OverlayCheck_Params(candidate));
  }

  const auto& iter = cache_.Get(overlay_params);
  // We are still waiting on results for this candidate list from GPU.
  if (iter != cache_.end() && iter->second)
    return;

  size_t size = candidates->size();

  if (iter == cache_.end()) {
    // We can skip GPU side validation in case all candidates are invalid
    // or we are checking only for Primary.
    bool needs_gpu_validation = false;
    for (size_t i = 0; i < size; i++) {
      const OverlaySurfaceCandidate& candidate = candidates->at(i);

      if (candidate.plane_z_order == 0) {
        // We expect primary to be always valid.
        overlay_params.at(i).is_overlay_candidate = true;
        continue;
      }

      if (!CanHandleCandidate(candidate)) {
        overlay_params.at(i).is_overlay_candidate = false;
        continue;
      }

      needs_gpu_validation = true;
    }

    cache_.Put(overlay_params, needs_gpu_validation);

    if (needs_gpu_validation)
      SendOverlayValidationRequest(overlay_params);
  } else {
    const std::vector<OverlayCheck_Params>& validated_params = iter->first;
    DCHECK(size == validated_params.size());

    for (size_t i = 0; i < size; i++) {
      candidates->at(i).overlay_handled =
          validated_params.at(i).is_overlay_candidate;
    }
  }
}

void DrmOverlayCandidatesHost::OnChannelEstablished(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    const base::Callback<void(IPC::Message*)>& sender) {
  // Reset any old cache.
  ResetCache();
}

void DrmOverlayCandidatesHost::OnChannelDestroyed(int host_id) {
}

bool DrmOverlayCandidatesHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = false;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(DrmOverlayCandidatesHost, message, &handled)
  IPC_MESSAGE_FORWARD(OzoneHostMsg_OverlayCapabilitiesReceived, this,
                      DrmOverlayCandidatesHost::OnOverlayResult)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DrmOverlayCandidatesHost::ResetCache() {
  cache_.Clear();
}

void DrmOverlayCandidatesHost::SendOverlayValidationRequest(
    const std::vector<OverlayCheck_Params>& new_params) const {
  if (!platform_support_->IsConnected())
    return;

  platform_support_->Send(new OzoneGpuMsg_CheckOverlayCapabilities(
      window_->GetAcceleratedWidget(), new_params));
}

void DrmOverlayCandidatesHost::OnOverlayResult(
    bool* handled,
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& params) {
  if (widget != window_->GetAcceleratedWidget())
    return;

  *handled = true;
  cache_.Put(params, false);
}

bool DrmOverlayCandidatesHost::CanHandleCandidate(
    const OverlaySurfaceCandidate& candidate) const {
  if (candidate.buffer_size.IsEmpty())
    return false;

  // 0.01 constant chosen to match DCHECKs in gfx::ToNearestRect and avoid
  // that code asserting on quads that we accept.
  if (!gfx::IsNearestRectWithinDistance(candidate.display_rect, 0.01f))
    return false;

  if (candidate.transform == gfx::OVERLAY_TRANSFORM_INVALID)
    return false;

  if (candidate.plane_z_order != 0) {
    // It is possible that the cc rect we get actually falls off the edge of
    // the screen. Usually this is prevented via things like status bars
    // blocking overlaying or cc clipping it, but in case it wasn't properly
    // clipped (since GL will render this situation fine) just ignore it
    // here. This should be an extremely rare occurrance.
    if (!window_->GetBounds().Contains(
            gfx::ToNearestRect(candidate.display_rect))) {
      return false;
    }
  }

  if (candidate.is_clipped &&
      !candidate.clip_rect.Contains(candidate.quad_rect_in_target_space))
    return false;

  return true;
}

}  // namespace ui
