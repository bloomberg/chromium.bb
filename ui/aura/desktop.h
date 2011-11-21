// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DESKTOP_H_
#define UI_AURA_DESKTOP_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/cursor.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/window.h"
#include "ui/base/events.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/layer_animation_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"

namespace gfx {
class Size;
}

namespace ui {
class LayerAnimationSequence;
class Transform;
}

namespace aura {

class DesktopHost;
class DesktopObserver;
class KeyEvent;
class MouseEvent;
class ScreenAura;
class StackingClient;
class TouchEvent;

// Desktop is responsible for hosting a set of windows.
class AURA_EXPORT Desktop : public ui::CompositorDelegate,
                            public Window,
                            public internal::FocusManager,
                            public ui::LayerAnimationObserver {
 public:
  Desktop();
  virtual ~Desktop();

  static Desktop* GetInstance();
  static void DeleteInstanceForTesting();

  static void set_use_fullscreen_host_window(bool use_fullscreen) {
    use_fullscreen_host_window_ = use_fullscreen;
  }

  ui::Compositor* compositor() { return compositor_.get(); }
  gfx::Point last_mouse_location() const { return last_mouse_location_; }
  gfx::NativeCursor last_cursor() const { return last_cursor_; }
  StackingClient* stacking_client() { return stacking_client_.get(); }
  Window* active_window() { return active_window_; }
  Window* mouse_pressed_handler() { return mouse_pressed_handler_; }
  Window* capture_window() { return capture_window_; }
  ScreenAura* screen() { return screen_; }

  void SetStackingClient(StackingClient* stacking_client);

  // Shows the desktop host.
  void ShowDesktop();

  // Sets the size of the desktop.
  void SetHostSize(const gfx::Size& size);
  gfx::Size GetHostSize() const;

  // Shows the specified cursor.
  void SetCursor(gfx::NativeCursor cursor);

  // Shows the desktop host and runs an event loop for it.
  void Run();

  // Draws the necessary set of windows.
  void Draw();

  // Handles a mouse event. Returns true if handled.
  bool DispatchMouseEvent(MouseEvent* event);

  // Handles a key event. Returns true if handled.
  bool DispatchKeyEvent(KeyEvent* event);

  // Handles a touch event. Returns true if handled.
  bool DispatchTouchEvent(TouchEvent* event);

  // Called when the host changes size.
  void OnHostResized(const gfx::Size& size);

  // Sets the active window to |window| and the focused window to |to_focus|.
  // If |to_focus| is NULL, |window| is focused. Does nothing if |window| is
  // NULL.
  void SetActiveWindow(Window* window, Window* to_focus);

  // Activates the topmost window. Does nothing if the topmost window is already
  // active.
  void ActivateTopmostWindow();

  // Deactivates |window| and activates the topmost window. Does nothing if
  // |window| is not a topmost window, or there are no other suitable windows to
  // activate.
  void Deactivate(Window* window);

  // Invoked when |window| is initialized.
  void WindowInitialized(Window* window);

  // Invoked when |window| is being destroyed.
  void WindowDestroying(Window* window);

  // Returns the desktop's dispatcher. The result should only be passed to
  // MessageLoopForUI::RunWithDispatcher() or
  // MessageLoopForUI::RunAllPendingWithDispatcher(), or used to dispatch
  // an event by |Dispatch(const NativeEvent&)| on it. It must never be stored.
  MessageLoop::Dispatcher* GetDispatcher();

  // Add/remove observer.
  void AddObserver(DesktopObserver* observer);
  void RemoveObserver(DesktopObserver* observer);

  // Are any mouse buttons currently down?
  bool IsMouseButtonDown() const;

  // Posts |native_event| to the platform's event queue.
  void PostNativeEvent(const base::NativeEvent& native_event);

  // Capture -------------------------------------------------------------------

  // Sets capture to the specified window.
  void SetCapture(Window* window);

  // If |window| has mouse capture, the current capture window is set to NULL.
  void ReleaseCapture(Window* window);

  // Overridden from Window:
  virtual void SetTransform(const ui::Transform& transform) OVERRIDE;

 private:
  // Called whenever the mouse moves, tracks the current |mouse_moved_handler_|,
  // sending exited and entered events as its value changes.
  void HandleMouseMoved(const MouseEvent& event, Window* target);

  bool ProcessMouseEvent(Window* target, MouseEvent* event);
  bool ProcessKeyEvent(Window* target, KeyEvent* event);
  ui::TouchStatus ProcessTouchEvent(Window* target, TouchEvent* event);

  // Overridden from ui::CompositorDelegate
  virtual void ScheduleDraw();

  // Overridden from Window:
  virtual bool CanFocus() const OVERRIDE;
  virtual internal::FocusManager* GetFocusManager() OVERRIDE;
  virtual Desktop* GetDesktop() OVERRIDE;
  virtual void WindowDetachedFromDesktop(Window* window) OVERRIDE;

  // Overridden from ui::LayerAnimationObserver:
  virtual void OnLayerAnimationEnded(
      const ui::LayerAnimationSequence* animation) OVERRIDE;
  virtual void OnLayerAnimationScheduled(
      const ui::LayerAnimationSequence* animation) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      const ui::LayerAnimationSequence* animation) OVERRIDE;

  // Overridden from FocusManager:
  virtual void SetFocusedWindow(Window* window) OVERRIDE;
  virtual Window* GetFocusedWindow() OVERRIDE;
  virtual bool IsFocusedWindow(const Window* window) const OVERRIDE;

  // Initializes the desktop.
  void Init();

  // Parses the switch describing the initial size for the host window and
  // returns bounds for the window.
  gfx::Rect GetInitialHostWindowBounds() const;

  scoped_refptr<ui::Compositor> compositor_;

  scoped_ptr<DesktopHost> host_;

  scoped_ptr<StackingClient> stacking_client_;

  static Desktop* instance_;

  // If set before the Desktop is created, the host window will cover the entire
  // screen.  Note that this can still be overridden via the
  // switches::kAuraHostWindowSize flag.
  static bool use_fullscreen_host_window_;

  // Used to schedule painting.
  base::WeakPtrFactory<Desktop> schedule_paint_factory_;

  Window* active_window_;

  // Last location seen in a mouse event.
  gfx::Point last_mouse_location_;

  // ui::EventFlags containing the current state of the mouse buttons.
  int mouse_button_flags_;

  // Last cursor set.  Used for testing.
  gfx::NativeCursor last_cursor_;

  // Are we in the process of being destroyed? Used to avoid processing during
  // destruction.
  bool in_destructor_;

  ObserverList<DesktopObserver> observers_;

  ScreenAura* screen_;

  // The capture window. When not-null, this window receives all the mouse and
  // touch events.
  Window* capture_window_;

  Window* mouse_pressed_handler_;
  Window* mouse_moved_handler_;
  Window* focused_window_;
  Window* touch_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(Desktop);
};

}  // namespace aura

#endif  // UI_AURA_DESKTOP_H_
