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
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_modifiers_evdev.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace ui {

class CursorDelegateEvdev;
class DeviceManager;

#if defined(USE_EVDEV_GESTURES)
class GesturePropertyProvider;
#endif

// Ozone events implementation for the Linux input subsystem ("evdev").
class EVENTS_OZONE_EVDEV_EXPORT EventFactoryEvdev : public DeviceEventObserver,
                                                    public PlatformEventSource {
 public:
  EventFactoryEvdev(CursorDelegateEvdev* cursor,
                    DeviceManager* device_manager);
  ~EventFactoryEvdev() override;

  void WarpCursorTo(gfx::AcceleratedWidget widget,
                    const gfx::PointF& location);

 protected:
  // DeviceEventObserver overrides:
  //
  // Callback for device add (on UI thread).
  void OnDeviceEvent(const DeviceEvent& event) override;

  // PlatformEventSource:
  void OnDispatcherListChanged() override;

 private:
  // Post a task to dispatch an event.
  void PostUiEvent(scoped_ptr<Event> event);

  // Dispatch event via PlatformEventSource.
  void DispatchUiEventTask(scoped_ptr<Event> event);

  // Open device at path & starting processing events (on UI thread).
  void AttachInputDevice(scoped_ptr<EventConverterEvdev> converter);

  // Close device at path (on UI thread).
  void DetachInputDevice(const base::FilePath& file_path);

  void NotifyHotplugEventObserver(const EventConverterEvdev& converter);

  int NextDeviceId();

  // Owned per-device event converters (by path).
  std::map<base::FilePath, EventConverterEvdev*> converters_;

  // Used to uniquely identify input devices.
  int last_device_id_;

  // Interface for scanning & monitoring input devices.
  DeviceManager* device_manager_;  // Not owned.

  // Task runner for event dispatch.
  scoped_refptr<base::TaskRunner> ui_task_runner_;

  // Dispatch callback for events.
  EventDispatchCallback dispatch_callback_;

  // Modifier key state (shift, ctrl, etc).
  EventModifiersEvdev modifiers_;

  // Keyboard state.
  KeyboardEvdev keyboard_;

  // Cursor movement.
  CursorDelegateEvdev* cursor_;

#if defined(USE_EVDEV_GESTURES)
  // Gesture library property provider (used by touchpads/mice).
  scoped_ptr<GesturePropertyProvider> gesture_property_provider_;
#endif

  // Support weak pointers for attach & detach callbacks.
  base::WeakPtrFactory<EventFactoryEvdev> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EventFactoryEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_FACTORY_EVDEV_H_
