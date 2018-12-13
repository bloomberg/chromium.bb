// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_INPUT_DEVICE_OBSERVER_ANDROID_H_
#define UI_EVENTS_DEVICES_INPUT_DEVICE_OBSERVER_ANDROID_H_

#include <jni.h>

#include "base/observer_list.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ui {

// This class is a singleton responsible to notify the
// InputDeviceChangeObserver whenever an input change
// happened on the Java side.
class EVENTS_DEVICES_EXPORT InputDeviceObserverAndroid {
 public:
  static InputDeviceObserverAndroid* GetInstance();
  ~InputDeviceObserverAndroid();

  void AddObserver(ui::InputDeviceEventObserver* observer);
  void RemoveObserver(ui::InputDeviceEventObserver* observer);

  void NotifyObserversDeviceConfigurationChanged();

 private:
  InputDeviceObserverAndroid();

  base::ObserverList<ui::InputDeviceEventObserver>::Unchecked observers_;

  friend struct base::DefaultSingletonTraits<InputDeviceObserverAndroid>;
  DISALLOW_COPY_AND_ASSIGN(InputDeviceObserverAndroid);
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_INPUT_DEVICE_OBSERVER_ANDROID_H_
