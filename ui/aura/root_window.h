// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ROOT_WINDOW_H_
#define UI_AURA_ROOT_WINDOW_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
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

class RootWindowHost;
class RootWindowObserver;
class KeyEvent;
class MouseEvent;
class ScreenAura;
class StackingClient;
class ScrollEvent;
class TouchEvent;
class GestureEvent;
class GestureRecognizer;

// RootWindow is responsible for hosting a set of windows.
class AURA_EXPORT RootWindow : public ui::CompositorDelegate,
                               public Window,
                               public internal::FocusManager,
                               public ui::LayerAnimationObserver {
 public:
  static RootWindow* GetInstance();
  static void DeleteInstance();

  static void set_use_fullscreen_host_window(bool use_fullscreen) {
    use_fullscreen_host_window_ = use_fullscreen;
  }
  static bool use_fullscreen_host_window() {
    return use_fullscreen_host_window_;
  }

  ui::Compositor* compositor() { return compositor_.get(); }
  gfx::Point last_mouse_location() const { return last_mouse_location_; }
  gfx::NativeCursor last_cursor() const { return last_cursor_; }
  Window* mouse_pressed_handler() { return mouse_pressed_handler_; }
  Window* capture_window() { return capture_window_; }
  ScreenAura* screen() { return screen_; }

  // Shows the root window host.
  void ShowRootWindow();

  // Sets the size of the root window.
  void SetHostSize(const gfx::Size& size);
  gfx::Size GetHostSize() const;

  // Shows the specified cursor.
  void SetCursor(gfx::NativeCursor cursor);

  // Sets current cursor visibility to |show|.
  void ShowCursor(bool show);

  // Moves the cursor to the specified location relative to the root window.
  void MoveCursorTo(const gfx::Point& location);

  // Shows the root window host and runs an event loop for it.
  void Run();

  // Draws the necessary set of windows.
  void Draw();

  // Handles a mouse event. Returns true if handled.
  bool DispatchMouseEvent(MouseEvent* event);

  // Handles a key event. Returns true if handled.
  bool DispatchKeyEvent(KeyEvent* event);

  // Handles a scroll event. Returns true if handled.
  bool DispatchScrollEvent(ScrollEvent* event);

  // Handles a touch event. Returns true if handled.
  bool DispatchTouchEvent(TouchEvent* event);

  // Handles a gesture event. Returns true if handled. Unlike the other
  // event-dispatching function (e.g. for touch/mouse/keyboard events), gesture
  // events are dispatched from GestureRecognizer instead of RootWindowHost.
  bool DispatchGestureEvent(GestureEvent* event);

  // Called when the host changes size.
  void OnHostResized(const gfx::Size& size);

  // Called when the native screen's resolution changes.
  void OnNativeScreenResized(const gfx::Size& size);

  // Invoked when |window| is initialized.
  void WindowInitialized(Window* window);

  // Invoked when |window| is being destroyed.
  void WindowDestroying(Window* window);

  // Returns the root window's dispatcher. The result should only be passed to
  // MessageLoopForUI::RunWithDispatcher() or
  // MessageLoopForUI::RunAllPendingWithDispatcher(), or used to dispatch
  // an event by |Dispatch(const NativeEvent&)| on it. It must never be stored.
  MessageLoop::Dispatcher* GetDispatcher();

  // Add/remove observer.
  void AddRootWindowObserver(RootWindowObserver* observer);
  void RemoveRootWindowObserver(RootWindowObserver* observer);

  // Are any mouse buttons currently down?
  bool IsMouseButtonDown() const;

  // Posts |native_event| to the platform's event queue.
  void PostNativeEvent(const base::NativeEvent& native_event);

  // Converts |point| from the root window's coordinate system to native
  // screen's.
  void ConvertPointToNativeScreen(gfx::Point* point) const;

  // Capture -------------------------------------------------------------------

  // Sets capture to the specified window.
  void SetCapture(Window* window);

  // If |window| has mouse capture, the current capture window is set to NULL.
  void ReleaseCapture(Window* window);

  // Overridden from Window:
  virtual void SetTransform(const ui::Transform& transform) OVERRIDE;

#if !defined(NDEBUG)
  // Toggles the host's full screen state.
  void ToggleFullScreen();
#endif

  // Provided only for testing:
  void SetGestureRecognizerForTesting(GestureRecognizer* gr) {
    gesture_recognizer_ = gr;
  }

  // Overridden from ui::CompositorDelegate:
  virtual void ScheduleDraw() OVERRIDE;

 private:
  RootWindow();
  virtual ~RootWindow();

  // Called whenever the mouse moves, tracks the current |mouse_moved_handler_|,
  // sending exited and entered events as its value changes.
  void HandleMouseMoved(const MouseEvent& event, Window* target);

  bool ProcessMouseEvent(Window* target, MouseEvent* event);
  bool ProcessKeyEvent(Window* target, KeyEvent* event);
  ui::TouchStatus ProcessTouchEvent(Window* target, TouchEvent* event);
  ui::GestureStatus ProcessGestureEvent(Window* target, GestureEvent* event);

  // Overridden from Window:
  virtual bool CanFocus() const OVERRIDE;
  virtual bool CanReceiveEvents() const OVERRIDE;
  virtual internal::FocusManager* GetFocusManager() OVERRIDE;
  virtual RootWindow* GetRootWindow() OVERRIDE;
  virtual void WindowDetachedFromRootWindow(Window* window) OVERRIDE;

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

  // Initializes the root window.
  void Init();

  // Parses the switch describing the initial size for the host window and
  // returns bounds for the window.
  gfx::Rect GetInitialHostWindowBounds() const;

  scoped_refptr<ui::Compositor> compositor_;

  scoped_ptr<RootWindowHost> host_;

  static RootWindow* instance_;

  // If set before the RootWindow is created, the host window will cover the
  // entire screen.  Note that this can still be overridden via the
  // switches::kAuraHostWindowSize flag.
  static bool use_fullscreen_host_window_;

  // Used to schedule painting.
  base::WeakPtrFactory<RootWindow> schedule_paint_factory_;

  // Last location seen in a mouse event.
  gfx::Point last_mouse_location_;

  // ui::EventFlags containing the current state of the mouse buttons.
  int mouse_button_flags_;

  // Last cursor set.  Used for testing.
  gfx::NativeCursor last_cursor_;

  ObserverList<RootWindowObserver> observers_;

  ScreenAura* screen_;

  // The capture window. When not-null, this window receives all the mouse and
  // touch events.
  Window* capture_window_;

  Window* mouse_pressed_handler_;
  Window* mouse_moved_handler_;
  Window* focused_window_;
  Window* touch_event_handler_;
  Window* gesture_handler_;

  // The gesture_recognizer_ for this.
  GestureRecognizer* gesture_recognizer_;

  DISALLOW_COPY_AND_ASSIGN(RootWindow);
};

}  // namespace aura

#endif  // UI_AURA_ROOT_WINDOW_H_
