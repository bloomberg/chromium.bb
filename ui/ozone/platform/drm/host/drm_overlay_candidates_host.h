// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_OVERLAY_CANDIDATES_H_
#define UI_OZONE_PLATFORM_DRM_HOST_OVERLAY_CANDIDATES_H_

#include <deque>
#include <map>
#include <vector>

#include "base/containers/mru_cache.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace ui {

class DrmGpuPlatformSupportHost;

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
  struct OverlayCompare {
    bool operator()(const OverlayCheck_Params& l, const OverlayCheck_Params& r);
  };

 public:
  DrmOverlayCandidatesHost(gfx::AcceleratedWidget widget,
                           DrmGpuPlatformSupportHost* platform_support);
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

 private:
  void SendRequest(const OverlaySurfaceCandidateList& candidates,
                   const OverlayCheck_Params& check);
  void OnOverlayResult(bool* handled,
                       gfx::AcceleratedWidget widget,
                       bool result);
  void ProcessResult(const OverlayCheck_Params& check, bool result);

  void CheckSingleOverlay(OverlaySurfaceCandidateList* candidates);

  gfx::AcceleratedWidget widget_;
  DrmGpuPlatformSupportHost* platform_support_;  // Not owned.

  std::deque<OverlayCheck_Params> pending_checks_;

  template <class KeyType, class ValueType>
  struct OverlayMap {
    typedef std::map<KeyType, ValueType, OverlayCompare> Type;
  };
  base::MRUCacheBase<OverlayCheck_Params,
                     bool,
                     base::MRUCacheNullDeletor<bool>,
                     OverlayMap> cache_;

  DISALLOW_COPY_AND_ASSIGN(DrmOverlayCandidatesHost);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_OVERLAY_CANDIDATES_H_
