// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_MANAGER_CORE_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_MANAGER_CORE_H_

#include <stdint.h>

#include <map>
#include <queue>

#include "base/file_descriptor_posix.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
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
class DrmDisplayHostManagerCore;
class DrmGpuPlatformSupportHost;
class DrmNativeDisplayDelegate;

struct DisplaySnapshot_Params;

// The concrete implementation of DrmDisplayHostManagerCoreProxy contains all
// the
// necessary code for the DrmDisplayHostManagerCoreCore to communicate with
// the GPU child thread whether by IPC or thead-hop.
class DrmDisplayHostManagerProxy {
 public:
  virtual ~DrmDisplayHostManagerProxy();
  virtual void RegisterHandler() = 0;
  virtual DrmGpuPlatformSupportHost* GetGpuPlatformSupportHost() = 0;
  virtual bool TakeDisplayControl() = 0;
  virtual bool RefreshNativeDisplays() = 0;
  virtual bool RelinquishDisplayControl() = 0;
  virtual bool AddGraphicsDevice(const base::FilePath& path,
                                 base::FileDescriptor fd) = 0;
  virtual bool RemoveGraphicsDevice(const base::FilePath& path) = 0;
};

// The portion of the DrmDisplayHostManagerCore implementation that is agnostic
// in how its communication with GPU-specific functionality is implemented.
// This is used from both the IPC and the in-process versions in  MUS.
class DrmDisplayHostManagerCore : public DeviceEventObserver {
 public:
  DrmDisplayHostManagerCore(DrmDisplayHostManagerProxy* proxy,
                            DeviceManager* device_manager,
                            InputControllerEvdev* input_controller);
  ~DrmDisplayHostManagerCore() override;

  DrmDisplayHost* GetDisplay(int64_t display_id);

  // External API.
  void AddDelegate(DrmNativeDisplayDelegate* delegate);
  void RemoveDelegate(DrmNativeDisplayDelegate* delegate);
  void TakeDisplayControl(const DisplayControlCallback& callback);
  void RelinquishDisplayControl(const DisplayControlCallback& callback);
  void UpdateDisplays(const GetDisplaysCallback& callback);

  // DeviceEventObserver overrides:
  void OnDeviceEvent(const DeviceEvent& event) override;

  // Communication-free implementations of actions performed in response to
  // messages from the GPU thread.
  void GpuThreadStarted();
  void GpuHasUpdatedNativeDisplays(
      const std::vector<DisplaySnapshot_Params>& displays);
  void GpuConfiguredDisplay(int64_t display_id, bool status);
  void GpuReceivedHDCPState(int64_t display_id, bool status, HDCPState state);
  void GpuUpdatedHDCPState(int64_t display_id, bool status);
  void GpuTookDisplayControl(bool status);
  void GpuRelinquishedDisplayControl(bool status);

 private:
  struct DisplayEvent {
    DisplayEvent(DeviceEvent::ActionType action_type,
                 const base::FilePath& path)
        : action_type(action_type), path(path) {}

    DeviceEvent::ActionType action_type;
    base::FilePath path;
  };

  // Handle hotplug events sequentially.
  void ProcessEvent();

  // Called as a result of finishing to process the display hotplug event. These
  // are responsible for dequing the event and scheduling the next event.
  void OnAddGraphicsDevice(const base::FilePath& path,
                           const base::FilePath& sysfs_path,
                           scoped_ptr<DrmDeviceHandle> handle);
  void OnUpdateGraphicsDevice();
  void OnRemoveGraphicsDevice(const base::FilePath& path);

  void RunUpdateDisplaysCallback(const GetDisplaysCallback& callback) const;

  void NotifyDisplayDelegate() const;

  DrmDisplayHostManagerProxy* proxy_;       // Not owned.
  DeviceManager* device_manager_;           // Not owned.
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

  // Keeps track of all the active DRM devices. The key is the device path, the
  // value is the sysfs path which has been resolved from the device path.
  std::map<base::FilePath, base::FilePath> drm_devices_;

  // This is used to cache the primary DRM device until the channel is
  // established.
  scoped_ptr<DrmDeviceHandle> primary_drm_device_handle_;

  base::WeakPtrFactory<DrmDisplayHostManagerCore> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DrmDisplayHostManagerCore);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_MANAGER_CORE_H_
