// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_MANAGER_GPU_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_MANAGER_GPU_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/drm/common/drm_overlay_manager.h"

namespace ui {
class DrmThreadProxy;

// DrmOverlayManager implementation that runs in the GPU process. PostTasks
// overlay validations requests to the DRM thread.
class DrmOverlayManagerGpu : public DrmOverlayManager {
 public:
  explicit DrmOverlayManagerGpu(DrmThreadProxy* drm_thread_proxy);
  ~DrmOverlayManagerGpu() override;

  // TODO(crbug.com/930173): Plumb some sort of display configuration
  // information into this class. DrmOverlayManagerHost checks if the overlay
  // candidates are contained by the display rect. The overlay configuration
  // cache also needs to be reset when display configuration changes.

 private:
  // DrmOverlayManager:
  void SendOverlayValidationRequest(
      const std::vector<OverlaySurfaceCandidate>& candidates,
      gfx::AcceleratedWidget widget) override;

  void ReceiveOverlayValidationResponse(
      gfx::AcceleratedWidget widget,
      const std::vector<OverlaySurfaceCandidate>& candidates,
      const std::vector<OverlayStatus>& status);

  DrmThreadProxy* const drm_thread_proxy_;

  base::WeakPtrFactory<DrmOverlayManagerGpu> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DrmOverlayManagerGpu);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_OVERLAY_MANAGER_GPU_H_
