// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_EVENT_DISPATCHER_H_
#define UI_AURA_WINDOW_EVENT_DISPATCHER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/client/capture_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_delegate.h"
#include "ui/base/cursor/cursor.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_targeter.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"

namespace gfx {
class Size;
class Transform;
}

namespace ui {
class GestureEvent;
class GestureRecognizer;
class KeyEvent;
class LayerAnimationSequence;
class MouseEvent;
class ScrollEvent;
class TouchEvent;
class ViewProp;
}

namespace aura {
class WindowTreeHost;
class RootWindowObserver;
class TestScreen;
class WindowTargeter;

// RootWindow is responsible for hosting a set of windows.
class AURA_EXPORT WindowEventDispatcher : public ui::EventProcessor,
                                          public ui::GestureEventHelper,
                                          public ui::LayerAnimationObserver,
                                          public client::CaptureDelegate,
                                          public WindowTreeHostDelegate {
 public:
  struct AURA_EXPORT CreateParams {
    // CreateParams with initial_bounds and default host in pixel.
    explicit CreateParams(const gfx::Rect& initial_bounds);
    ~CreateParams() {}

    gfx::Rect initial_bounds;

    // A host to use in place of the default one that RootWindow will create.
    // NULL by default.
    WindowTreeHost* host;
  };

  explicit WindowEventDispatcher(const CreateParams& params);
  virtual ~WindowEventDispatcher();

  // Returns the WindowTreeHost for the specified accelerated widget, or NULL
  // if there is none associated.
  static WindowEventDispatcher* GetForAcceleratedWidget(
      gfx::AcceleratedWidget widget);

  Window* window() {
    return const_cast<Window*>(
        const_cast<const WindowEventDispatcher*>(this)->window());
  }
  const Window* window() const { return window_.get(); }
  WindowTreeHost* host() {
    return const_cast<WindowTreeHost*>(
        const_cast<const WindowEventDispatcher*>(this)->host());
  }
  const WindowTreeHost* host() const { return host_.get(); }
  Window* mouse_pressed_handler() { return mouse_pressed_handler_; }
  Window* mouse_moved_handler() { return mouse_moved_handler_; }

  // Stop listening events in preparation for shutdown.
  void PrepareForShutdown();

  // Repost event for re-processing. Used when exiting context menus.
  // We only support the ET_MOUSE_PRESSED and ET_GESTURE_TAP_DOWN event
  // types (although the latter is currently a no-op).
  void RepostEvent(const ui::LocatedEvent& event);

  WindowTreeHostDelegate* AsWindowTreeHostDelegate();

  // Invoked when the mouse events get enabled or disabled.
  void OnMouseEventsEnableStateChanged(bool enabled);

  // Returns a target window for the given gesture event.
  Window* GetGestureTarget(ui::GestureEvent* event);

  // Handles a gesture event. Returns true if handled. Unlike the other
  // event-dispatching function (e.g. for touch/mouse/keyboard events), gesture
  // events are dispatched from GestureRecognizer instead of WindowTreeHost.
  void DispatchGestureEvent(ui::GestureEvent* event);

  // Invoked when |window| is being destroyed.
  void OnWindowDestroying(Window* window);

  // Invoked when |window|'s bounds have changed. |contained_mouse| indicates if
  // the bounds before change contained the |last_moust_location()|.
  void OnWindowBoundsChanged(Window* window, bool contained_mouse);

  // Dispatches OnMouseExited to the |window| which is hiding if nessessary.
  void DispatchMouseExitToHidingWindow(Window* window);

  // Dispatches a ui::ET_MOUSE_EXITED event at |point|.
  void DispatchMouseExitAtPoint(const gfx::Point& point);

  // Invoked when |window|'s visibility has changed.
  void OnWindowVisibilityChanged(Window* window, bool is_visible);

  // Invoked when |window|'s tranfrom has changed. |contained_mouse|
  // indicates if the bounds before change contained the
  // |last_moust_location()|.
  void OnWindowTransformed(Window* window, bool contained_mouse);

  // Invoked when the keyboard mapping (in X11 terms: the mapping between
  // keycodes and keysyms) has changed.
  void OnKeyboardMappingChanged();

  // The system windowing system has sent a request that we close our window.
  void OnWindowTreeHostCloseRequested();

  // Add/remove observer. There is no need to remove the observer if
  // the root window is being deleted. In particular, you SHOULD NOT remove
  // in |WindowObserver::OnWindowDestroying| of the observer observing
  // the root window because it is too late to remove it.
  void AddRootWindowObserver(RootWindowObserver* observer);
  void RemoveRootWindowObserver(RootWindowObserver* observer);

  // Gesture Recognition -------------------------------------------------------

  // When a touch event is dispatched to a Window, it may want to process the
  // touch event asynchronously. In such cases, the window should consume the
  // event during the event dispatch. Once the event is properly processed, the
  // window should let the WindowEventDispatcher know about the result of the
  // event processing, so that gesture events can be properly created and
  // dispatched.
  void ProcessedTouchEvent(ui::TouchEvent* event,
                           Window* window,
                           ui::EventResult result);

  // These methods are used to defer the processing of mouse/touch events
  // related to resize. A client (typically a RenderWidgetHostViewAura) can call
  // HoldPointerMoves when an resize is initiated and then ReleasePointerMoves
  // once the resize is completed.
  //
  // More than one hold can be invoked and each hold must be cancelled by a
  // release before we resume normal operation.
  void HoldPointerMoves();
  void ReleasePointerMoves();

  // Gets the last location seen in a mouse event in this root window's
  // coordinates. This may return a point outside the root window's bounds.
  gfx::Point GetLastMouseLocationInRoot() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(WindowEventDispatcherTest,
                           KeepTranslatedEventInRoot);

  friend class Window;
  friend class TestScreen;

  // The parameter for OnWindowHidden() to specify why window is hidden.
  enum WindowHiddenReason {
    WINDOW_DESTROYED,  // Window is destroyed.
    WINDOW_HIDDEN,     // Window is hidden.
    WINDOW_MOVING,     // Window is temporarily marked as hidden due to move
                       // across root windows.
  };

  // Updates the event with the appropriate transform for the device scale
  // factor. The WindowTreeHostDelegate dispatches events in the physical pixel
  // coordinate. But the event processing from WindowEventDispatcher onwards
  // happen in device-independent pixel coordinate. So it is necessary to update
  // the event received from the host.
  void TransformEventForDeviceScaleFactor(ui::LocatedEvent* event);

  // Dispatches the specified event type (intended for enter/exit) to the
  // |mouse_moved_handler_|.
  ui::EventDispatchDetails DispatchMouseEnterOrExit(
      const ui::MouseEvent& event,
      ui::EventType type) WARN_UNUSED_RESULT;
  ui::EventDispatchDetails ProcessGestures(
      ui::GestureRecognizer::Gestures* gestures) WARN_UNUSED_RESULT;

  // Called when a Window is attached or detached from the
  // WindowEventDispatcher.
  void OnWindowAddedToRootWindow(Window* window);
  void OnWindowRemovedFromRootWindow(Window* window, Window* new_root);

  // Called when a window becomes invisible, either by being removed
  // from root window hierarchy, via SetVisible(false) or being destroyed.
  // |reason| specifies what triggered the hiding. Note that becoming invisible
  // will cause a window to lose capture and some windows may destroy themselves
  // on capture (like DragDropTracker).
  void OnWindowHidden(Window* invisible, WindowHiddenReason reason);

  // Cleans up the state of gestures for all windows in |window| (including
  // |window| itself). This includes cancelling active touch points.
  void CleanupGestureState(Window* window);

  // Overridden from aura::client::CaptureDelegate:
  virtual void UpdateCapture(Window* old_capture, Window* new_capture) OVERRIDE;
  virtual void OnOtherRootGotCapture() OVERRIDE;
  virtual void SetNativeCapture() OVERRIDE;
  virtual void ReleaseNativeCapture() OVERRIDE;

  // Overridden from ui::EventProcessor:
  virtual ui::EventTarget* GetRootTarget() OVERRIDE;
  virtual void PrepareEventForDispatch(ui::Event* event) OVERRIDE;

  // Overridden from ui::EventDispatcherDelegate.
  virtual bool CanDispatchToTarget(ui::EventTarget* target) OVERRIDE;
  virtual ui::EventDispatchDetails PreDispatchEvent(ui::EventTarget* target,
                                                    ui::Event* event) OVERRIDE;
  virtual ui::EventDispatchDetails PostDispatchEvent(
      ui::EventTarget* target, const ui::Event& event) OVERRIDE;

  // Overridden from ui::GestureEventHelper.
  virtual bool CanDispatchToConsumer(ui::GestureConsumer* consumer) OVERRIDE;
  virtual void DispatchPostponedGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual void DispatchCancelTouchEvent(ui::TouchEvent* event) OVERRIDE;

  // Overridden from ui::LayerAnimationObserver:
  virtual void OnLayerAnimationEnded(
      ui::LayerAnimationSequence* animation) OVERRIDE;
  virtual void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* animation) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      ui::LayerAnimationSequence* animation) OVERRIDE;

  // Overridden from aura::WindowTreeHostDelegate:
  virtual void OnHostCancelMode() OVERRIDE;
  virtual void OnHostActivated() OVERRIDE;
  virtual void OnHostLostWindowCapture() OVERRIDE;
  virtual void OnHostLostMouseGrab() OVERRIDE;
  virtual void OnHostMoved(const gfx::Point& origin) OVERRIDE;
  virtual void OnHostResized(const gfx::Size& size) OVERRIDE;
  virtual void OnCursorMovedToRootLocation(
      const gfx::Point& root_location) OVERRIDE;
  virtual WindowEventDispatcher* AsDispatcher() OVERRIDE;
  virtual const WindowEventDispatcher* AsDispatcher() const OVERRIDE;
  virtual ui::EventProcessor* GetEventProcessor() OVERRIDE;

  // We hold and aggregate mouse drags and touch moves as a way of throttling
  // resizes when HoldMouseMoves() is called. The following methods are used to
  // dispatch held and newly incoming mouse and touch events, typically when an
  // event other than one of these needs dispatching or a matching
  // ReleaseMouseMoves()/ReleaseTouchMoves() is called.  NOTE: because these
  // methods dispatch events from WindowTreeHost the coordinates are in terms of
  // the root.

  ui::EventDispatchDetails DispatchHeldEvents() WARN_UNUSED_RESULT;
  // Creates and dispatches synthesized mouse move event using the
  // current mouse location.
  ui::EventDispatchDetails SynthesizeMouseMoveEvent() WARN_UNUSED_RESULT;

  void SynthesizeMouseMoveEventAsync();

  // Posts a task to send synthesized mouse move event if there
  // is no a pending task.
  void PostMouseMoveEventAfterWindowChange();

  void PreDispatchLocatedEvent(Window* target, ui::LocatedEvent* event);
  void PreDispatchMouseEvent(Window* target, ui::MouseEvent* event);
  void PreDispatchTouchEvent(Window* target, ui::TouchEvent* event);

  // TODO(beng): evaluate the ideal ownership model.
  scoped_ptr<Window> window_;

  scoped_ptr<WindowTreeHost> host_;

  // Touch ids that are currently down.
  uint32 touch_ids_down_;

  ObserverList<RootWindowObserver> observers_;

  Window* mouse_pressed_handler_;
  Window* mouse_moved_handler_;
  Window* event_dispatch_target_;
  Window* old_dispatch_target_;

  bool synthesize_mouse_move_;

  // How many move holds are outstanding. We try to defer dispatching
  // touch/mouse moves while the count is > 0.
  int move_hold_count_;
  // The location of |held_move_event_| is in |window_|'s coordinate.
  scoped_ptr<ui::LocatedEvent> held_move_event_;

  // Allowing for reposting of events. Used when exiting context menus.
  scoped_ptr<ui::LocatedEvent> held_repostable_event_;

  // Set when dispatching a held event.
  bool dispatching_held_event_;

  scoped_ptr<ui::ViewProp> prop_;

  // Used to schedule reposting an event.
  base::WeakPtrFactory<WindowEventDispatcher> repost_event_factory_;

  // Used to schedule DispatchHeldEvents() when |move_hold_count_| goes to 0.
  base::WeakPtrFactory<WindowEventDispatcher> held_event_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowEventDispatcher);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_EVENT_DISPATCHER_H_
