// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_overlay_candidates_host.h"

#include <algorithm>

#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/drm/host/drm_gpu_platform_support_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host.h"

namespace ui {

namespace {
const size_t kMaxCacheSize = 100;
}  // namespace

bool DrmOverlayCandidatesHost::OverlayCompare::operator()(
    const OverlayCheck_Params& l,
    const OverlayCheck_Params& r) const {
  if (l.plane_z_order < r.plane_z_order)
    return true;
  if (l.plane_z_order > r.plane_z_order)
    return false;
  if (l.display_rect < r.display_rect)
    return true;
  if (l.display_rect != r.display_rect)
    return false;
  if (l.format < r.format)
    return true;
  if (l.format > r.format)
    return false;
  if (l.transform < r.transform)
    return true;
  if (l.transform > r.transform)
    return false;
  if (l.buffer_size.width() < r.buffer_size.width())
    return true;
  if (l.buffer_size.width() > r.buffer_size.width())
    return false;
  return l.buffer_size.height() < r.buffer_size.height();
}

DrmOverlayCandidatesHost::HardwareDisplayPlaneProxy::HardwareDisplayPlaneProxy(
    uint32_t id)
    : plane_id(id) {}

DrmOverlayCandidatesHost::HardwareDisplayPlaneProxy::
    ~HardwareDisplayPlaneProxy() {}

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
  uint32_t compatible_candidates = 0;
  uint32_t planes_in_use = 0;
  bool force_validation = false;
  std::vector<OverlayCheck_Params> new_candidates;
  for (auto& candidate : *candidates) {
    if (!CanHandleCandidate(candidate))
      continue;

    OverlayCheck_Params lookup(candidate);
    if (!force_validation) {
      CompatibleParams::const_iterator last_iter =
          in_use_compatible_params_.find(lookup);
      if (last_iter != in_use_compatible_params_.end()) {
        candidate.overlay_handled = last_iter->second;
        compatible_candidates++;
        if (candidate.overlay_handled)
          planes_in_use++;

        continue;
      }
    }

    auto iter = cache_.Get(lookup);
    if (iter == cache_.end()) {
      lookup.weight = CalculateCandidateWeight(candidate);
      cache_.Put(lookup, false);
      // It is possible that the cc rect we get actually falls off the edge of
      // the screen. Usually this is prevented via things like status bars
      // blocking overlaying or cc clipping it, but in case it wasn't properly
      // clipped (since GL will render this situation fine) just ignore it here.
      // This should be an extremely rare occurrance.
      if (lookup.plane_z_order != 0 &&
          !window_->GetBounds().Contains(lookup.display_rect)) {
        continue;
      }

      new_candidates.push_back(lookup);
    } else if (iter->second) {
      force_validation = true;
    }
  }

  // We have new candidates whose configuration needs to be validated in GPU
  // side.
  if (!new_candidates.empty())
    SendRequest(new_candidates);

  if (compatible_candidates > planes_in_use &&
      planes_in_use < hardware_plane_proxy_.size()) {
    force_validation = true;
  }

  if (!force_validation) {
    DCHECK(planes_in_use <= hardware_plane_proxy_.size())
        << "Total layers promoted to use Overlay:" << planes_in_use
        << "While the maximum layers which can be actually supported are:"
        << hardware_plane_proxy_.size();
    return;
  }

  // A new layer has been added or removed from the last validation. We expect
  // this to be very rare situation, hence not fully optimized. We always
  // validate the new combination of layers.
  ValidateCandidates(candidates);
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
  in_use_compatible_params_.clear();
  hardware_plane_proxy_.clear();
}

void DrmOverlayCandidatesHost::SendRequest(
    const std::vector<OverlayCheck_Params>& list) {
  if (!platform_support_->IsConnected())
    return;

  platform_support_->Send(new OzoneGpuMsg_CheckOverlayCapabilities(
      window_->GetAcceleratedWidget(), list));
}

void DrmOverlayCandidatesHost::OnOverlayResult(
    bool* handled,
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& params) {
  if (widget != window_->GetAcceleratedWidget())
    return;

  *handled = true;
  for (const auto& check : params) {
    // We expect params to contain only supported configurations.
    cache_.Put(check, true);
    for (const auto& plane_id : check.plane_ids) {
      bool plane_found = false;
      for (const auto* plane : hardware_plane_proxy_) {
        if (plane->plane_id == plane_id) {
          plane_found = true;
          break;
        }
      }

      if (!plane_found) {
        hardware_plane_proxy_.push_back(
            make_scoped_ptr(new HardwareDisplayPlaneProxy(plane_id)));
      }
    }
  }
}

bool DrmOverlayCandidatesHost::CanHandleCandidate(
    const OverlaySurfaceCandidate& candidate) const {
  // 0.01 constant chosen to match DCHECKs in gfx::ToNearestRect and avoid
  // that code asserting on quads that we accept.
  if (!gfx::IsNearestRectWithinDistance(candidate.display_rect, 0.01f))
    return false;

  if (candidate.transform == gfx::OVERLAY_TRANSFORM_INVALID)
    return false;

  if (candidate.plane_z_order != 0 && candidate.buffer_size.IsEmpty())
    return false;

  if (candidate.is_clipped &&
      !candidate.clip_rect.Contains(candidate.quad_rect_in_target_space))
    return false;

  return true;
}

uint32_t DrmOverlayCandidatesHost::CalculateCandidateWeight(
    const OverlaySurfaceCandidate& candidate) const {
  uint32_t weight = 0;
  if (candidate.plane_z_order == 0) {
    // Make sure primary plane is always first in the list.
    weight = 100;
  } else {
    // Make sure we first try to find hardware planes supporting special
    // requirements.
    if (candidate.transform != gfx::OVERLAY_TRANSFORM_NONE)
      weight++;

    if (candidate.format != gfx::BufferFormat::BGRA_8888 &&
        candidate.format != gfx::BufferFormat::BGRX_8888) {
      weight++;
    }

    // TODO(kalyank): We want to consider size based on power benefits.
  }

  return weight;
}

void DrmOverlayCandidatesHost::ValidateCandidates(
    OverlaySurfaceCandidateList* candidates) {
  // Make sure params being currently used are in cache. They might have been
  // removed in case we haven't tried to get them from cache for a while.
  for (const auto& param : in_use_compatible_params_)
    cache_.Put(param.first, true);

  in_use_compatible_params_.clear();
  typedef std::pair<OverlaySurfaceCandidate*, OverlayCheck_Params>
      CandidatePair;
  std::vector<CandidatePair> compatible_candidates;
  for (auto& candidate : *candidates) {
    candidate.overlay_handled = false;

    if (!CanHandleCandidate(candidate))
      continue;

    OverlayCheck_Params lookup(candidate);
    auto iter = cache_.Peek(lookup);
    DCHECK(iter != cache_.end());
    if (!iter->second)
      continue;

    in_use_compatible_params_[iter->first] = false;
    compatible_candidates.push_back(std::make_pair(&candidate, iter->first));
  }

  uint32_t available_overlays = hardware_plane_proxy_.size();
  for (auto* plane : hardware_plane_proxy_)
    plane->in_use = false;

  // Sort in decending order w.r.t weight.
  std::sort(compatible_candidates.begin(), compatible_candidates.end(),
            [](const CandidatePair& l, const CandidatePair& r) {
              return l.second.weight > r.second.weight;
            });

  // Make sure we don't handle more candidates than what we can support in
  // GPU side.
  for (const auto& candidate : compatible_candidates) {
    for (auto* plane : hardware_plane_proxy_) {
      // Plane is already in use.
      if (plane->in_use)
        continue;

      for (const auto& plane_id : candidate.second.plane_ids) {
        if (plane->plane_id == plane_id) {
          available_overlays--;
          auto iter = in_use_compatible_params_.find(candidate.second);
          DCHECK(iter != in_use_compatible_params_.end());
          iter->second = true;
          candidate.first->overlay_handled = true;
          plane->in_use = true;
          break;
        }
      }

      // We have succefully found a plane.
      if (plane->in_use)
        break;
    }

    // We dont have any free hardware resources.
    if (!available_overlays)
      break;
  }
}

}  // namespace ui
