// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_OVERLAY_CANDIDATES_H_
#define UI_OZONE_PLATFORM_DRM_HOST_OVERLAY_CANDIDATES_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace ui {

class DrmOverlayManagerCore;

// This is an implementation of OverlayCandidatesOzone where the driver is asked
// about overlay capabilities via IPC. We have no way of querying abstract
// capabilities, only if a particular configuration is supported or not.
// Each time we we are asked if a particular configuration is supported, if we
// have not seen that configuration before, it is IPCed to the GPU via
// OzoneGpuMsg_CheckOverlayCapabilities; a test commit is then performed and
// the result is returned in OzoneHostMsg_OverlayCapabilitiesReceived. Testing
// is asynchronous, until the reply arrives that configuration will be failed.
//
// All OverlayCandidatesOzone objects share a single cache of tested
// configurations stored in the overlay manager.
class DrmOverlayCandidatesHost : public OverlayCandidatesOzone {
 public:
  DrmOverlayCandidatesHost(DrmOverlayManagerCore* manager_core,
                           gfx::AcceleratedWidget widget);
  ~DrmOverlayCandidatesHost() override;

  void CheckOverlaySupport(OverlaySurfaceCandidateList* candidates) override;

 private:
  DrmOverlayManagerCore* overlay_manager_;  // Not owned.
  gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(DrmOverlayCandidatesHost);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_OVERLAY_CANDIDATES_H_
