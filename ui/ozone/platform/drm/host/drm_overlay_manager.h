// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_H_

#include "base/macros.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/platform/drm/host/drm_overlay_manager_core.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/overlay_manager_ozone.h"

namespace ui {

class DrmWindowHostManager;
class DrmGpuPlatformSupportHost;

class DrmOverlayManager : public OverlayManagerOzone,
                          public GpuPlatformSupportHost {
 public:
  DrmOverlayManager(DrmGpuPlatformSupportHost* platform_support_host,
                    DrmWindowHostManager* window_manager);
  ~DrmOverlayManager() override;

  // OverlayManagerOzone:
  scoped_ptr<OverlayCandidatesOzone> CreateOverlayCandidates(
      gfx::AcceleratedWidget w) override;

  // GpuPlatformSupportHost:
  void OnChannelEstablished(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> send_runner,
      const base::Callback<void(IPC::Message*)>& sender) override;
  void OnChannelDestroyed(int host_id) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void ResetCache();

 private:
  // IPC handler.
  class OverlayCandidatesIPC : public DrmOverlayManagerProxy {
   public:
    OverlayCandidatesIPC(DrmGpuPlatformSupportHost* platform_support,
                         DrmOverlayManager* parent);
    ~OverlayCandidatesIPC() override;
    void RegisterHandler() override;
    bool IsConnected() override;
    void UnregisterHandler() override;
    bool CheckOverlayCapabilities(
        gfx::AcceleratedWidget widget,
        const std::vector<OverlayCheck_Params>& new_params) override;

   private:
    DrmGpuPlatformSupportHost* platform_support_;
    DrmOverlayManager* parent_;
    DISALLOW_COPY_AND_ASSIGN(OverlayCandidatesIPC);
  };

  // Entry point for incoming IPC.
  void OnOverlayResult(gfx::AcceleratedWidget widget,
                       const std::vector<OverlayCheck_Params>& params);

  scoped_ptr<OverlayCandidatesIPC> sender_;
  scoped_ptr<DrmOverlayManagerCore> core_;

  DISALLOW_COPY_AND_ASSIGN(DrmOverlayManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_OVERLAY_MANAGER_H_
