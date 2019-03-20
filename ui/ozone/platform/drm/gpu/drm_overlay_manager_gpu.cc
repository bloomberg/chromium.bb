// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_overlay_manager_gpu.h"

#include <utility>

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

DrmOverlayManagerGpu::DrmOverlayManagerGpu(DrmThreadProxy* drm_thread_proxy)
    : drm_thread_proxy_(drm_thread_proxy), weak_ptr_factory_(this) {}

DrmOverlayManagerGpu::~DrmOverlayManagerGpu() = default;

void DrmOverlayManagerGpu::SendOverlayValidationRequest(
    const std::vector<OverlaySurfaceCandidate>& candidates,
    gfx::AcceleratedWidget widget) {
  TRACE_EVENT_ASYNC_BEGIN0(
      "hwoverlays", "DrmOverlayManagerGpu::SendOverlayValidationRequest", this);

  drm_thread_proxy_->CheckOverlayCapabilities(
      widget, candidates,
      base::BindOnce(&DrmOverlayManagerGpu::ReceiveOverlayValidationResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DrmOverlayManagerGpu::ReceiveOverlayValidationResponse(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlaySurfaceCandidate>& candidates,
    const std::vector<OverlayStatus>& status) {
  TRACE_EVENT_ASYNC_END0(
      "hwoverlays", "DrmOverlayManagerGpu::SendOverlayValidationRequest", this);

  UpdateCacheForOverlayCandidates(candidates, status);
}

}  // namespace ui
