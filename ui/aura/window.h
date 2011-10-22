// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_H_
#define UI_AURA_WINDOW_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "ui/base/events.h"
#include "ui/base/ui_base_types.h"
#include "ui/aura/aura_export.h"
#include "ui/gfx/compositor/layer_delegate.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

class SkCanvas;

namespace ui {
class Animation;
class Compositor;
class Layer;
class Transform;
}

namespace aura {

class Desktop;
class EventFilter;
class KeyEvent;
class LayoutManager;
class MouseEvent;
class ToplevelWindowContainer;
class TouchEvent;
class WindowDelegate;
class WindowObserver;

namespace internal {
class FocusManager;
}

// Aura window implementation. Interesting events are sent to the
// WindowDelegate.
// TODO(beng): resolve ownership.
class AURA_EXPORT Window : public ui::LayerDelegate {
 public:
  typedef std::vector<Window*> Windows;

  explicit Window(WindowDelegate* delegate);
  virtual ~Window();

  void Init();

  // A type is used to identify a class of Windows and customize behavior such
  // as event handling and parenting. The value can be any of those in
  // window_types.h or a user defined value.
  int type() const { return type_; }
  void SetType(int type);

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  ui::Layer* layer() { return layer_.get(); }
  const ui::Layer* layer() const { return layer_.get(); }

  WindowDelegate* delegate() { return delegate_; }

  const gfx::Rect& bounds() const;

  Window* parent() { return parent_; }
  const Window* parent() const { return parent_; }

  // The Window does not own this object.
  void set_user_data(void* user_data) { user_data_ = user_data; }
  void* user_data() const { return user_data_; }

  // Changes the visibility of the window.
  void Show();
  void Hide();
  // Returns true if this window and all its ancestors are visible.
  bool IsVisible() const;

  // Maximize the window.
  void Maximize();

  // Go into fullscreen mode.
  void Fullscreen();

  // Restore the window to its original bounds.
  void Restore();

  // Returns the window's bounds in screen coordinates.
  gfx::Rect GetScreenBounds() const;

  // Returns the window's show state.
  ui::WindowShowState show_state() const { return show_state_; }

  // Activates this window. Only top level windows can be activated. Requests
  // to activate a non-top level window are ignored.
  void Activate();

  // Deactivates this window. Only top level windows can be
  // deactivated. Requests to deactivate a non-top level window are ignored.
  void Deactivate();

  // Returns true if this window is active.
  bool IsActive() const;

  // RTTI to a container for top-level windows. Returns NULL if this window is
  // not a top level window container.
  virtual ToplevelWindowContainer* AsToplevelWindowContainer();
  virtual const ToplevelWindowContainer* AsToplevelWindowContainer() const;

  virtual void SetTransform(const ui::Transform& transform);

  // Assigns a LayoutManager to size and place child windows.
  // The Window takes ownership of the LayoutManager.
  void SetLayoutManager(LayoutManager* layout_manager);
  LayoutManager* layout_manager() { return layout_manager_.get(); }

  // Changes the bounds of the window. If present, the window's parent's
  // LayoutManager may adjust the bounds.
  void SetBounds(const gfx::Rect& new_bounds);

  // Returns the target bounds of the window. If the window's layer is
  // not animating, it simply returns the current bounds.
  gfx::Rect GetTargetBounds() const;

  // Sets the minimum size of the window that a user can resize it to.
  // A smaller size can still be set using SetBounds().
  void set_minimum_size(const gfx::Size& minimum_size) {
    minimum_size_ = minimum_size;
  }
  const gfx::Size& minimum_size() const { return minimum_size_; }

  // Marks the a portion of window as needing to be painted.
  void SchedulePaintInRect(const gfx::Rect& rect);

  // Sets the contents of the window.
  void SetCanvas(const SkCanvas& canvas, const gfx::Point& origin);

  // Sets the parent window of the window. If NULL, the window is parented to
  // the desktop's window.
  void SetParent(Window* parent);

  // Move the specified child of this Window to the front of the z-order.
  void MoveChildToFront(Window* child);

  // Returns true if this window can be activated.
  bool CanActivate() const;

  // Tree operations.
  // TODO(beng): Child windows are currently not owned by the hierarchy. We
  //             should change this.
  void AddChild(Window* child);
  void RemoveChild(Window* child);

  const Windows& children() const { return children_; }

  // Retrieves the first-level child with the specified id, or NULL if no first-
  // level child is found matching |id|.
  Window* GetChildById(int id);
  const Window* GetChildById(int id) const;

  static void ConvertPointToWindow(const Window* source,
                                   const Window* target,
                                   gfx::Point* point);

  // Returns the cursor for the specified point, in window coordinates.
  gfx::NativeCursor GetCursor(const gfx::Point& point) const;

  // Window takes ownership of the EventFilter.
  void SetEventFilter(EventFilter* event_filter);

  // Handles a mouse event. Returns true if handled.
  bool OnMouseEvent(MouseEvent* event);

  // Handles a key event. Returns true if handled.
  bool OnKeyEvent(KeyEvent* event);

  // Handles a touch event.
  ui::TouchStatus OnTouchEvent(TouchEvent* event);

  // Add/remove observer.
  void AddObserver(WindowObserver* observer);
  void RemoveObserver(WindowObserver* observer);

  // When set to true, this Window will stop propagation of all events targeted
  // at Windows below it in the z-order, but only if this Window has children.
  // This is used to implement lock-screen type functionality where we do not
  // want events to be sent to running logged-in windows when the lock screen is
  // displayed.
  void set_stops_event_propagation(bool stops_event_propagation) {
    stops_event_propagation_ = stops_event_propagation;
  }

  // Returns true if relative-to-this-Window's-origin |local_point| falls
  // within this Window's bounds.
  bool ContainsPoint(const gfx::Point& local_point);

  // Returns true if the mouse pointer at relative-to-this-Window's-origin
  // |local_point| can trigger an event for this Window.
  // TODO(beng): A Window can supply a hit-test mask to cause some portions of
  // itself to not trigger events, causing the events to fall through to the
  // Window behind.
  bool HitTest(const gfx::Point& local_point);

  // Returns the Window that most closely encloses |local_point| for the
  // purposes of event targeting.
  Window* GetEventHandlerForPoint(const gfx::Point& local_point);

  // Returns the topmost Window with a delegate containing |local_point|.
  Window* GetTopWindowContainingPoint(const gfx::Point& local_point);

  // Claims or relinquishes the claim to focus.
  void Focus();
  void Blur();

  // Returns true if the Window is currently the focused window.
  bool HasFocus() const;

  // Returns true if the Window can be focused.
  virtual bool CanFocus() const;

  // Returns the FocusManager for the Window, which may be attached to a parent
  // Window. Can return NULL if the Window has no FocusManager.
  virtual internal::FocusManager* GetFocusManager();
  virtual const internal::FocusManager* GetFocusManager() const;

  // Does a mouse capture on the window. This does nothing if the window isn't
  // showing (VISIBILITY_SHOWN) or isn't contained in a valid window hierarchy.
  void SetCapture();

  // Releases a mouse capture.
  void ReleaseCapture();

  // Returns true if this window has a mouse capture.
  bool HasCapture();

  // Returns the first ancestor whose parent window returns true from
  // IsToplevelWindowContainer.
  Window* GetToplevelWindow();

  // Returns true if this window is fullscreen or contains a fullscreen window.
  bool IsOrContainsFullscreenWindow() const;

  // Returns an animation configured with the default duration. All animations
  // should use this. Caller owns returned value.
  static ui::Animation* CreateDefaultAnimation();

 protected:
  // Returns the desktop or NULL if we aren't yet attached to a desktop.
  virtual Desktop* GetDesktop();

 private:
  friend class LayoutManager;

  // Changes the bounds of the window without condition.
  void SetBoundsInternal(const gfx::Rect& new_bounds);

  // Updates the visible state of the layer, but does not make visible-state
  // specific changes. Called from Show()/Hide().
  void SetVisible(bool visible);

  // Schedules a paint for the Window's entire bounds.
  void SchedulePaint();

  // This window is currently stopping event propagation for any windows behind
  // it in the z-order.
  bool StopsEventPropagation() const;

  // Update the show state and restore bounds. Returns false
  // if |new_show_state| is same as current show state.
  bool UpdateShowStateAndRestoreBounds(ui::WindowShowState new_show_state);

  // Gets a Window (either this one or a subwindow) containing |local_point|.
  // If |return_tightest| is true, returns the tightest-containing (i.e.
  // furthest down the hierarchy) Window containing the point; otherwise,
  // returns the loosest.  If |for_event_handling| is true, then hit-test masks
  // and StopsEventPropagation() are honored; otherwise, only bounds checks are
  // performed.
  Window* GetWindowForPoint(const gfx::Point& local_point,
                            bool return_tightest,
                            bool for_event_handling);

  // Overridden from ui::LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnLayerAnimationEnded(const ui::Animation* animation) OVERRIDE;

  int type_;

  WindowDelegate* delegate_;

  ui::WindowShowState show_state_;

  // The original bounds of a maximized/fullscreen window.
  gfx::Rect restore_bounds_;

  // The minimum size of the window a user can resize to.
  gfx::Size minimum_size_;

  scoped_ptr<ui::Layer> layer_;

  // The Window's parent.
  Window* parent_;

  // Child windows. Topmost is last.
  Windows children_;

  int id_;
  std::string name_;

  scoped_ptr<EventFilter> event_filter_;
  scoped_ptr<LayoutManager> layout_manager_;

  void* user_data_;

  // When true, events are not sent to windows behind this one in the z-order,
  // provided this window has children. See set_stops_event_propagation().
  bool stops_event_propagation_;

  ObserverList<WindowObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_H_
