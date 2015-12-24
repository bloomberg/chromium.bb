// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_H_

#include "base/macros.h"
#include "ui/ozone/public/overlay_manager_ozone.h"

namespace ui {

class DrmGpuPlatformSupportHost;
class DrmWindowHostManager;

class DrmOverlayManager : public OverlayManagerOzone {
 public:
  DrmOverlayManager(DrmGpuPlatformSupportHost* platform_support_host,
                    DrmWindowHostManager* manager);
  ~DrmOverlayManager() override;

  // OverlayManagerOzone:
  scoped_ptr<OverlayCandidatesOzone> CreateOverlayCandidates(
      gfx::AcceleratedWidget w) override;

 private:
  DrmGpuPlatformSupportHost* platform_support_host_;
  DrmWindowHostManager* window_manager_;
  bool is_supported_;

  DISALLOW_COPY_AND_ASSIGN(DrmOverlayManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_H_
