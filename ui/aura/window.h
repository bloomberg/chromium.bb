// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_H_
#define UI_AURA_WINDOW_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "ui/aura/aura_export.h"
#include "ui/gfx/compositor/layer_delegate.h"
#include "ui/gfx/rect.h"

class SkCanvas;

namespace ui {
class Animation;
class Compositor;
class Layer;
}

namespace aura {

class Desktop;
class EventFilter;
class KeyEvent;
class LayoutManager;
class MouseEvent;
class WindowDelegate;

namespace internal {
class FocusManager;
class RootWindow;
}

// Aura window implementation. Interesting events are sent to the
// WindowDelegate.
// TODO(beng): resolve ownership.
class AURA_EXPORT Window : public ui::LayerDelegate {
 public:
  typedef std::vector<Window*> Windows;

  enum Visibility {
    // Don't display the window onscreen and don't let it receive mouse
    // events. This is the default.
    VISIBILITY_HIDDEN = 1,

    // Display the window and let it receive mouse events.
    VISIBILITY_SHOWN = 2,

    // Display the window but prevent it from receiving mouse events.
    VISIBILITY_SHOWN_NO_INPUT = 3,
  };

  explicit Window(WindowDelegate* delegate);
  ~Window();

  void Init();

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  const string16& name() const { return name_; }
  void set_name(const string16& name) { name_ = name; }

  ui::Layer* layer() { return layer_.get(); }
  const ui::Layer* layer() const { return layer_.get(); }

  // Changes the visibility of the window.
  void SetVisibility(Visibility visibility);
  Visibility visibility() const { return visibility_; }

  // Assigns a LayoutManager to size and place child windows.
  // The Window takes ownership of the LayoutManager.
  void SetLayoutManager(LayoutManager* layout_manager);

  // Changes the bounds of the window.
  void SetBounds(const gfx::Rect& new_bounds);
  const gfx::Rect& bounds() const;

  // Marks the a portion of window as needing to be painted.
  void SchedulePaintInRect(const gfx::Rect& rect);

  // Sets the contents of the window.
  void SetCanvas(const SkCanvas& canvas, const gfx::Point& origin);

  // Sets the parent window of the window. If NULL, the window is parented to
  // the desktop's window.
  void SetParent(Window* parent);
  Window* parent() { return parent_; }

  // Returns true if this Window is the container for toplevel windows.
  virtual bool IsToplevelWindowContainer() const;

  // Move the specified child of this Window to the front of the z-order.
  // TODO(beng): this is (obviously) feeble.
  void MoveChildToFront(Window* child);

  // Tree operations.
  // TODO(beng): Child windows are currently not owned by the hierarchy. We
  //             should change this.
  void AddChild(Window* child);
  void RemoveChild(Window* child);

  const Windows& children() const { return children_; }

  static void ConvertPointToWindow(Window* source,
                                   Window* target,
                                   gfx::Point* point);

  // Window takes ownership of the EventFilter.
  void SetEventFilter(EventFilter* event_filter);

  // Handles a mouse event. Returns true if handled.
  bool OnMouseEvent(MouseEvent* event);

  // Handles a key event. Returns true if handled.
  bool OnKeyEvent(KeyEvent* event);

  WindowDelegate* delegate() { return delegate_; }

  // Returns true if the mouse pointer at the specified |point| can trigger an
  // event for this Window.
  // TODO(beng):
  // A Window can supply a hit-test mask to cause some portions of itself to not
  // trigger events, causing the events to fall through to the Window behind.
  bool HitTest(const gfx::Point& point);

  // Returns the Window that most closely encloses |point| for the purposes of
  // event targeting.
  Window* GetEventHandlerForPoint(const gfx::Point& point);

  // Returns the FocusManager for the Window, which may be attached to a parent
  // Window. Can return NULL if the Window has no FocusManager.
  virtual internal::FocusManager* GetFocusManager();

  // The Window does not own this object.
  void set_user_data(void* user_data) { user_data_ = user_data; }
  void* user_data() const { return user_data_; }

  // Does a mouse capture on the window. This does nothing if the window isn't
  // showing (VISIBILITY_SHOWN) or isn't contained in a valid window hierarchy.
  void SetCapture();

  // Releases a mouse capture.
  void ReleaseCapture();

  // Returns true if this window has a mouse capture.
  bool HasCapture();

  // Returns an animation configured with the default duration. All animations
  // should use this. Caller owns returned value.
  static ui::Animation* CreateDefaultAnimation();

 protected:
  // Returns the RootWindow or NULL if we don't yet have a RootWindow.
  virtual internal::RootWindow* GetRoot();

 private:
  // Schedules a paint for the Window's entire bounds.
  void SchedulePaint();

  // Overridden from ui::LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE;

  WindowDelegate* delegate_;

  Visibility visibility_;

  scoped_ptr<ui::Layer> layer_;

  // The Window's parent.
  // TODO(beng): Implement NULL-ness for toplevels.
  Window* parent_;

  // Child windows. Topmost is last.
  Windows children_;

  int id_;
  string16 name_;

  scoped_ptr<EventFilter> event_filter_;
  scoped_ptr<LayoutManager> layout_manager_;

  void* user_data_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_H_
