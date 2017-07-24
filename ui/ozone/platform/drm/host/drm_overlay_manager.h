// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_H_

#include <stdint.h>

#include <vector>

#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "ui/ozone/platform/drm/host/gpu_thread_adapter.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"

namespace ui {
class DrmWindowHostManager;
class OverlaySurfaceCandidate;

class DrmOverlayManager : public OverlayManagerOzone {
 public:
  DrmOverlayManager(GpuThreadAdapter* proxy,
                    DrmWindowHostManager* window_manager);
  ~DrmOverlayManager() override;

  // OverlayManagerOzone:
  std::unique_ptr<OverlayCandidatesOzone> CreateOverlayCandidates(
      gfx::AcceleratedWidget w) override;

  // Invoked on changes to the window (aka display) that require re-populating
  // the cache from the DRM thread.
  void ResetCache();

  // Communication-free implementations of actions performed in response to
  // messages from the GPU thread.
  void GpuSentOverlayResult(const gfx::AcceleratedWidget& widget,
                            const OverlaySurfaceCandidateList& params,
                            const OverlayStatusList& returns);

  // Service method for DrmOverlayCandidatesHost
  void CheckOverlaySupport(
      OverlayCandidatesOzone::OverlaySurfaceCandidateList* candidates,
      gfx::AcceleratedWidget widget);

 private:
  void SendOverlayValidationRequest(
      const OverlaySurfaceCandidateList& candidates,
      gfx::AcceleratedWidget widget) const;
  bool CanHandleCandidate(const OverlaySurfaceCandidate& candidate,
                          gfx::AcceleratedWidget widget) const;

  GpuThreadAdapter* proxy_;               // Not owned.
  DrmWindowHostManager* window_manager_;  // Not owned.

  // List of all OverlaySurfaceCandidate instances which have been validated in
  // GPU side.  Value is set to true if we are waiting for validation results
  // from GPU.
  base::MRUCache<std::vector<OverlaySurfaceCandidate>,
                 std::vector<OverlayStatus>>
      cache_;

  DISALLOW_COPY_AND_ASSIGN(DrmOverlayManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_H_
