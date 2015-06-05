// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_H_

#include "ui/ozone/public/overlay_manager_ozone.h"

namespace ui {

class DrmGpuPlatformSupportHost;

class DrmOverlayManager : public OverlayManagerOzone {
 public:
  DrmOverlayManager(bool allow_surfaceless,
                    DrmGpuPlatformSupportHost* platform_support_host);
  ~DrmOverlayManager() override;

  // OverlayManagerOzone:
  scoped_ptr<OverlayCandidatesOzone> CreateOverlayCandidates(
      gfx::AcceleratedWidget w) override;
  bool CanShowPrimaryPlaneAsOverlay() override;

 private:
  DrmGpuPlatformSupportHost* platform_support_host_;
  bool allow_surfaceless_;
  bool is_supported_;

  DISALLOW_COPY_AND_ASSIGN(DrmOverlayManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_H_
