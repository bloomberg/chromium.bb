// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ENV_H_
#define UI_AURA_ENV_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/observer_list.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/client/stacking_client.h"
#include "ui/gfx/point.h"

#if defined(USE_X11)
#include "ui/aura/device_list_updater_aurax11.h"
#endif

namespace aura {
class EnvObserver;
class EventFilter;
class DisplayManager;
class Window;

namespace internal {
class DisplayChangeObserverX11;
}

#if !defined(USE_X11)
// Creates a platform-specific native event dispatcher.
MessageLoop::Dispatcher* CreateDispatcher();
#endif

// A singleton object that tracks general state within Aura.
// TODO(beng): manage RootWindows.
class AURA_EXPORT Env {
 public:
  Env();
  ~Env();

  static Env* GetInstance();
  static void DeleteInstance();

  void AddObserver(EnvObserver* observer);
  void RemoveObserver(EnvObserver* observer);

  bool is_mouse_button_down() const { return mouse_button_flags_ != 0; }
  void set_mouse_button_flags(int mouse_button_flags) {
    mouse_button_flags_ = mouse_button_flags;
  }

  // Gets/sets the last mouse location seen in a mouse event in the screen
  // coordinates.
  const gfx::Point& last_mouse_location() const { return last_mouse_location_; }
  void SetLastMouseLocation(const Window& window,
                            const gfx::Point& location_in_root);

  // If |cursor_shown| is false, sets the last_mouse_position to an invalid
  // location. If |cursor_shown| is true, restores the last_mouse_position.
  void SetCursorShown(bool cursor_shown);

  // Whether any touch device is currently down.
  bool is_touch_down() const { return is_touch_down_; }
  void set_touch_down(bool value) { is_touch_down_ = value; }

  // Whether RenderWidgetHostViewAura::OnPaint() should paint white background
  // when backing store is not present. Default is true.
  // In some cases when page is using transparent background painting white
  // background before backing store is initialized causes a white splash.
  bool render_white_bg() const { return render_white_bg_; }
  void set_render_white_bg(bool value) { render_white_bg_ = value; }

  client::StackingClient* stacking_client() { return stacking_client_; }
  void set_stacking_client(client::StackingClient* stacking_client) {
    stacking_client_ = stacking_client;
  }

  // Gets/sets DisplayManager. The DisplayManager's ownership is
  // transfered.
  DisplayManager* display_manager() { return display_manager_.get(); }
  void SetDisplayManager(DisplayManager* display_manager);

  // Env takes ownership of the EventFilter.
  EventFilter* event_filter() { return event_filter_.get(); }
  void SetEventFilter(EventFilter* event_filter);

  // Returns the native event dispatcher. The result should only be passed to
  // base::RunLoop(dispatcher), or used to dispatch an event by
  // |Dispatch(const NativeEvent&)| on it. It must never be stored.
#if !defined(OS_MACOSX)
  MessageLoop::Dispatcher* GetDispatcher();
#endif

 private:
  friend class Window;

  void Init();

  // Called by the Window when it is initialized. Notifies observers.
  void NotifyWindowInitialized(Window* window);

  ObserverList<EnvObserver> observers_;
#if !defined(USE_X11)
  scoped_ptr<MessageLoop::Dispatcher> dispatcher_;
#endif

  static Env* instance_;
  int mouse_button_flags_;
  // Location of last mouse event, in screen coordinates.
  gfx::Point last_mouse_location_;
  // If the cursor is hidden, saves the previous last_mouse_position.
  gfx::Point hidden_cursor_location_;
  bool is_cursor_hidden_;
  bool is_touch_down_;
  bool render_white_bg_;
  client::StackingClient* stacking_client_;
  scoped_ptr<DisplayManager> display_manager_;
  scoped_ptr<EventFilter> event_filter_;

#if defined(USE_X11)
  scoped_ptr<internal::DisplayChangeObserverX11> display_change_observer_;
  DeviceListUpdaterAuraX11 device_list_updater_aurax11_;
#endif

  DISALLOW_COPY_AND_ASSIGN(Env);
};

}  // namespace aura

#endif  // UI_AURA_ENV_H_
