// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_CURSOR_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_CURSOR_H_

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

namespace gfx {
class PointF;
class Vector2dF;
class Rect;
}

namespace ui {

class BitmapCursorOzone;
class BitmapCursorFactoryOzone;
class DrmGpuPlatformSupportHost;
class DrmWindowHostManager;

class DrmCursor : public CursorDelegateEvdev, public GpuPlatformSupportHost {
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
  void PrepareForBoundsChange(gfx::AcceleratedWidget window);
  void CommitBoundsChange(gfx::AcceleratedWidget window,
                          const gfx::Rect& new_display_bounds_in_screen,
                          const gfx::Rect& new_confined_bounds);

  // Confines the cursor to |confined_bounds| for |window|.
  void ConfineCursorToBounds(gfx::AcceleratedWidget window,
                             const gfx::Rect& bounds);

  // CursorDelegateEvdev:
  void MoveCursorTo(gfx::AcceleratedWidget window,
                    const gfx::PointF& location) override;
  void MoveCursorTo(const gfx::PointF& screen_location) override;
  void MoveCursor(const gfx::Vector2dF& delta) override;
  bool IsCursorVisible() override;
  gfx::PointF GetLocation() override;
  gfx::Rect GetCursorConfinedBounds() override;

  // GpuPlatformSupportHost:
  void OnChannelEstablished(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> send_runner,
      const base::Callback<void(IPC::Message*)>& sender) override;
  void OnChannelDestroyed(int host_id) override;

  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  // Set the location (clamps to cursor bounds).
  void SetCursorLocationLocked(const gfx::PointF& location);

  // The location of the bitmap (the cursor location is the hotspot location).
  gfx::Point GetBitmapLocationLocked();

  // IPC-related functions.
  bool IsConnectedLocked();
  void SendCursorShowLocked();
  void SendCursorHideLocked();
  void SendCursorMoveLocked();
  void SendLocked(IPC::Message* message);

  DrmWindowHostManager* window_manager_;                  // Not owned.

  // Task runner for main thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  struct CursorState {
    CursorState();
    ~CursorState();

    // The current cursor bitmap (immutable).
    scoped_refptr<BitmapCursorOzone> bitmap;

    // The window under the cursor.
    gfx::AcceleratedWidget window;

    // The location of the cursor within the window.
    gfx::PointF location;

    // The bounds of the display under the cursor.
    gfx::Rect display_bounds_in_screen;

    // The bounds that the cursor is confined to in |window|.
    gfx::Rect confined_bounds;

    int host_id;

    // Callback for IPC updates.
    base::Callback<void(IPC::Message*)> send_callback;
    scoped_refptr<base::SingleThreadTaskRunner> send_runner;

    // The mutex synchronizing this object.
    base::Lock lock;
  };

  CursorState state_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_CURSOR_H_
