// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_CHROMEOS_NATIVE_DISPLAY_DELEGATE_PROXY_H_
#define UI_OZONE_PLATFORM_DRI_CHROMEOS_NATIVE_DISPLAY_DELEGATE_PROXY_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "ui/display/types/chromeos/native_display_delegate.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

namespace ui {

class DeviceManager;
class GpuPlatformSupportHostGbm;

struct DisplaySnapshot_Params;

class NativeDisplayDelegateProxy : public NativeDisplayDelegate,
                                   public DeviceEventObserver,
                                   public GpuPlatformSupportHost {
 public:
  NativeDisplayDelegateProxy(GpuPlatformSupportHostGbm* proxy,
                             DeviceManager* device_manager);
  virtual ~NativeDisplayDelegateProxy();

  // NativeDisplayDelegate overrides:
  virtual void Initialize() OVERRIDE;
  virtual void GrabServer() OVERRIDE;
  virtual void UngrabServer() OVERRIDE;
  virtual void SyncWithServer() OVERRIDE;
  virtual void SetBackgroundColor(uint32_t color_argb) OVERRIDE;
  virtual void ForceDPMSOn() OVERRIDE;
  virtual std::vector<DisplaySnapshot*> GetDisplays() OVERRIDE;
  virtual void AddMode(const DisplaySnapshot& output,
                       const DisplayMode* mode) OVERRIDE;
  virtual bool Configure(const DisplaySnapshot& output,
                         const DisplayMode* mode,
                         const gfx::Point& origin) OVERRIDE;
  virtual void CreateFrameBuffer(const gfx::Size& size) OVERRIDE;
  virtual bool GetHDCPState(const DisplaySnapshot& output,
                            HDCPState* state) OVERRIDE;
  virtual bool SetHDCPState(const DisplaySnapshot& output,
                            HDCPState state) OVERRIDE;
  virtual std::vector<ColorCalibrationProfile>
      GetAvailableColorCalibrationProfiles(
          const DisplaySnapshot& output) OVERRIDE;
  virtual bool SetColorCalibrationProfile(
      const DisplaySnapshot& output,
      ColorCalibrationProfile new_profile) OVERRIDE;
  virtual void AddObserver(NativeDisplayObserver* observer) OVERRIDE;
  virtual void RemoveObserver(NativeDisplayObserver* observer) OVERRIDE;

  // DeviceEventObserver overrides:
  virtual void OnDeviceEvent(const DeviceEvent& event) OVERRIDE;

  // GpuPlatformSupportHost:
  virtual void OnChannelEstablished(int host_id, IPC::Sender* sender) OVERRIDE;
  virtual void OnChannelDestroyed(int host_id) OVERRIDE;

  // IPC::Listener overrides:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  void OnUpdateNativeDisplays(
      const std::vector<DisplaySnapshot_Params>& displays);

  GpuPlatformSupportHostGbm* proxy_;  // Not owned.
  DeviceManager* device_manager_;  // Not owned.
  ScopedVector<DisplaySnapshot> displays_;
  ObserverList<NativeDisplayObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(NativeDisplayDelegateProxy);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_CHROMEOS_NATIVE_DISPLAY_DELEGATE_PROXY_H_
