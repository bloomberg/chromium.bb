// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_GPU_PLATFORM_SUPPORT_H_
#define UI_OZONE_PLATFORM_DRI_DRI_GPU_PLATFORM_SUPPORT_H_

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/gpu_platform_support.h"

class SkBitmap;

namespace gfx {
class Point;
class Rect;
}

namespace ui {

class DriSurfaceFactory;
class DriWindowDelegate;
class DriWindowDelegateManager;
class NativeDisplayDelegateDri;
class ScreenManager;

struct DisplayMode_Params;
struct DisplaySnapshot_Params;

class DriGpuPlatformSupport : public GpuPlatformSupport {
 public:
  DriGpuPlatformSupport(DriSurfaceFactory* dri,
                        DriWindowDelegateManager* window_manager,
                        ScreenManager* screen_manager,
                        scoped_ptr<NativeDisplayDelegateDri> ndd);
  ~DriGpuPlatformSupport() override;

  void AddHandler(scoped_ptr<GpuPlatformSupport> handler);

  // GpuPlatformSupport:
  void OnChannelEstablished(IPC::Sender* sender) override;

  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  void OnCreateWindowDelegate(gfx::AcceleratedWidget widget);
  void OnDestroyWindowDelegate(gfx::AcceleratedWidget widget);
  void OnWindowBoundsChanged(gfx::AcceleratedWidget widget,
                             const gfx::Rect& bounds);
  void OnCursorSet(gfx::AcceleratedWidget widget,
                   const std::vector<SkBitmap>& bitmaps,
                   const gfx::Point& location,
                   int frame_delay_ms);
  void OnCursorMove(gfx::AcceleratedWidget widget, const gfx::Point& location);

  // Display related IPC handlers.
  void OnForceDPMSOn();
  void OnRefreshNativeDisplays(
      const std::vector<DisplaySnapshot_Params>& cached_displays);
  void OnConfigureNativeDisplay(int64_t id,
                                const DisplayMode_Params& mode,
                                const gfx::Point& origin);
  void OnDisableNativeDisplay(int64_t id);
  void OnTakeDisplayControl();
  void OnRelinquishDisplayControl();

  IPC::Sender* sender_;                       // Not owned.
  DriSurfaceFactory* dri_;                    // Not owned.
  DriWindowDelegateManager* window_manager_;  // Not owned.
  ScreenManager* screen_manager_;             // Not owned.

  scoped_ptr<NativeDisplayDelegateDri> ndd_;
  ScopedVector<GpuPlatformSupport> handlers_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_GPU_PLATFORM_SUPPORT_H_
