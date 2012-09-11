// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ROOT_WINDOW_H_
#define UI_AURA_ROOT_WINDOW_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/client/capture_delegate.h"
#include "ui/aura/root_window_host_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/events/event_dispatcher.h"
#include "ui/base/gestures/gesture_recognizer.h"
#include "ui/base/gestures/gesture_types.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"

namespace gfx {
class Size;
}

namespace ui {
class GestureEvent;
class GestureRecognizer;
class KeyEvent;
class LayerAnimationSequence;
class MouseEvent;
class ScrollEvent;
class TouchEvent;
class Transform;
}

namespace aura {

class FocusManager;
class RootWindow;
class RootWindowHost;
class RootWindowObserver;

// This class represents a lock on the compositor, that can be used to prevent a
// compositing pass from happening while we're waiting for an asynchronous
// event. The typical use case is when waiting for a renderer to produce a frame
// at the right size. The caller keeps a reference on this object, and drops the
// reference once it desires to release the lock.
// Note however that the lock is canceled after a short timeout to ensure
// responsiveness of the UI, so the compositor tree should be kept in a
// "reasonable" state while the lock is held.
// Don't instantiate this class directly, use RootWindow::GetCompositorLock.
class AURA_EXPORT CompositorLock
    : public base::RefCounted<CompositorLock>,
      public base::SupportsWeakPtr<CompositorLock> {
 private:
  friend class base::RefCounted<CompositorLock>;
  friend class RootWindow;

  explicit CompositorLock(RootWindow* root_window);
  ~CompositorLock();

  void CancelLock();

  RootWindow* root_window_;
  DISALLOW_COPY_AND_ASSIGN(CompositorLock);
};

// RootWindow is responsible for hosting a set of windows.
class AURA_EXPORT RootWindow : public ui::CompositorDelegate,
                               public ui::CompositorObserver,
                               public Window,
                               public ui::EventDispatcher,
                               public ui::GestureEventHelper,
                               public ui::LayerAnimationObserver,
                               public aura::client::CaptureDelegate,
                               public aura::RootWindowHostDelegate {
 public:
  struct AURA_EXPORT CreateParams {
    // CreateParams with initial_bounds and default host.
    CreateParams(const gfx::Rect& initial_bounds);
    ~CreateParams() {}

    gfx::Rect initial_bounds;

    // A host to use in place of the default one that RootWindow will create.
    // NULL by default.
    RootWindowHost* host;
  };

  explicit RootWindow(const CreateParams& params);
  virtual ~RootWindow();

  static RootWindow* GetForAcceleratedWidget(gfx::AcceleratedWidget widget);

  ui::Compositor* compositor() { return compositor_.get(); }
  gfx::NativeCursor last_cursor() const { return last_cursor_; }
  Window* mouse_pressed_handler() { return mouse_pressed_handler_; }
  bool cursor_shown() const { return cursor_shown_; }

  void set_focus_manager(FocusManager* focus_manager) {
    focus_manager_ = focus_manager;
  }

  // Initializes the root window.
  void Init();

  // Shows the root window host.
  void ShowRootWindow();

  // Hides the root window host.
  void HideRootWindow();

  // Stop listening events in preparation for shutdown.
  void PrepareForShutdown();

  RootWindowHostDelegate* AsRootWindowHostDelegate();

  // Sets the size of the root window.
  void SetHostSize(const gfx::Size& size_in_pixel);
  gfx::Size GetHostSize() const;

  // Sets the bounds of the host window.
  void SetHostBounds(const gfx::Rect& size_in_pixel);

  // Returns where the RootWindow is on screen.
  gfx::Point GetHostOrigin() const;

  // Sets the currently-displayed cursor. If the cursor was previously hidden
  // via ShowCursor(false), it will remain hidden until ShowCursor(true) is
  // called, at which point the cursor that was last set via SetCursor() will be
  // used.
  void SetCursor(gfx::NativeCursor cursor);

  // Shows or hides the cursor.
  void ShowCursor(bool show);

  // Moves the cursor to the specified location relative to the root window.
  virtual void MoveCursorTo(const gfx::Point& location) OVERRIDE;

  // Clips the cursor movement to the root_window.
  bool ConfineCursorToWindow();

  // Draws the necessary set of windows.
  void Draw();

  // Draw the whole screen.
  void ScheduleFullDraw();

  // Handles a gesture event. Returns true if handled. Unlike the other
  // event-dispatching function (e.g. for touch/mouse/keyboard events), gesture
  // events are dispatched from GestureRecognizer instead of RootWindowHost.
  bool DispatchGestureEvent(ui::GestureEvent* event);

  // Invoked when |window| is being destroyed.
  void OnWindowDestroying(Window* window);

  // Invoked when |window|'s bounds have changed. |contained_mouse| indicates if
  // the bounds before change contained the |last_moust_location()|.
  void OnWindowBoundsChanged(Window* window, bool contained_mouse);

  // Invoked when |window|'s visibility is changed.
  void OnWindowVisibilityChanged(Window* window, bool is_visible);

  // Invoked when |window|'s tranfrom has changed. |contained_mouse|
  // indicates if the bounds before change contained the
  // |last_moust_location()|.
  void OnWindowTransformed(Window* window, bool contained_mouse);

  // Invoked when the keyboard mapping (in X11 terms: the mapping between
  // keycodes and keysyms) has changed.
  void OnKeyboardMappingChanged();

  // The system windowing system has sent a request that we close our window.
  void OnRootWindowHostCloseRequested();

  // Add/remove observer. There is no need to remove the observer if
  // the root window is being deleted. In particular, you SHOULD NOT remove
  // in |WindowObserver::OnWindowDestroying| of the observer observing
  // the root window because it is too late to remove it.
  void AddRootWindowObserver(RootWindowObserver* observer);
  void RemoveRootWindowObserver(RootWindowObserver* observer);

  // Posts |native_event| to the platform's event queue.
  void PostNativeEvent(const base::NativeEvent& native_event);

  // Converts |point| from the root window's coordinate system to native
  // screen's.
  void ConvertPointToNativeScreen(gfx::Point* point) const;

  // Converts |point| from native screen coordinate system to the root window's.
  void ConvertPointFromNativeScreen(gfx::Point* point) const;

  // Gesture Recognition -------------------------------------------------------

  // When a touch event is dispatched to a Window, it can notify the RootWindow
  // to queue the touch event for asynchronous gesture recognition. These are
  // the entry points for the asynchronous processing of the queued touch
  // events.
  // Process the next touch event for gesture recognition. |processed| indicates
  // whether the queued event was processed by the window or not.
  void AdvanceQueuedTouchEvent(Window* window, bool processed);

  ui::GestureRecognizer* gesture_recognizer() const {
    return gesture_recognizer_.get();
  }

  // Provided only for testing:
  void SetGestureRecognizerForTesting(ui::GestureRecognizer* gr);

  // Returns the accelerated widget from the RootWindowHost.
  gfx::AcceleratedWidget GetAcceleratedWidget();

  // Toggles the host's full screen state.
  void ToggleFullScreen();

  // These methods are used to defer the processing of mouse events related
  // to resize. A client (typically a RenderWidgetHostViewAura) can call
  // HoldMouseMoves when an resize is initiated and then ReleaseMouseMoves
  // once the resize is completed.
  //
  // More than one hold can be invoked and each hold must be cancelled by a
  // release before we resume normal operation.
  void HoldMouseMoves();
  void ReleaseMouseMoves();

  // Creates a compositor lock.
  scoped_refptr<CompositorLock> GetCompositorLock();

  // Sets if the window should be focused when shown.
  void SetFocusWhenShown(bool focus_when_shown);

  // Grabs the snapshot of the root window by using the platform-dependent APIs.
  bool GrabSnapshot(const gfx::Rect& snapshot_bounds,
                    std::vector<unsigned char>* png_representation);

  // Gets the last location seen in a mouse event in this root window's
  // coordinates. This may return a point outside the root window's bounds.
  gfx::Point GetLastMouseLocationInRoot() const;

  // Overridden from Window:
  virtual RootWindow* GetRootWindow() OVERRIDE;
  virtual const RootWindow* GetRootWindow() const OVERRIDE;
  virtual void SetTransform(const ui::Transform& transform) OVERRIDE;

  // Overridden from ui::CompositorDelegate:
  virtual void ScheduleDraw() OVERRIDE;

  // Overridden from ui::CompositorObserver:
  virtual void OnCompositingDidCommit(ui::Compositor*) OVERRIDE;
  virtual void OnCompositingWillStart(ui::Compositor*) OVERRIDE;
  virtual void OnCompositingStarted(ui::Compositor*) OVERRIDE;
  virtual void OnCompositingEnded(ui::Compositor*) OVERRIDE;
  virtual void OnCompositingAborted(ui::Compositor*) OVERRIDE;

  // Overridden from ui::LayerDelegate:
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;

  // Overridden from Window:
  virtual bool CanFocus() const OVERRIDE;
  virtual bool CanReceiveEvents() const OVERRIDE;
  virtual FocusManager* GetFocusManager() OVERRIDE;

  // Overridden from aura::client::CaptureDelegate:
  virtual void UpdateCapture(Window* old_capture, Window* new_capture) OVERRIDE;
  virtual void SetNativeCapture() OVERRIDE;
  virtual void ReleaseNativeCapture() OVERRIDE;

  // Exposes RootWindowHost::QueryMouseLocation() for test purposes.
  gfx::Point QueryMouseLocationForTest() const;

 private:
  friend class Window;
  friend class CompositorLock;

  // Called whenever the mouse moves, tracks the current |mouse_moved_handler_|,
  // sending exited and entered events as its value changes.
  void HandleMouseMoved(const ui::MouseEvent& event, Window* target);

  bool ProcessMouseEvent(Window* target, ui::MouseEvent* event);
  bool ProcessKeyEvent(Window* target, ui::KeyEvent* event);
  ui::TouchStatus ProcessTouchEvent(Window* target, ui::TouchEvent* event);
  ui::EventResult ProcessGestureEvent(Window* target,
                                      ui::GestureEvent* event);
  bool ProcessGestures(ui::GestureRecognizer::Gestures* gestures);

  // Called when a Window is attached or detached from the RootWindow.
  void OnWindowAddedToRootWindow(Window* window);
  void OnWindowRemovedFromRootWindow(Window* window);

  // Called when a window becomes invisible, either by being removed
  // from root window hierachy, via SetVisible(false) or being destroyed.
  // |destroyed| is set to true when the window is being destroyed.
  void OnWindowHidden(Window* invisible, bool destroyed);

  // Cleans up the gesture recognizer for all windows in |window| (including
  // |window| itself).
  void CleanupGestureRecognizerState(Window* window);

  // Overridden from ui::EventDispatcher.
  virtual bool CanDispatchToTarget(EventTarget* target) OVERRIDE;
  virtual void ProcessPreTargetList(ui::EventHandlerList* list) OVERRIDE;
  virtual void ProcessPostTargetList(ui::EventHandlerList* list) OVERRIDE;

  // Overridden from ui::GestureEventHelper.
  virtual bool DispatchLongPressGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual bool DispatchCancelTouchEvent(ui::TouchEvent* event) OVERRIDE;

  // Overridden from ui::LayerAnimationObserver:
  virtual void OnLayerAnimationEnded(
      ui::LayerAnimationSequence* animation) OVERRIDE;
  virtual void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* animation) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      ui::LayerAnimationSequence* animation) OVERRIDE;

  // Overridden from aura::RootWindowHostDelegate:
  virtual bool OnHostKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual bool OnHostMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual bool OnHostScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual bool OnHostTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual void OnHostLostCapture() OVERRIDE;
  virtual void OnHostPaint() OVERRIDE;
  virtual void OnHostMoved(const gfx::Point& origin) OVERRIDE;
  virtual void OnHostResized(const gfx::Size& size) OVERRIDE;
  virtual float GetDeviceScaleFactor() OVERRIDE;
  virtual RootWindow* AsRootWindow() OVERRIDE;

  // We hold and aggregate mouse drags as a way of throttling resizes when
  // HoldMouseMoves() is called. The following methods are used to dispatch held
  // and newly incoming mouse events, typically when an event other than a mouse
  // drag needs dispatching or a matching ReleaseMouseMoves() is called.
  // NOTE: because these methods dispatch events from RootWindowHost the
  // coordinates are in terms of the root.
  bool DispatchMouseEventImpl(ui::MouseEvent* event);
  bool DispatchMouseEventToTarget(ui::MouseEvent* event, Window* target);
  void DispatchHeldMouseMove();

  // Parses the switch describing the initial size for the host window and
  // returns bounds for the window.
  gfx::Rect GetInitialHostWindowBounds() const;

  // Posts a task to send synthesized mouse move event if there
  // is no a pending task.
  void PostMouseMoveEventAfterWindowChange();

  // Creates and dispatches synthesized mouse move event using the
  // current mouse location.
  void SynthesizeMouseMoveEvent();

  // Called by CompositorLock.
  void UnlockCompositor();

  scoped_ptr<ui::Compositor> compositor_;

  scoped_ptr<RootWindowHost> host_;

  // Used to schedule painting.
  base::WeakPtrFactory<RootWindow> schedule_paint_factory_;

  // Use to post mouse move event.
  base::WeakPtrFactory<RootWindow> event_factory_;

  // ui::EventFlags containing the current state of the mouse buttons.
  int mouse_button_flags_;

  // Touch ids that are currently down.
  uint32 touch_ids_down_;

  // Last cursor set.  Used for testing.
  gfx::NativeCursor last_cursor_;

  // Is the cursor currently shown?  Used for testing.
  bool cursor_shown_;

  ObserverList<RootWindowObserver> observers_;

  Window* mouse_pressed_handler_;
  Window* mouse_moved_handler_;
  Window* mouse_event_dispatch_target_;
  Window* event_dispatch_target_;
  FocusManager* focus_manager_;

  // The gesture_recognizer_ for this.
  scoped_ptr<ui::GestureRecognizer> gesture_recognizer_;

  bool synthesize_mouse_move_;
  bool waiting_on_compositing_end_;
  bool draw_on_compositing_end_;

  bool defer_draw_scheduling_;

  // How many holds are outstanding. We try to defer dispatching mouse moves
  // while the count is > 0.
  int mouse_move_hold_count_;
  scoped_ptr<ui::MouseEvent> held_mouse_move_;
  // Used to schedule DispatchHeldMouseMove() when |mouse_move_hold_count_| goes
  // to 0.
  base::WeakPtrFactory<RootWindow> held_mouse_event_factory_;

  CompositorLock* compositor_lock_;
  bool draw_on_compositor_unlock_;

  DISALLOW_COPY_AND_ASSIGN(RootWindow);
};

}  // namespace aura

#endif  // UI_AURA_ROOT_WINDOW_H_
