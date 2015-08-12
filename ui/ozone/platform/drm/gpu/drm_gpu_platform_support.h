// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_PLATFORM_SUPPORT_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_PLATFORM_SUPPORT_H_

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/message_filter.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/gpu_platform_support.h"

class SkBitmap;

namespace base {
class FilePath;
class SingleThreadTaskRunner;
struct FileDescriptor;
}

namespace gfx {
class Point;
class Rect;
}

namespace ui {

class DrmDeviceManager;
class DrmGpuDisplayManager;
class DrmSurfaceFactory;
class DrmWindow;
class ScreenManager;
class ScanoutBufferGenerator;

struct DisplayMode_Params;
struct DisplaySnapshot_Params;
struct OverlayCheck_Params;
struct GammaRampRGBEntry;

class DrmGpuPlatformSupport : public GpuPlatformSupport {
 public:
  DrmGpuPlatformSupport(DrmDeviceManager* drm_device_manager,
                        ScreenManager* screen_manager,
                        ScanoutBufferGenerator* buffer_generator,
                        scoped_ptr<DrmGpuDisplayManager> display_manager);
  ~DrmGpuPlatformSupport() override;

  // GpuPlatformSupport:
  void OnChannelEstablished(IPC::Sender* sender) override;
  IPC::MessageFilter* GetMessageFilter() override;

  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  void OnCreateWindow(gfx::AcceleratedWidget widget);
  void OnDestroyWindow(gfx::AcceleratedWidget widget);
  void OnWindowBoundsChanged(gfx::AcceleratedWidget widget,
                             const gfx::Rect& bounds);
  void OnCursorSet(gfx::AcceleratedWidget widget,
                   const std::vector<SkBitmap>& bitmaps,
                   const gfx::Point& location,
                   int frame_delay_ms);
  void OnCursorMove(gfx::AcceleratedWidget widget, const gfx::Point& location);
  void OnCheckOverlayCapabilities(
      gfx::AcceleratedWidget widget,
      const std::vector<OverlayCheck_Params>& overlays);

  // Display related IPC handlers.
  void OnRefreshNativeDisplays();
  void OnConfigureNativeDisplay(int64_t id,
                                const DisplayMode_Params& mode,
                                const gfx::Point& origin);
  void OnDisableNativeDisplay(int64_t id);
  void OnTakeDisplayControl();
  void OnRelinquishDisplayControl();
  void OnAddGraphicsDevice(const base::FilePath& path,
                           const base::FileDescriptor& fd);
  void OnRemoveGraphicsDevice(const base::FilePath& path);
  void OnGetHDCPState(int64_t display_id);
  void OnSetHDCPState(int64_t display_id, HDCPState state);
  void OnSetGammaRamp(int64_t id, const std::vector<GammaRampRGBEntry>& lut);

  void SetIOTaskRunner(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);

  IPC::Sender* sender_ = nullptr;             // Not owned.
  DrmDeviceManager* drm_device_manager_;  // Not owned.
  ScreenManager* screen_manager_;         // Not owned.
  ScanoutBufferGenerator* buffer_generator_;  // Not owned.

  scoped_ptr<DrmGpuDisplayManager> display_manager_;
  scoped_refptr<IPC::MessageFilter> filter_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_PLATFORM_SUPPORT_H_
