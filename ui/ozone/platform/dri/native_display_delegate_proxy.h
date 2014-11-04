// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_NATIVE_DISPLAY_DELEGATE_PROXY_H_
#define UI_OZONE_PLATFORM_DRI_NATIVE_DISPLAY_DELEGATE_PROXY_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

namespace ui {

class DeviceManager;
class DriGpuPlatformSupportHost;

struct DisplaySnapshot_Params;

class NativeDisplayDelegateProxy : public NativeDisplayDelegate,
                                   public DeviceEventObserver,
                                   public GpuPlatformSupportHost {
 public:
  NativeDisplayDelegateProxy(DriGpuPlatformSupportHost* proxy,
                             DeviceManager* device_manager);
  virtual ~NativeDisplayDelegateProxy();

  // NativeDisplayDelegate overrides:
  virtual void Initialize() override;
  virtual void GrabServer() override;
  virtual void UngrabServer() override;
  virtual void SyncWithServer() override;
  virtual void SetBackgroundColor(uint32_t color_argb) override;
  virtual void ForceDPMSOn() override;
  virtual std::vector<DisplaySnapshot*> GetDisplays() override;
  virtual void AddMode(const DisplaySnapshot& output,
                       const DisplayMode* mode) override;
  virtual bool Configure(const DisplaySnapshot& output,
                         const DisplayMode* mode,
                         const gfx::Point& origin) override;
  virtual void CreateFrameBuffer(const gfx::Size& size) override;
  virtual bool GetHDCPState(const DisplaySnapshot& output,
                            HDCPState* state) override;
  virtual bool SetHDCPState(const DisplaySnapshot& output,
                            HDCPState state) override;
  virtual std::vector<ColorCalibrationProfile>
  GetAvailableColorCalibrationProfiles(const DisplaySnapshot& output) override;
  virtual bool SetColorCalibrationProfile(
      const DisplaySnapshot& output,
      ColorCalibrationProfile new_profile) override;
  virtual void AddObserver(NativeDisplayObserver* observer) override;
  virtual void RemoveObserver(NativeDisplayObserver* observer) override;

  // DeviceEventObserver overrides:
  virtual void OnDeviceEvent(const DeviceEvent& event) override;

  // GpuPlatformSupportHost:
  virtual void OnChannelEstablished(int host_id, IPC::Sender* sender) override;
  virtual void OnChannelDestroyed(int host_id) override;

  // IPC::Listener overrides:
  virtual bool OnMessageReceived(const IPC::Message& message) override;

 private:
  void OnUpdateNativeDisplays(
      const std::vector<DisplaySnapshot_Params>& displays);

  DriGpuPlatformSupportHost* proxy_;  // Not owned.
  DeviceManager* device_manager_;     // Not owned.
  ScopedVector<DisplaySnapshot> displays_;
  ObserverList<NativeDisplayObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(NativeDisplayDelegateProxy);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_NATIVE_DISPLAY_DELEGATE_PROXY_H_
