// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_CORE_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_CORE_H_

#include <stdint.h>

#include <vector>

#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"

namespace ui {
class DrmWindowHostManager;

// Pluggable sender proxy abstract base class.
class DrmOverlayManagerProxy {
 public:
  virtual ~DrmOverlayManagerProxy();
  virtual bool IsConnected() = 0;
  virtual bool CheckOverlayCapabilities(
      gfx::AcceleratedWidget widget,
      const std::vector<OverlayCheck_Params>& new_params) = 0;
  virtual void UnregisterHandler() = 0;
  virtual void RegisterHandler() = 0;
};

// This is the core functionality of DrmOverlayManager where the
// communication with the GPU thread is via a pluggable proxy.
class DrmOverlayManagerCore : public OverlayManagerOzone {
 public:
  DrmOverlayManagerCore(DrmOverlayManagerProxy* proxy,
                        DrmWindowHostManager* window_manager);
  ~DrmOverlayManagerCore() override;

  // OverlayManagerOzone:
  scoped_ptr<OverlayCandidatesOzone> CreateOverlayCandidates(
      gfx::AcceleratedWidget w) override;

  void ResetCache();
  void GpuSentOverlayResult(gfx::AcceleratedWidget widget,
                            const std::vector<OverlayCheck_Params>& params);

  void CheckOverlaySupport(
      OverlayCandidatesOzone::OverlaySurfaceCandidateList* candidates,
      gfx::AcceleratedWidget widget);

 private:
  void SendOverlayValidationRequest(
      const std::vector<OverlayCheck_Params>& new_params,
      gfx::AcceleratedWidget widget) const;
  bool CanHandleCandidate(
      const OverlayCandidatesOzone::OverlaySurfaceCandidate& candidate,
      gfx::AcceleratedWidget widget) const;

  bool is_supported_;
  DrmOverlayManagerProxy* proxy_;         // Not owned.
  DrmWindowHostManager* window_manager_;  // Not owned.

  // List of all OverlayCheck_Params which have been validated in GPU side.
  // Value is set to true if we are waiting for validation results from GPU.
  base::MRUCacheBase<std::vector<OverlayCheck_Params>,
                     bool,
                     base::MRUCacheNullDeletor<bool>>
      cache_;

  DISALLOW_COPY_AND_ASSIGN(DrmOverlayManagerCore);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_CORE_H_
