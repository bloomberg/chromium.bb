// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_HOST_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_HOST_H_

#include <stdint.h>

#include <vector>

#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "ui/ozone/platform/drm/common/drm_overlay_manager.h"
#include "ui/ozone/platform/drm/host/gpu_thread_adapter.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace ui {
class DrmWindowHostManager;
class OverlaySurfaceCandidate;

// This is an implementation of DrmOverlayManager where the driver is asked
// about overlay capabilities via IPC. We have no way of querying abstract
// capabilities, only if a particular configuration is supported or not.
// Each time we we are asked if a particular configuration is supported, if we
// have not seen that configuration before, it is IPCed to the GPU via
// OzoneGpuMsg_CheckOverlayCapabilities, a test commit is then performed and
// the result is returned in OzoneHostMsg_OverlayCapabilitiesReceived. Testing
// is asynchronous, until the reply arrives that configuration will be failed.
//
// All widgets share a single cache of tested configurations stored in the
// overlay manager.
class DrmOverlayManagerHost : public DrmOverlayManager {
 public:
  DrmOverlayManagerHost(GpuThreadAdapter* proxy,
                        DrmWindowHostManager* window_manager);
  ~DrmOverlayManagerHost() override;

  // DrmOverlayManager:
  void ResetCache() override;
  void CheckOverlaySupport(
      OverlayCandidatesOzone::OverlaySurfaceCandidateList* candidates,
      gfx::AcceleratedWidget widget) override;

  // Communication-free implementations of actions performed in response to
  // messages from the GPU thread.
  void GpuSentOverlayResult(gfx::AcceleratedWidget widget,
                            const OverlaySurfaceCandidateList& params,
                            const OverlayStatusList& returns);

 private:
  // Value for the request cache, that keeps track of how many times a
  // specific validation has been requested, if there is a GPU validation
  // in flight, and at last the result of the validation.
  struct OverlayValidationCacheValue {
    OverlayValidationCacheValue();
    OverlayValidationCacheValue(const OverlayValidationCacheValue&);
    ~OverlayValidationCacheValue();
    int request_num = 0;
    std::vector<OverlayStatus> status;
  };

  void SendOverlayValidationRequest(
      const OverlaySurfaceCandidateList& candidates,
      gfx::AcceleratedWidget widget) const;
  bool CanHandleCandidate(const OverlaySurfaceCandidate& candidate,
                          gfx::AcceleratedWidget widget) const;

  GpuThreadAdapter* const proxy_;               // Not owned.
  DrmWindowHostManager* const window_manager_;  // Not owned.

  // List of all OverlaySurfaceCandidate instances which have been requested
  // for validation and/or validated.
  base::MRUCache<OverlaySurfaceCandidateList, OverlayValidationCacheValue>
      cache_;

  DISALLOW_COPY_AND_ASSIGN(DrmOverlayManagerHost);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_HOST_H_
