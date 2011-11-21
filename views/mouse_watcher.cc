// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/mouse_watcher.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/event_types.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "ui/base/events.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"
#include "views/view.h"

namespace views {

// Amount of time between when the mouse moves outside the view's zone and when
// the listener is notified.
const int kNotifyListenerTimeMs = 300;

class MouseWatcher::Observer : public MessageLoopForUI::Observer {
 public:
  explicit Observer(MouseWatcher* mouse_watcher)
      : mouse_watcher_(mouse_watcher),
        ALLOW_THIS_IN_INITIALIZER_LIST(notify_listener_factory_(this)) {
    MessageLoopForUI::current()->AddObserver(this);
  }

  ~Observer() {
    MessageLoopForUI::current()->RemoveObserver(this);
  }

  // MessageLoop::Observer implementation:
#if defined(OS_WIN)
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE {
    return base::EVENT_CONTINUE;
  }

  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE {
    // We spy on three different Windows messages here to see if the mouse has
    // moved out of the bounds of the view. The messages are:
    //
    // WM_MOUSEMOVE:
    //   For when the mouse moves from the view into the rest of the browser UI,
    //   i.e. within the bounds of the same windows HWND.
    // WM_MOUSELEAVE:
    //   For when the mouse moves out of the bounds of the view's HWND.
    // WM_NCMOUSELEAVE:
    //   For notification when the mouse leaves the _non-client_ area.
    //
    switch (event.message) {
      case WM_MOUSEMOVE:
        HandleGlobalMouseMoveEvent(false);
        break;
      case WM_MOUSELEAVE:
      case WM_NCMOUSELEAVE:
        HandleGlobalMouseMoveEvent(true);
        break;
    }
  }
#elif defined(USE_WAYLAND)
  virtual MessageLoopForUI::Observer::EventStatus WillProcessEvent(
      base::wayland::WaylandEvent* event) OVERRIDE {
    switch (event->type) {
      case base::wayland::WAYLAND_MOTION:
        HandleGlobalMouseMoveEvent(false);
        break;
      case base::wayland::WAYLAND_POINTER_FOCUS:
        if (!event->pointer_focus.state)
          HandleGlobalMouseMoveEvent(true);
        break;
      default:
        break;
    }
    return EVENT_CONTINUE;
  }
#elif defined(TOUCH_UI) || defined(USE_AURA)
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE {
    return base::EVENT_CONTINUE;
  }
  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE {
    switch (ui::EventTypeFromNative(event)) {
      case ui::ET_MOUSE_MOVED:
      case ui::ET_MOUSE_DRAGGED:
        // DRAGGED is a special case of MOVED. See events_win.cc/events_x.cc.
        HandleGlobalMouseMoveEvent(false);
        break;
      case ui::ET_MOUSE_EXITED:
        HandleGlobalMouseMoveEvent(true);
        break;
      default:
        break;
    }
  }
#elif defined(TOOLKIT_USES_GTK)
  virtual void WillProcessEvent(GdkEvent* event) OVERRIDE {
  }

  virtual void DidProcessEvent(GdkEvent* event) OVERRIDE {
    switch (event->type) {
      case GDK_MOTION_NOTIFY:
        HandleGlobalMouseMoveEvent(false);
        break;
      case GDK_LEAVE_NOTIFY:
        HandleGlobalMouseMoveEvent(true);
        break;
      default:
        break;
    }
  }
#endif

 private:
  View* view() const { return mouse_watcher_->host_; }

  // Returns whether or not the cursor is currently in the view's "zone" which
  // is defined as a slightly larger region than the view.
  bool IsCursorInViewZone() {
    gfx::Rect bounds = view()->GetLocalBounds();
    gfx::Point view_topleft(bounds.origin());
    View::ConvertPointToScreen(view(), &view_topleft);
    bounds.set_origin(view_topleft);
    bounds.SetRect(view_topleft.x() - mouse_watcher_->hot_zone_insets_.left(),
                   view_topleft.y() - mouse_watcher_->hot_zone_insets_.top(),
                   bounds.width() + mouse_watcher_->hot_zone_insets_.width(),
                   bounds.height() + mouse_watcher_->hot_zone_insets_.height());

    gfx::Point cursor_point = gfx::Screen::GetCursorScreenPoint();

    return bounds.Contains(cursor_point.x(), cursor_point.y());
  }

  // Returns true if the mouse is over the view's window.
  bool IsMouseOverWindow() {
    Widget* widget = view()->GetWidget();
    if (!widget)
      return false;

    return gfx::Screen::GetWindowAtCursorScreenPoint() ==
        widget->GetNativeWindow();
  }

  // Called from the message loop observer when a mouse movement has occurred.
  void HandleGlobalMouseMoveEvent(bool check_window) {
    bool in_view = IsCursorInViewZone();
    if (!in_view || (check_window && !IsMouseOverWindow())) {
      // Mouse moved outside the view's zone, start a timer to notify the
      // listener.
      if (!notify_listener_factory_.HasWeakPtrs()) {
        MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            base::Bind(&Observer::NotifyListener,
                       notify_listener_factory_.GetWeakPtr()),
            !in_view ? kNotifyListenerTimeMs :
                       mouse_watcher_->notify_on_exit_time_ms_);
      }
    } else {
      // Mouse moved quickly out of the view and then into it again, so cancel
      // the timer.
      notify_listener_factory_.InvalidateWeakPtrs();
    }
  }

  void NotifyListener() {
    mouse_watcher_->NotifyListener();
    // WARNING: we've been deleted.
  }

 private:
  MouseWatcher* mouse_watcher_;

  // A factory that is used to construct a delayed callback to the listener.
  base::WeakPtrFactory<Observer> notify_listener_factory_;

  DISALLOW_COPY_AND_ASSIGN(Observer);
};

MouseWatcherListener::~MouseWatcherListener() {
}

MouseWatcher::MouseWatcher(View* host,
                           MouseWatcherListener* listener,
                           const gfx::Insets& hot_zone_insets)
    : host_(host),
      listener_(listener),
      hot_zone_insets_(hot_zone_insets),
      notify_on_exit_time_ms_(kNotifyListenerTimeMs) {
}

MouseWatcher::~MouseWatcher() {
}

void MouseWatcher::Start() {
  if (!is_observing())
    observer_.reset(new Observer(this));
}

void MouseWatcher::Stop() {
  observer_.reset(NULL);
}

void MouseWatcher::NotifyListener() {
  observer_.reset(NULL);
  listener_->MouseMovedOutOfView();
}

}  // namespace views
