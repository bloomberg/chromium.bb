// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_MANAGER_H_

#include <stdint.h>

#include "base/memory/scoped_ptr.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager_core.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

namespace ui {

class DeviceManager;
class DrmDisplayHost;
class DrmGpuPlatformSupportHost;
class DrmNativeDisplayDelegate;

struct DisplaySnapshot_Params;

class DrmDisplayHostManager : public GpuPlatformSupportHost {
 public:
  DrmDisplayHostManager(DrmGpuPlatformSupportHost* proxy,
                        DeviceManager* device_manager,
                        InputControllerEvdev* input_controller);
  ~DrmDisplayHostManager() override;

  DrmDisplayHost* GetDisplay(int64_t display_id);
  void AddDelegate(DrmNativeDisplayDelegate* delegate);
  void RemoveDelegate(DrmNativeDisplayDelegate* delegate);

  void TakeDisplayControl(const DisplayControlCallback& callback);
  void RelinquishDisplayControl(const DisplayControlCallback& callback);
  void UpdateDisplays(const GetDisplaysCallback& callback);

  // IPC::Listener (by way of GpuPlatformSupportHost) overrides:
  void OnChannelEstablished(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> send_runner,
      const base::Callback<void(IPC::Message*)>& send_callback) override;
  void OnChannelDestroyed(int host_id) override;
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  // Concrete implementation of sending messages to the GPU thread hosted
  // functionality.
  class HostManagerIPC : public DrmDisplayHostManagerProxy {
   public:
    HostManagerIPC(DrmGpuPlatformSupportHost* proxy,
                   DrmDisplayHostManager* parent);
    ~HostManagerIPC() override;

    void RegisterHandler() override;
    DrmGpuPlatformSupportHost* GetGpuPlatformSupportHost() override;
    bool TakeDisplayControl() override;
    bool RefreshNativeDisplays() override;
    bool RelinquishDisplayControl() override;
    bool AddGraphicsDevice(const base::FilePath& path,
                           base::FileDescriptor fd) override;
    bool RemoveGraphicsDevice(const base::FilePath& path) override;

   private:
    DrmGpuPlatformSupportHost* proxy_;
    DrmDisplayHostManager* parent_;

    DISALLOW_COPY_AND_ASSIGN(HostManagerIPC);
  };

  // IPC Entry points.
  void OnUpdateNativeDisplays(
      const std::vector<DisplaySnapshot_Params>& displays);
  void OnDisplayConfigured(int64_t display_id, bool status);
  void OnHDCPStateReceived(int64_t display_id, bool status, HDCPState state);
  void OnHDCPStateUpdated(int64_t display_id, bool status);
  void OnTakeDisplayControl(bool status);
  void OnRelinquishDisplayControl(bool status);

  // Sends messages to the GPU thread.
  scoped_ptr<HostManagerIPC> sender_;

  // Implementation without messaging functionality.
  scoped_ptr<DrmDisplayHostManagerCore> core_;

  DISALLOW_COPY_AND_ASSIGN(DrmDisplayHostManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_MANAGER_H_
