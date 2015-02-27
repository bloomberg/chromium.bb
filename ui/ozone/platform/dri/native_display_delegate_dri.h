// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_NATIVE_DISPLAY_DELEGATE_DRI_H_
#define UI_OZONE_PLATFORM_DRI_NATIVE_DISPLAY_DELEGATE_DRI_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

namespace base {
class FilePath;
class SingleThreadTaskRunner;
struct FileDescriptor;
}

namespace ui {

class DeviceManager;
class DisplaySnapshotDri;
class DisplayMode;
class DisplayModeDri;
class DriWrapper;
class DrmDeviceGenerator;
class ScreenManager;

class NativeDisplayDelegateDri {
 public:
  NativeDisplayDelegateDri(ScreenManager* screen_manager,
                           const scoped_refptr<DriWrapper>& primary_device,
                           scoped_ptr<DrmDeviceGenerator> device_generator);
  ~NativeDisplayDelegateDri();

  void InitializeIOTaskRunner(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  // Returns a list of the connected displays. When this is called the list of
  // displays is refreshed.
  std::vector<DisplaySnapshot_Params> GetDisplays();

  bool ConfigureDisplay(int64_t id,
                        const DisplayMode_Params& mode,
                        const gfx::Point& origin);
  bool DisableDisplay(int64_t id);

  // Takes/releases the control of the DRM devices.
  bool TakeDisplayControl();
  bool RelinquishDisplayControl();

  // Called on DRM hotplug events to add/remove a DRM device.
  void AddGraphicsDevice(const base::FilePath& path,
                         const base::FileDescriptor& fd);
  void RemoveGraphicsDevice(const base::FilePath& path);

 private:
  DisplaySnapshotDri* FindDisplaySnapshot(int64_t id);
  const DisplayModeDri* FindDisplayMode(const gfx::Size& size,
                                        bool is_interlaced,
                                        float refresh_rate);

  void RefreshDisplayList();
  bool Configure(const DisplaySnapshotDri& output,
                 const DisplayModeDri* mode,
                 const gfx::Point& origin);

  bool GetHDCPState(const DisplaySnapshotDri& output, HDCPState* state);
  bool SetHDCPState(const DisplaySnapshotDri& output, HDCPState state);

  // Notify ScreenManager of all the displays that were present before the
  // update but are gone after the update.
  void NotifyScreenManager(
      const std::vector<DisplaySnapshotDri*>& new_displays,
      const std::vector<DisplaySnapshotDri*>& old_displays) const;

  ScreenManager* screen_manager_;  // Not owned.
  scoped_ptr<DrmDeviceGenerator> drm_device_generator_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  std::vector<scoped_refptr<DriWrapper>> devices_;
  // Modes can be shared between different displays, so we need to keep track
  // of them independently for cleanup.
  ScopedVector<const DisplayMode> cached_modes_;
  ScopedVector<DisplaySnapshotDri> cached_displays_;

  DISALLOW_COPY_AND_ASSIGN(NativeDisplayDelegateDri);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_NATIVE_DISPLAY_DELEGATE_DRI_H_
