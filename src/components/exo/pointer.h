// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_POINTER_H_
#define COMPONENTS_EXO_POINTER_H_

#include <memory>

#include "ash/display/window_tree_host_manager.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "components/exo/surface_observer.h"
#include "components/exo/surface_tree_host.h"
#include "components/exo/wm_helper.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/client/capture_client_observer.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/native_widget_types.h"

namespace viz {
class CopyOutputResult;
}

namespace ui {
class LocatedEvent;
class MouseEvent;
}

namespace exo {
class PointerDelegate;
class PointerGesturePinchDelegate;
class RelativePointerDelegate;
class Surface;
class SurfaceTreeHost;

// This class implements a client pointer that represents one or more input
// devices, such as mice, which control the pointer location and pointer focus.
class Pointer : public SurfaceTreeHost,
                public SurfaceObserver,
                public ui::EventHandler,
                public aura::client::CaptureClientObserver,
                public aura::client::CursorClientObserver,
                public aura::client::FocusChangeObserver,
                public ash::WindowTreeHostManager::Observer {
 public:
  explicit Pointer(PointerDelegate* delegate);
  ~Pointer() override;

  PointerDelegate* delegate() const { return delegate_; }

  // Set the pointer surface, i.e., the surface that contains the pointer image
  // (cursor). The |hotspot| argument defines the position of the pointer
  // surface relative to the pointer location. Its top-left corner is always at
  // (x, y) - (hotspot.x, hotspot.y), where (x, y) are the coordinates of the
  // pointer location, in surface local coordinates.
  void SetCursor(Surface* surface, const gfx::Point& hotspot);

  // Set the pointer cursor type. This is similar to SetCursor, but this method
  // accepts ui::CursorType instead of the surface for the pointer image.
  void SetCursorType(ui::CursorType cursor_type);

  // Set delegate for pinch events.
  void SetGesturePinchDelegate(PointerGesturePinchDelegate* delegate);

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  // Overridden from ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from aura::client::CaptureClientObserver:
  void OnCaptureChanged(aura::Window* lost_capture,
                        aura::Window* gained_capture) override;

  // Overridden from aura::client::CursorClientObserver:
  void OnCursorSizeChanged(ui::CursorSize cursor_size) override;
  void OnCursorDisplayChanged(const display::Display& display) override;

  // Overridden from aura::client::FocusChangeObserver;
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overridden from ash::WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // Relative motion registration.
  void RegisterRelativePointerDelegate(RelativePointerDelegate* delegate);
  void UnregisterRelativePointerDelegate(RelativePointerDelegate* delegate);

  // Pointer capture toggles:
  void EnablePointerCapture();
  void DisablePointerCapture();

 private:
  // Returns the effective target for |event|.
  Surface* GetEffectiveTargetForEvent(ui::LocatedEvent* event) const;

  // Change pointer focus to |surface|.
  void SetFocus(Surface* surface,
                const gfx::PointF& location,
                int button_flags);

  // Updates the root_surface in |SurfaceTreeHost| from which the cursor
  // is captured.
  void UpdatePointerSurface(Surface* surface);

  // Asynchronously update the cursor by capturing a snapshot of
  // |SurfaceTreeHost::root_surface()|.
  void CaptureCursor(const gfx::Point& hotspot);

  // Called when cursor snapshot has been captured.
  void OnCursorCaptured(const gfx::Point& hotspot,
                        std::unique_ptr<viz::CopyOutputResult> result);

  // Update |cursor_| to |cursor_bitmap_| transformed for the current display.
  void UpdateCursor();

  // Convert the given |location_in_target| to coordinates in the root window.
  gfx::PointF GetLocationInRoot(Surface* target,
                                gfx::PointF location_in_target);

  // Called to check if cursor should be moved to the center of the window when
  // sending relative movements.
  bool ShouldMoveToCenter();

  // Moves the cursor to center of the active display.
  void MoveCursorToCenterOfActiveDisplay();

  // Process the delta for relative pointer motion.
  void HandleRelativePointerMotion(base::TimeTicks time_stamp,
                                   gfx::PointF location_in_target);

  // The delegate instance that all events are dispatched to.
  PointerDelegate* const delegate_;

  // The delegate instance that all pinch related events are dispatched to.
  PointerGesturePinchDelegate* pinch_delegate_ = nullptr;

  // The delegate instance that relative movement events are dispatched to.
  RelativePointerDelegate* relative_pointer_delegate_ = nullptr;

  // The current focus surface for the pointer.
  Surface* focus_surface_ = nullptr;

  // The location of the pointer in the current focus surface.
  gfx::PointF location_;

  // The location of the pointer when pointer capture is first enabled.
  base::Optional<gfx::Point> location_when_pointer_capture_enabled_;

  // If this is not nullptr, a synthetic move was sent and this points to the
  // location of a generated move that was sent which should not be forwarded.
  base::Optional<gfx::Point> location_synthetic_move_;

  // The window with input capture. Pointer capture is enabled if and only if
  // this is not null.
  aura::Window* capture_window_ = nullptr;

  // The position of the pointer surface relative to the pointer location.
  gfx::Point hotspot_;

  // Latest cursor snapshot.
  SkBitmap cursor_bitmap_;

  // The current cursor.
  ui::Cursor cursor_;

  // Hotspot to use with latest cursor snapshot.
  gfx::Point cursor_hotspot_;

  // Scale at which cursor snapshot is captured.
  float capture_scale_;

  // Density ratio of the cursor snapshot. The bitmap is scaled on displays with
  // a different ratio.
  float capture_ratio_;

  // Source used for cursor capture copy output requests.
  const base::UnguessableToken cursor_capture_source_id_;

  // Last received event type.
  ui::EventType last_event_type_ = ui::ET_UNKNOWN;

  // Weak pointer factory used for cursor capture callbacks.
  base::WeakPtrFactory<Pointer> cursor_capture_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Pointer);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_POINTER_H_
