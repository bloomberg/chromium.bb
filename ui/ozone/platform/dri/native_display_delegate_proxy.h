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
  ~NativeDisplayDelegateProxy() override;

  // NativeDisplayDelegate overrides:
  void Initialize() override;
  void GrabServer() override;
  void UngrabServer() override;
  bool TakeDisplayControl() override;
  bool RelinquishDisplayControl() override;
  void SyncWithServer() override;
  void SetBackgroundColor(uint32_t color_argb) override;
  void ForceDPMSOn() override;
  std::vector<DisplaySnapshot*> GetDisplays() override;
  void AddMode(const DisplaySnapshot& output, const DisplayMode* mode) override;
  bool Configure(const DisplaySnapshot& output,
                 const DisplayMode* mode,
                 const gfx::Point& origin) override;
  void CreateFrameBuffer(const gfx::Size& size) override;
  bool GetHDCPState(const DisplaySnapshot& output, HDCPState* state) override;
  bool SetHDCPState(const DisplaySnapshot& output, HDCPState state) override;
  std::vector<ColorCalibrationProfile> GetAvailableColorCalibrationProfiles(
      const DisplaySnapshot& output) override;
  bool SetColorCalibrationProfile(const DisplaySnapshot& output,
                                  ColorCalibrationProfile new_profile) override;
  void AddObserver(NativeDisplayObserver* observer) override;
  void RemoveObserver(NativeDisplayObserver* observer) override;

  // DeviceEventObserver overrides:
  void OnDeviceEvent(const DeviceEvent& event) override;

  // GpuPlatformSupportHost:
  void OnChannelEstablished(int host_id, IPC::Sender* sender) override;
  void OnChannelDestroyed(int host_id) override;

  // IPC::Listener overrides:
  bool OnMessageReceived(const IPC::Message& message) override;

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
