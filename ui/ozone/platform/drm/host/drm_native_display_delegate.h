// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_NATIVE_DISPLAY_DELEGATE_HOST_H_
#define UI_OZONE_PLATFORM_DRM_HOST_NATIVE_DISPLAY_DELEGATE_HOST_H_

#include <map>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

namespace ui {

class DeviceManager;
class DisplayManager;
class DrmGpuPlatformSupportHost;

struct DisplaySnapshot_Params;

class DrmNativeDisplayDelegate : public NativeDisplayDelegate,
                                 public DeviceEventObserver,
                                 public GpuPlatformSupportHost {
 public:
  DrmNativeDisplayDelegate(DrmGpuPlatformSupportHost* proxy,
                           DeviceManager* device_manager,
                           DisplayManager* display_manager,
                           const base::FilePath& primary_graphics_card_path);
  ~DrmNativeDisplayDelegate() override;

  // NativeDisplayDelegate overrides:
  void Initialize() override;
  void GrabServer() override;
  void UngrabServer() override;
  bool TakeDisplayControl() override;
  bool RelinquishDisplayControl() override;
  void SyncWithServer() override;
  void SetBackgroundColor(uint32_t color_argb) override;
  void ForceDPMSOn() override;
  void GetDisplays(const GetDisplaysCallback& callback) override;
  void AddMode(const DisplaySnapshot& output, const DisplayMode* mode) override;
  void Configure(const DisplaySnapshot& output,
                 const DisplayMode* mode,
                 const gfx::Point& origin,
                 const ConfigureCallback& callback) override;
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
  void OnChannelEstablished(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> send_runner,
      const base::Callback<void(IPC::Message*)>& send_callback) override;
  void OnChannelDestroyed(int host_id) override;

  // IPC::Listener overrides:
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  void OnUpdateNativeDisplays(
      const std::vector<DisplaySnapshot_Params>& displays);
  void OnDisplayConfigured(int64_t display_id, bool status);

  void OnNewGraphicsDevice(const base::FilePath& path, base::File file);

  DrmGpuPlatformSupportHost* proxy_;  // Not owned.
  DeviceManager* device_manager_;     // Not owned.
  DisplayManager* display_manager_;   // Not owned.

  // File path for the primary graphics card which is opened by default in the
  // GPU process. We'll avoid opening this in hotplug events since it will race
  // with the GPU process trying to open it and aquire DRM master.
  const base::FilePath primary_graphics_card_path_;

  // Keeps track if there is a dummy display. This happens on initialization
  // when there is no connection to the GPU to update the displays.
  bool has_dummy_display_;

  ScopedVector<DisplaySnapshot> displays_;
  ObserverList<NativeDisplayObserver> observers_;

  GetDisplaysCallback get_displays_callback_;

  // Map between display_id and the configuration callback.
  std::map<int64_t, ConfigureCallback> configure_callback_map_;

  base::WeakPtrFactory<DrmNativeDisplayDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DrmNativeDisplayDelegate);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_NATIVE_DISPLAY_DELEGATE_HOST_H_
