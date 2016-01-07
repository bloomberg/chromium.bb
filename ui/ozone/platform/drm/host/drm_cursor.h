// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_CURSOR_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_CURSOR_H_

#include <memory>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/host/drm_cursor_core.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

namespace gfx {
class PointF;
class Vector2dF;
class Rect;
}

namespace ui {
class BitmapCursorFactoryOzone;
class DrmWindowHostManager;

class DrmCursor : public CursorDelegateEvdev {
 public:
  explicit DrmCursor(DrmWindowHostManager* window_manager);
  ~DrmCursor() override;

  // Change the cursor over the specifed window.
  void SetCursor(gfx::AcceleratedWidget window, PlatformCursor platform_cursor);

  // Handle window lifecycle.
  void OnWindowAdded(gfx::AcceleratedWidget window,
                     const gfx::Rect& bounds_in_screen,
                     const gfx::Rect& cursor_confined_bounds);
  void OnWindowRemoved(gfx::AcceleratedWidget window);

  // Handle window bounds changes.
  void CommitBoundsChange(gfx::AcceleratedWidget window,
                          const gfx::Rect& new_display_bounds_in_screen,
                          const gfx::Rect& new_confined_bounds);

  // CursorDelegateEvdev:
  void MoveCursorTo(gfx::AcceleratedWidget window,
                    const gfx::PointF& location) override;
  void MoveCursorTo(const gfx::PointF& screen_location) override;
  void MoveCursor(const gfx::Vector2dF& delta) override;
  bool IsCursorVisible() override;
  gfx::PointF GetLocation() override;
  gfx::Rect GetCursorConfinedBounds() override;

  // IPC.
  void OnChannelEstablished(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> send_runner,
      const base::Callback<void(IPC::Message*)>& sender);
  void OnChannelDestroyed(int host_id);
  bool OnMessageReceived(const IPC::Message& message);

 private:
  // An IPC-based implementation of the DrmCursorProxy that ships
  // messages to the GPU process.
  class CursorIPC : public DrmCursorProxy {
   public:
    explicit CursorIPC(base::Lock* lock);
    ~CursorIPC();

    bool IsConnectedLocked();
    void SendLocked(IPC::Message* message);

    // DrmCursorProxy implementation.
    void CursorSet(gfx::AcceleratedWidget window,
                   const std::vector<SkBitmap>& bitmaps,
                   gfx::Point point,
                   int frame_delay_ms) override;
    void Move(gfx::AcceleratedWidget window, gfx::Point point) override;

    int host_id;

    // Callback for IPC updates.
    base::Callback<void(IPC::Message*)> send_callback;
    scoped_refptr<base::SingleThreadTaskRunner> send_runner;
    base::Lock* lock;
  };

  // The mutex synchronizing this object.
  base::Lock lock_;

  // Enforce our threading constraints.
  base::ThreadChecker thread_checker_;

  CursorIPC ipc_;
  std::unique_ptr<DrmCursorCore> core_;

  DISALLOW_COPY_AND_ASSIGN(DrmCursor);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_CURSOR_H_
