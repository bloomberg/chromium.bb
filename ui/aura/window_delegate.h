// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_DELEGATE_H_
#define UI_AURA_WINDOW_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "ui/aura/aura_export.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Path;
class Point;
class Rect;
class Size;
}

namespace ui {
class PaintContext;
}

namespace viz {
class SurfaceInfo;
}

namespace aura {

// Delegate interface for aura::Window.
class AURA_EXPORT WindowDelegate : public ui::EventHandler {
 public:
  // Returns the window's minimum size, or size 0,0 if there is no limit.
  virtual gfx::Size GetMinimumSize() const = 0;

  // Returns the window's maximum size, or size 0,0 if there is no limit.
  virtual gfx::Size GetMaximumSize() const = 0;

  // Called when the Window's position and/or size changes.
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) = 0;

  // Returns the native cursor for the specified point, in window coordinates,
  // or NULL for the default cursor.
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) = 0;

  // Returns the non-client component (see hit_test.h) containing |point|, in
  // window coordinates.
  virtual int GetNonClientComponent(const gfx::Point& point) const = 0;

  // Returns true if event handling should descend into |child|. |location| is
  // in terms of the Window.
  virtual bool ShouldDescendIntoChildForEventHandling(
      Window* child,
      const gfx::Point& location) = 0;

  // Returns true of the window can be focused.
  virtual bool CanFocus() = 0;

  // Invoked when mouse capture is lost on the window.
  virtual void OnCaptureLost() = 0;

  // Asks the delegate to paint window contents into the supplied context.
  virtual void OnPaint(const ui::PaintContext& context) = 0;

  // Called when the window's device scale factor has changed.
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) = 0;

  // Called from Window's destructor before OnWindowDestroyed and before the
  // children have been destroyed and the window has been removed from its
  // parent.
  // This method takes the window because the delegate implementation may no
  // longer have a route back to the window by the time this method is called.
  virtual void OnWindowDestroying(Window* window) = 0;

  // Called when the Window has been destroyed (i.e. from its destructor). This
  // is called after OnWindowDestroying and after the children have been
  // deleted and the window has been removed from its parent.
  // The delegate can use this as an opportunity to delete itself if necessary.
  // This method takes the window because the delegate implementation may no
  // longer have a route back to the window by the time this method is called.
  virtual void OnWindowDestroyed(Window* window) = 0;

  // Called when the TargetVisibility() of a Window changes. |visible|
  // corresponds to the target visibility of the window. See
  // Window::TargetVisibility() for details.
  virtual void OnWindowTargetVisibilityChanged(bool visible) = 0;

  // Called from Window::HitTest to check if the window has a custom hit test
  // mask. It works similar to the views counterparts. That is, if the function
  // returns true, GetHitTestMask below will be called to get the mask.
  // Otherwise, Window will hit-test against its bounds.
  virtual bool HasHitTestMask() const = 0;

  // Called from Window::HitTest to retrieve hit test mask when HasHitTestMask
  // above returns true.
  virtual void GetHitTestMask(gfx::Path* mask) const = 0;

  // Called when a child submits a CompositorFrame to a surface with the given
  // |surface_info| for the first time.
  virtual void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) {}

 protected:
  ~WindowDelegate() override {}
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_DELEGATE_H_
