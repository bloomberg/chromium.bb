// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_OVERLAY_CANDIDATES_H_
#define UI_OZONE_PLATFORM_DRM_HOST_OVERLAY_CANDIDATES_H_

#include <stdint.h>

#include <deque>
#include <map>
#include <vector>

#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace ui {

class DrmGpuPlatformSupportHost;
class DrmWindowHost;

// This is an implementation of OverlayCandidatesOzone where the driver is asked
// about overlay capabilities via IPC. We have no way of querying abstract
// capabilities, only if a particular configuration is supported or not.
// Each time we we are asked if a particular configuration is supported, if we
// have not seen that configuration before, it is IPCed to the GPU via
// OzoneGpuMsg_CheckOverlayCapabilities; a test commit is then performed and
// the result is returned in OzoneHostMsg_OverlayCapabilitiesReceived. Testing
// is asynchronous, until the reply arrives that configuration will be failed.
//
// There is a many:1 relationship between this class and
// DrmGpuPlatformSupportHost, each compositor will own one of these objects.
// Each request has a unique request ID, which is assigned from a shared
// sequence number so that replies can be routed to the correct object.
class DrmOverlayCandidatesHost : public OverlayCandidatesOzone,
                                 public GpuPlatformSupportHost {
 public:
  DrmOverlayCandidatesHost(DrmGpuPlatformSupportHost* platform_support,
                           DrmWindowHost* window);
  ~DrmOverlayCandidatesHost() override;

  // OverlayCandidatesOzone:
  void CheckOverlaySupport(OverlaySurfaceCandidateList* candidates) override;

  // GpuPlatformSupportHost:
  void OnChannelEstablished(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> send_runner,
      const base::Callback<void(IPC::Message*)>& sender) override;
  void OnChannelDestroyed(int host_id) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void ResetCache();

 private:
  void SendOverlayValidationRequest(
      const std::vector<OverlayCheck_Params>& list) const;
  void OnOverlayResult(bool* handled,
                       gfx::AcceleratedWidget widget,
                       const std::vector<OverlayCheck_Params>& params);
  bool CanHandleCandidate(const OverlaySurfaceCandidate& candidate) const;

  DrmGpuPlatformSupportHost* platform_support_;  // Not owned.
  DrmWindowHost* window_;                        // Not owned.

  // List of all OverlayCheck_Params which have been validated in GPU side.
  // Value is set to true if we are waiting for validation results from GPU.
  base::MRUCacheBase<std::vector<OverlayCheck_Params>,
                     bool,
                     base::MRUCacheNullDeletor<bool>> cache_;

  DISALLOW_COPY_AND_ASSIGN(DrmOverlayCandidatesHost);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_OVERLAY_CANDIDATES_H_
