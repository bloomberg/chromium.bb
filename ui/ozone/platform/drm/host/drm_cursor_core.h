// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_CURSOR_CORE_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_CURSOR_CORE_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class BitmapCursorOzone;
class DrmWindowHostManager;

// DrmCursorCore manages all cursor state but is dependent on an injected
// proxy for how it communicates state changes to other threads or
// processes. The proxy implementation must satisfy DrmCursorProxy.
class DrmCursorProxy {
 public:
  // Sets the cursor |bitmaps| on |window| at |point| with |frame_delay_ms|.
  virtual void CursorSet(gfx::AcceleratedWidget window,
                         const std::vector<SkBitmap>& bitmaps,
                         gfx::Point point,
                         int frame_delay_ms) = 0;
  // Moves the cursor in |window| to |point|
  virtual void Move(gfx::AcceleratedWidget window, gfx::Point point) = 0;
};

// DrmCursorCore manages all cursor state and semantics.
class DrmCursorCore {
 public:
  DrmCursorCore(DrmCursorProxy* proxy, DrmWindowHostManager* window_manager);
  ~DrmCursorCore();

  // Change the cursor over the specifed window.
  void SetCursor(gfx::AcceleratedWidget window,
                 scoped_refptr<BitmapCursorOzone> platform_cursor);

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
  void MoveCursorTo(gfx::AcceleratedWidget window, const gfx::PointF& location);
  void MoveCursorTo(const gfx::PointF& screen_location);
  void MoveCursor(const gfx::Vector2dF& delta);
  bool IsCursorVisible();
  gfx::PointF GetLocation();
  gfx::Rect GetCursorConfinedBounds();

 private:
  void SetCursorLocation(const gfx::PointF& location);
  void SendCursorShow();
  void SendCursorHide();
  void SendCursorMove();

  // The location of the bitmap (the cursor location is the hotspot location).
  gfx::Point GetBitmapLocationLocked();

  // The current cursor bitmap (immutable).
  scoped_refptr<BitmapCursorOzone> bitmap_;

  // The window under the cursor.
  gfx::AcceleratedWidget window_;

  // The location of the cursor within the window.
  gfx::PointF location_;

  // The bounds of the display under the cursor.
  gfx::Rect display_bounds_in_screen_;

  // The bounds that the cursor is confined to in |window|.
  gfx::Rect confined_bounds_;

  DrmWindowHostManager* window_manager_;  // Not owned.

  // Lifetime managed by the caller.
  DrmCursorProxy* proxy_;

  DISALLOW_COPY_AND_ASSIGN(DrmCursorCore);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_CURSOR_H_
