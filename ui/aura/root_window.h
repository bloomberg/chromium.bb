// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ROOT_WINDOW_H_
#define UI_AURA_ROOT_WINDOW_H_

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
#include "ui/gfx/transform.h"

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
class RootWindow;
class RootWindowHost;
class RootWindowObserver;
class RootWindowTransformer;
class TestScreen;
class WindowTargeter;

// RootWindow is responsible for hosting a set of windows.
class AURA_EXPORT RootWindow : public ui::EventProcessor,
                               public ui::GestureEventHelper,
                               public ui::LayerAnimationObserver,
                               public aura::client::CaptureDelegate,
                               public aura::RootWindowHostDelegate {
 public:
  struct AURA_EXPORT CreateParams {
    // CreateParams with initial_bounds and default host in pixel.
    explicit CreateParams(const gfx::Rect& initial_bounds);
    ~CreateParams() {}

    gfx::Rect initial_bounds;

    // A host to use in place of the default one that RootWindow will create.
    // NULL by default.
    RootWindowHost* host;
  };

  explicit RootWindow(const CreateParams& params);
  virtual ~RootWindow();

  // Returns the RootWindowHost for the specified accelerated widget, or NULL
  // if there is none associated.
  static RootWindow* GetForAcceleratedWidget(gfx::AcceleratedWidget widget);

  Window* window() {
    return const_cast<Window*>(const_cast<const RootWindow*>(this)->window());
  }
  const Window* window() const { return window_.get(); }
  RootWindowHost* host() {
    return const_cast<RootWindowHost*>(
        const_cast<const RootWindow*>(this)->host());
  }
  const RootWindowHost* host() const { return host_.get(); }
  ui::Compositor* compositor() { return compositor_.get(); }
  gfx::NativeCursor last_cursor() const { return last_cursor_; }
  Window* mouse_pressed_handler() { return mouse_pressed_handler_; }
  Window* mouse_moved_handler() { return mouse_moved_handler_; }

  // Initializes the root window.
  void Init();

  // Stop listening events in preparation for shutdown.
  void PrepareForShutdown();

  // Repost event for re-processing. Used when exiting context menus.
  // We only support the ET_MOUSE_PRESSED and ET_GESTURE_TAP_DOWN event
  // types (although the latter is currently a no-op).
  void RepostEvent(const ui::LocatedEvent& event);

  RootWindowHostDelegate* AsRootWindowHostDelegate();

  // Gets/sets the size of the host window.
  void SetHostSize(const gfx::Size& size_in_pixel);

  // Sets the bounds of the host window.
  void SetHostBounds(const gfx::Rect& size_in_pizel);

  // Sets the currently-displayed cursor. If the cursor was previously hidden
  // via ShowCursor(false), it will remain hidden until ShowCursor(true) is
  // called, at which point the cursor that was last set via SetCursor() will be
  // used.
  void SetCursor(gfx::NativeCursor cursor);

  // Invoked when the cursor's visibility has changed.
  void OnCursorVisibilityChanged(bool visible);

  // Invoked when the mouse events get enabled or disabled.
  void OnMouseEventsEnableStateChanged(bool enabled);

  // Moves the cursor to the specified location relative to the root window.
  void MoveCursorTo(const gfx::Point& location);

  // Moves the cursor to the |host_location| given in host coordinates.
  void MoveCursorToHostLocation(const gfx::Point& host_location);

  // Draw the damage_rect.
  void ScheduleRedrawRect(const gfx::Rect& damage_rect);

  // Returns a target window for the given gesture event.
  Window* GetGestureTarget(ui::GestureEvent* event);

  // Handles a gesture event. Returns true if handled. Unlike the other
  // event-dispatching function (e.g. for touch/mouse/keyboard events), gesture
  // events are dispatched from GestureRecognizer instead of RootWindowHost.
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
  void OnRootWindowHostCloseRequested();

  // Add/remove observer. There is no need to remove the observer if
  // the root window is being deleted. In particular, you SHOULD NOT remove
  // in |WindowObserver::OnWindowDestroying| of the observer observing
  // the root window because it is too late to remove it.
  void AddRootWindowObserver(RootWindowObserver* observer);
  void RemoveRootWindowObserver(RootWindowObserver* observer);

  // Converts |point| from the root window's coordinate system to the
  // host window's.
  void ConvertPointToHost(gfx::Point* point) const;

  // Converts |point| from the host window's coordinate system to the
  // root window's.
  void ConvertPointFromHost(gfx::Point* point) const;

  // Gesture Recognition -------------------------------------------------------

  // When a touch event is dispatched to a Window, it may want to process the
  // touch event asynchronously. In such cases, the window should consume the
  // event during the event dispatch. Once the event is properly processed, the
  // window should let the RootWindow know about the result of the event
  // processing, so that gesture events can be properly created and dispatched.
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

  void SetRootWindowTransformer(scoped_ptr<RootWindowTransformer> transformer);
  gfx::Transform GetRootTransform() const;

  void SetTransform(const gfx::Transform& transform);

 private:
  FRIEND_TEST_ALL_PREFIXES(RootWindowTest, KeepTranslatedEventInRoot);

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
  // factor. The RootWindowHostDelegate dispatches events in the physical pixel
  // coordinate. But the event processing from RootWindow onwards happen in
  // device-independent pixel coordinate. So it is necessary to update the event
  // received from the host.
  void TransformEventForDeviceScaleFactor(ui::LocatedEvent* event);

  // Moves the cursor to the specified location. This method is internally used
  // by MoveCursorTo() and MoveCursorToHostLocation().
  void MoveCursorToInternal(const gfx::Point& root_location,
                            const gfx::Point& host_location);

  // Dispatches the specified event type (intended for enter/exit) to the
  // |mouse_moved_handler_|.
  ui::EventDispatchDetails DispatchMouseEnterOrExit(
      const ui::MouseEvent& event,
      ui::EventType type) WARN_UNUSED_RESULT;
  ui::EventDispatchDetails ProcessGestures(
      ui::GestureRecognizer::Gestures* gestures) WARN_UNUSED_RESULT;

  // Called when a Window is attached or detached from the RootWindow.
  void OnWindowAddedToRootWindow(Window* window);
  void OnWindowRemovedFromRootWindow(Window* window, Window* new_root);

  // Called when a window becomes invisible, either by being removed
  // from root window hierarchy, via SetVisible(false) or being destroyed.
  // |reason| specifies what triggered the hiding.
  void OnWindowHidden(Window* invisible, WindowHiddenReason reason);

  // Cleans up the state of gestures for all windows in |window| (including
  // |window| itself). This includes cancelling active touch points.
  void CleanupGestureState(Window* window);

  // Updates the root window's size using |host_size|, current
  // transform and insets.
  void UpdateRootWindowSize(const gfx::Size& host_size);

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

  // Overridden from aura::RootWindowHostDelegate:
  virtual bool OnHostKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual bool OnHostMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual bool OnHostScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual bool OnHostTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual void OnHostCancelMode() OVERRIDE;
  virtual void OnHostActivated() OVERRIDE;
  virtual void OnHostLostWindowCapture() OVERRIDE;
  virtual void OnHostLostMouseGrab() OVERRIDE;
  virtual void OnHostPaint(const gfx::Rect& damage_rect) OVERRIDE;
  virtual void OnHostMoved(const gfx::Point& origin) OVERRIDE;
  virtual void OnHostResized(const gfx::Size& size) OVERRIDE;
  virtual float GetDeviceScaleFactor() OVERRIDE;
  virtual RootWindow* AsRootWindow() OVERRIDE;
  virtual const RootWindow* AsRootWindow() const OVERRIDE;
  virtual ui::EventProcessor* GetEventProcessor() OVERRIDE;

  ui::EventDispatchDetails OnHostMouseEventImpl(ui::MouseEvent* event)
      WARN_UNUSED_RESULT;

  // We hold and aggregate mouse drags and touch moves as a way of throttling
  // resizes when HoldMouseMoves() is called. The following methods are used to
  // dispatch held and newly incoming mouse and touch events, typically when an
  // event other than one of these needs dispatching or a matching
  // ReleaseMouseMoves()/ReleaseTouchMoves() is called.  NOTE: because these
  // methods dispatch events from RootWindowHost the coordinates are in terms of
  // the root.
  ui::EventDispatchDetails DispatchMouseEventImpl(ui::MouseEvent* event)
      WARN_UNUSED_RESULT;
  ui::EventDispatchDetails DispatchMouseEventRepost(ui::MouseEvent* event)
      WARN_UNUSED_RESULT;
  ui::EventDispatchDetails DispatchMouseEventToTarget(ui::MouseEvent* event,
                                                      Window* target)
      WARN_UNUSED_RESULT;
  ui::EventDispatchDetails DispatchTouchEventImpl(ui::TouchEvent* event)
      WARN_UNUSED_RESULT;
  ui::EventDispatchDetails DispatchHeldEvents() WARN_UNUSED_RESULT;
  // Creates and dispatches synthesized mouse move event using the
  // current mouse location.
  ui::EventDispatchDetails SynthesizeMouseMoveEvent() WARN_UNUSED_RESULT;

  void SynthesizeMouseMoveEventAsync();

  // Posts a task to send synthesized mouse move event if there
  // is no a pending task.
  void PostMouseMoveEventAfterWindowChange();

  gfx::Transform GetInverseRootTransform() const;

  void PreDispatchLocatedEvent(Window* target, ui::LocatedEvent* event);

  // TODO(beng): evaluate the ideal ownership model.
  scoped_ptr<Window> window_;

  scoped_ptr<ui::Compositor> compositor_;

  scoped_ptr<RootWindowHost> host_;

  // Touch ids that are currently down.
  uint32 touch_ids_down_;

  // Last cursor set.  Used for testing.
  gfx::NativeCursor last_cursor_;

  ObserverList<RootWindowObserver> observers_;

  Window* mouse_pressed_handler_;
  Window* mouse_moved_handler_;
  Window* event_dispatch_target_;
  Window* old_dispatch_target_;

  bool synthesize_mouse_move_;
  bool waiting_on_compositing_end_;
  bool draw_on_compositing_end_;

  bool defer_draw_scheduling_;

  // How many move holds are outstanding. We try to defer dispatching
  // touch/mouse moves while the count is > 0.
  int move_hold_count_;
  scoped_ptr<ui::LocatedEvent> held_move_event_;

  // Allowing for reposting of events. Used when exiting context menus.
  scoped_ptr<ui::LocatedEvent>  held_repostable_event_;

  // Set when dispatching a held event.
  bool dispatching_held_event_;

  scoped_ptr<ui::ViewProp> prop_;

  scoped_ptr<RootWindowTransformer> transformer_;

  // Used to schedule reposting an event.
  base::WeakPtrFactory<RootWindow> repost_event_factory_;

  // Used to schedule DispatchHeldEvents() when |move_hold_count_| goes to 0.
  base::WeakPtrFactory<RootWindow> held_event_factory_;

  DISALLOW_COPY_AND_ASSIGN(RootWindow);
};

}  // namespace aura

#endif  // UI_AURA_ROOT_WINDOW_H_
