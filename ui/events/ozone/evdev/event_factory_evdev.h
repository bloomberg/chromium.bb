// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_FACTORY_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_FACTORY_EVDEV_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/event_modifiers_evdev.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/events/ozone/evdev/input_controller_evdev.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"
#include "ui/events/ozone/evdev/mouse_button_map_evdev.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/system_input_injector.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace ui {

class CursorDelegateEvdev;
class DeviceManager;
class InputDeviceFactoryEvdev;
class SystemInputInjector;
enum class DomCode;

#if !defined(USE_EVDEV)
#error Missing dependency on ui/events/ozone:events_ozone_evdev
#endif

// Ozone events implementation for the Linux input subsystem ("evdev").
class EVENTS_OZONE_EVDEV_EXPORT EventFactoryEvdev
    : public DeviceEventObserver,
      public PlatformEventSource,
      public DeviceEventDispatcherEvdev {
 public:
  EventFactoryEvdev(CursorDelegateEvdev* cursor,
                    DeviceManager* device_manager,
                    KeyboardLayoutEngine* keyboard_layout_engine);
  ~EventFactoryEvdev() override;

  // Initialize. Must be called with a valid message loop.
  void Init();

  void WarpCursorTo(gfx::AcceleratedWidget widget,
                    const gfx::PointF& location);

  scoped_ptr<SystemInputInjector> CreateSystemInputInjector();

  InputController* input_controller() { return &input_controller_; }

  // DeviceEventDispatchEvdev:
  void DispatchKeyEvent(const KeyEventParams& params) override;
  void DispatchMouseMoveEvent(const MouseMoveEventParams& params) override;
  void DispatchMouseButtonEvent(const MouseButtonEventParams& params) override;
  void DispatchMouseWheelEvent(const MouseWheelEventParams& params) override;
  void DispatchScrollEvent(const ScrollEventParams& params) override;
  void DispatchTouchEvent(const TouchEventParams& params) override;

 protected:
  // DeviceEventObserver overrides:
  //
  // Callback for device add (on UI thread).
  void OnDeviceEvent(const DeviceEvent& event) override;

  // PlatformEventSource:
  void OnDispatcherListChanged() override;

 private:
  // Post a task to dispatch an event.
  virtual void PostUiEvent(scoped_ptr<Event> event);

  // Dispatch event via PlatformEventSource.
  void DispatchUiEventTask(scoped_ptr<Event> event);

  int NextDeviceId();

  // Used to uniquely identify input devices.
  int last_device_id_;

  // Interface for scanning & monitoring input devices.
  DeviceManager* device_manager_;  // Not owned.

  // Factory for per-device objects.
  scoped_ptr<InputDeviceFactoryEvdev> input_device_factory_;

  // Dispatch callback for events.
  EventDispatchCallback dispatch_callback_;

  // Modifier key state (shift, ctrl, etc).
  EventModifiersEvdev modifiers_;

  // Mouse button map.
  MouseButtonMapEvdev button_map_;

  // Keyboard state.
  KeyboardEvdev keyboard_;

  // Cursor movement.
  CursorDelegateEvdev* cursor_;

  // Object for controlling input devices.
  InputControllerEvdev input_controller_;

  // Whether we've set up the device factory & done initial scan.
  bool initialized_;

  // Support weak pointers for attach & detach callbacks.
  base::WeakPtrFactory<EventFactoryEvdev> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EventFactoryEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_FACTORY_EVDEV_H_
