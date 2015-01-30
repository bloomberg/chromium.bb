// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_FACTORY_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_FACTORY_EVDEV_H_

#include <map>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"

namespace ui {

class CursorDelegateEvdev;
class DeviceEventDispatcherEvdev;

#if !defined(USE_EVDEV)
#error Missing dependency on ui/events/ozone:events_ozone_evdev
#endif

#if defined(USE_EVDEV_GESTURES)
class GesturePropertyProvider;
#endif

typedef base::Callback<void(scoped_ptr<std::string>)> GetTouchDeviceStatusReply;

// Manager for event device objects. All device I/O starts here.
class EVENTS_OZONE_EVDEV_EXPORT InputDeviceFactoryEvdev {
 public:
  InputDeviceFactoryEvdev(
      DeviceEventDispatcherEvdev* dispatcher,
      scoped_refptr<base::SingleThreadTaskRunner> dispatch_runner,
      CursorDelegateEvdev* cursor);
  ~InputDeviceFactoryEvdev();

  // Open & start reading a newly plugged-in input device.
  void AddInputDevice(int id, const base::FilePath& path);

  // Stop reading & close an unplugged input device.
  void RemoveInputDevice(const base::FilePath& path);

  // Disables the internal touchpad.
  void DisableInternalTouchpad();

  // Enables the internal touchpad.
  void EnableInternalTouchpad();

  // Disables all keys on the internal keyboard except |excepted_keys|.
  void DisableInternalKeyboardExceptKeys(
      scoped_ptr<std::set<DomCode>> excepted_keys);

  // Enables all keys on the internal keyboard.
  void EnableInternalKeyboard();

  // Bits from InputController that have to be answered on IO.
  void SetTouchpadSensitivity(int value);
  void SetTapToClick(bool enabled);
  void SetThreeFingerClick(bool enabled);
  void SetTapDragging(bool enabled);
  void SetNaturalScroll(bool enabled);
  void SetMouseSensitivity(int value);
  void SetTapToClickPaused(bool state);
  void GetTouchDeviceStatus(const GetTouchDeviceStatusReply& reply);

 private:
  // Open device at path & starting processing events (on UI thread).
  void AttachInputDevice(scoped_ptr<EventConverterEvdev> converter);

  // Close device at path (on UI thread).
  void DetachInputDevice(const base::FilePath& file_path);

  // Update observers on device changes.
  void NotifyDeviceChange(const EventConverterEvdev& converter);
  void NotifyKeyboardsUpdated();
  void NotifyTouchscreensUpdated();
  void NotifyMouseDevicesUpdated();
  void NotifyTouchpadDevicesUpdated();

  void SetIntPropertyForOneType(const EventDeviceType type,
                                const std::string& name,
                                int value);
  void SetBoolPropertyForOneType(const EventDeviceType type,
                                 const std::string& name,
                                 bool value);

  // Owned per-device event converters (by path).
  std::map<base::FilePath, EventConverterEvdev*> converters_;

  // Task runner for event dispatch.
  scoped_refptr<base::TaskRunner> ui_task_runner_;

  // Cursor movement.
  CursorDelegateEvdev* cursor_;

#if defined(USE_EVDEV_GESTURES)
  // Gesture library property provider (used by touchpads/mice).
  scoped_ptr<GesturePropertyProvider> gesture_property_provider_;
#endif

  // Dispatcher for events.
  DeviceEventDispatcherEvdev* dispatcher_;

  // Support weak pointers for attach & detach callbacks.
  base::WeakPtrFactory<InputDeviceFactoryEvdev> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InputDeviceFactoryEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_INPUT_DEVICE_FACTORY_EVDEV_H_
