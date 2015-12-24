// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_MANAGER_H_

#include <stdint.h>

#include <queue>
#include <set>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

namespace ui {

class DeviceManager;
class DrmDeviceHandle;
class DrmDisplayHost;
class DrmGpuPlatformSupportHost;
class DrmNativeDisplayDelegate;

struct DisplaySnapshot_Params;

class DrmDisplayHostManager : public DeviceEventObserver,
                              public GpuPlatformSupportHost {
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
  struct DisplayEvent {
    DisplayEvent(DeviceEvent::ActionType action_type,
                 const base::FilePath& path)
        : action_type(action_type), path(path) {}

    DeviceEvent::ActionType action_type;
    base::FilePath path;
  };

  void OnUpdateNativeDisplays(
      const std::vector<DisplaySnapshot_Params>& displays);
  void OnDisplayConfigured(int64_t display_id, bool status);

  void ProcessEvent();

  // Called as a result of finishing to process the display hotplug event. These
  // are responsible for dequing the event and scheduling the next event.
  void OnAddGraphicsDevice(const base::FilePath& path,
                           scoped_ptr<DrmDeviceHandle> handle);
  void OnUpdateGraphicsDevice();
  void OnRemoveGraphicsDevice(const base::FilePath& path);

  void OnHDCPStateReceived(int64_t display_id, bool status, HDCPState state);
  void OnHDCPStateUpdated(int64_t display_id, bool status);

  void OnTakeDisplayControl(bool status);
  void OnRelinquishDisplayControl(bool status);

  void RunUpdateDisplaysCallback(const GetDisplaysCallback& callback) const;

  void NotifyDisplayDelegate() const;

  DrmGpuPlatformSupportHost* proxy_;  // Not owned.
  DeviceManager* device_manager_;     // Not owned.
  InputControllerEvdev* input_controller_;  // Not owned.

  DrmNativeDisplayDelegate* delegate_ = nullptr;  // Not owned.

  // File path for the primary graphics card which is opened by default in the
  // GPU process. We'll avoid opening this in hotplug events since it will race
  // with the GPU process trying to open it and aquire DRM master.
  base::FilePath primary_graphics_card_path_;

  // File path for virtual gem (VGEM) device.
  base::FilePath vgem_card_path_;

  // Keeps track if there is a dummy display. This happens on initialization
  // when there is no connection to the GPU to update the displays.
  bool has_dummy_display_ = false;

  std::vector<scoped_ptr<DrmDisplayHost>> displays_;

  GetDisplaysCallback get_displays_callback_;

  bool display_externally_controlled_ = false;
  bool display_control_change_pending_ = false;
  DisplayControlCallback take_display_control_callback_;
  DisplayControlCallback relinquish_display_control_callback_;

  // Used to serialize display event processing. This is done since
  // opening/closing DRM devices cannot be done on the UI thread and are handled
  // on a worker thread. Thus, we need to queue events in order to process them
  // in the correct order.
  std::queue<DisplayEvent> event_queue_;

  // True if a display event is currently being processed on a worker thread.
  bool task_pending_ = false;

  // Keeps track of all the active DRM devices.
  std::set<base::FilePath> drm_devices_;

  // This is used to cache the primary DRM device until the channel is
  // established.
  scoped_ptr<DrmDeviceHandle> primary_drm_device_handle_;

  base::WeakPtrFactory<DrmDisplayHostManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DrmDisplayHostManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_MANAGER_H_
