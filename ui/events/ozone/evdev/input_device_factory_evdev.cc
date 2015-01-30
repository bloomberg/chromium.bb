// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_device_factory_evdev.h"

#include <fcntl.h>
#include <linux/input.h>

#include "base/debug/trace_event.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/event_converter_evdev_impl.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/tablet_event_converter_evdev.h"
#include "ui/events/ozone/evdev/touch_event_converter_evdev.h"

#if defined(USE_EVDEV_GESTURES)
#include "ui/events/ozone/evdev/libgestures_glue/event_reader_libevdev_cros.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_interpreter_libevdev_cros.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_property_provider.h"
#endif

#ifndef EVIOCSCLOCKID
#define EVIOCSCLOCKID _IOW('E', 0xa0, int)
#endif

namespace ui {

namespace {

typedef base::Callback<void(scoped_ptr<EventConverterEvdev>)>
    OpenInputDeviceReplyCallback;

struct OpenInputDeviceParams {
  // Unique identifier for the new device.
  int id;

  // Device path to open.
  base::FilePath path;

  // Dispatcher for events. Call on UI thread only.
  DeviceEventDispatcherEvdev* dispatcher;

  // State shared between devices. Must not be dereferenced on worker thread.
  CursorDelegateEvdev* cursor;
#if defined(USE_EVDEV_GESTURES)
  GesturePropertyProvider* gesture_property_provider;
#endif
};

#if defined(USE_EVDEV_GESTURES)
bool UseGesturesLibraryForDevice(const EventDeviceInfo& devinfo) {
  if (devinfo.HasTouchpad())
    return true;

  if (devinfo.HasRelXY())
    return true;  // mouse

  return false;
}

void SetGestureIntProperty(GesturePropertyProvider* provider,
                           int id,
                           const std::string& name,
                           int value) {
  GesturesProp* property = provider->GetProperty(id, name);
  if (property) {
    std::vector<int> values(1, value);
    property->SetIntValue(values);
  }
}

void SetGestureBoolProperty(GesturePropertyProvider* provider,
                            int id,
                            const std::string& name,
                            bool value) {
  GesturesProp* property = provider->GetProperty(id, name);
  if (property) {
    std::vector<bool> values(1, value);
    property->SetBoolValue(values);
  }
}

// Return the values in an array in one string. Used for touch logging.
template <typename T>
std::string DumpArrayProperty(const std::vector<T>& value, const char* format) {
  std::string ret;
  for (size_t i = 0; i < value.size(); ++i) {
    if (i > 0)
      ret.append(", ");
    ret.append(base::StringPrintf(format, value[i]));
  }
  return ret;
}

// Return the values in a gesture property in one string. Used for touch
// logging.
std::string DumpGesturePropertyValue(GesturesProp* property) {
  switch (property->type()) {
    case GesturePropertyProvider::PT_INT:
      return DumpArrayProperty(property->GetIntValue(), "%d");
      break;
    case GesturePropertyProvider::PT_SHORT:
      return DumpArrayProperty(property->GetShortValue(), "%d");
      break;
    case GesturePropertyProvider::PT_BOOL:
      return DumpArrayProperty(property->GetBoolValue(), "%d");
      break;
    case GesturePropertyProvider::PT_STRING:
      return "\"" + property->GetStringValue() + "\"";
      break;
    case GesturePropertyProvider::PT_REAL:
      return DumpArrayProperty(property->GetDoubleValue(), "%lf");
      break;
    default:
      NOTREACHED();
      break;
  }
  return std::string();
}

// Dump touch device property values to a string.
void DumpTouchDeviceStatus(GesturePropertyProvider* provider,
                           std::string* status) {
  // We use DT_ALL since we want gesture property values for all devices that
  // run with the gesture library, not just mice or touchpads.
  std::vector<int> ids;
  provider->GetDeviceIdsByType(DT_ALL, &ids);

  // Dump the property names and values for each device.
  for (size_t i = 0; i < ids.size(); ++i) {
    std::vector<std::string> names = provider->GetPropertyNamesById(ids[i]);
    status->append("\n");
    status->append(base::StringPrintf("ID %d:\n", ids[i]));
    status->append(base::StringPrintf(
        "Device \'%s\':\n", provider->GetDeviceNameById(ids[i]).c_str()));

    // Note that, unlike X11, we don't maintain the "atom" concept here.
    // Therefore, the property name indices we output here shouldn't be treated
    // as unique identifiers of the properties.
    std::sort(names.begin(), names.end());
    for (size_t j = 0; j < names.size(); ++j) {
      status->append(base::StringPrintf("\t%s (%zu):", names[j].c_str(), j));
      GesturesProp* property = provider->GetProperty(ids[i], names[j]);
      status->append("\t" + DumpGesturePropertyValue(property) + '\n');
    }
  }
}
#endif

scoped_ptr<EventConverterEvdev> CreateConverter(
    const OpenInputDeviceParams& params,
    int fd,
    InputDeviceType type,
    const EventDeviceInfo& devinfo) {
#if defined(USE_EVDEV_GESTURES)
  // Touchpad or mouse: use gestures library.
  // EventReaderLibevdevCros -> GestureInterpreterLibevdevCros -> DispatchEvent
  if (UseGesturesLibraryForDevice(devinfo)) {
    scoped_ptr<GestureInterpreterLibevdevCros> gesture_interp =
        make_scoped_ptr(new GestureInterpreterLibevdevCros(
            params.id, params.cursor, params.gesture_property_provider,
            params.dispatcher));
    return make_scoped_ptr(new EventReaderLibevdevCros(
        fd, params.path, params.id, type, devinfo, gesture_interp.Pass()));
  }
#endif

  // Touchscreen: use TouchEventConverterEvdev.
  if (devinfo.HasMTAbsXY()) {
    scoped_ptr<TouchEventConverterEvdev> converter(new TouchEventConverterEvdev(
        fd, params.path, params.id, type, params.dispatcher));
    converter->Initialize(devinfo);
    return converter.Pass();
  }

  // Graphics tablet
  if (devinfo.HasAbsXY())
    return make_scoped_ptr<EventConverterEvdev>(new TabletEventConverterEvdev(
        fd, params.path, params.id, type, params.cursor, devinfo,
        params.dispatcher));

  // Everything else: use EventConverterEvdevImpl.
  return make_scoped_ptr<EventConverterEvdevImpl>(
      new EventConverterEvdevImpl(fd, params.path, params.id, type, devinfo,
                                  params.cursor, params.dispatcher));
}

// Open an input device. Opening may put the calling thread to sleep, and
// therefore should be run on a thread where latency is not critical. We
// run it on a thread from the worker pool.
//
// This takes a TaskRunner and runs the reply on that thread, so that we
// can hop threads if necessary (back to the UI thread).
void OpenInputDevice(scoped_ptr<OpenInputDeviceParams> params,
                     scoped_refptr<base::TaskRunner> reply_runner,
                     const OpenInputDeviceReplyCallback& reply_callback) {
  const base::FilePath& path = params->path;

  TRACE_EVENT1("ozone", "OpenInputDevice", "path", path.value());

  int fd = open(path.value().c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    PLOG(ERROR) << "Cannot open '" << path.value();
    return;
  }

  // Use monotonic timestamps for events. The touch code in particular
  // expects event timestamps to correlate to the monotonic clock
  // (base::TimeTicks).
  unsigned int clk = CLOCK_MONOTONIC;
  if (ioctl(fd, EVIOCSCLOCKID, &clk))
    PLOG(ERROR) << "failed to set CLOCK_MONOTONIC";

  EventDeviceInfo devinfo;
  if (!devinfo.Initialize(fd)) {
    LOG(ERROR) << "failed to get device information for " << path.value();
    close(fd);
    return;
  }

  InputDeviceType type = GetInputDeviceTypeFromPath(path);

  scoped_ptr<EventConverterEvdev> converter =
      CreateConverter(*params, fd, type, devinfo);

  // Reply with the constructed converter.
  reply_runner->PostTask(FROM_HERE,
                         base::Bind(reply_callback, base::Passed(&converter)));
}

// Close an input device. Closing may put the calling thread to sleep, and
// therefore should be run on a thread where latency is not critical. We
// run it on the FILE thread.
void CloseInputDevice(const base::FilePath& path,
                      scoped_ptr<EventConverterEvdev> converter) {
  TRACE_EVENT1("ozone", "CloseInputDevice", "path", path.value());
  converter.reset();
}

}  // namespace

InputDeviceFactoryEvdev::InputDeviceFactoryEvdev(
    DeviceEventDispatcherEvdev* dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> dispatch_runner,
    CursorDelegateEvdev* cursor)
    : ui_task_runner_(dispatch_runner),
      cursor_(cursor),
#if defined(USE_EVDEV_GESTURES)
      gesture_property_provider_(new GesturePropertyProvider),
#endif
      dispatcher_(dispatcher),
      weak_ptr_factory_(this) {
}

InputDeviceFactoryEvdev::~InputDeviceFactoryEvdev() {
  STLDeleteValues(&converters_);
}

void InputDeviceFactoryEvdev::AddInputDevice(int id,
                                             const base::FilePath& path) {
  scoped_ptr<OpenInputDeviceParams> params(new OpenInputDeviceParams);
  params->id = id;
  params->path = path;
  params->cursor = cursor_;
  params->dispatcher = dispatcher_;

#if defined(USE_EVDEV_GESTURES)
  params->gesture_property_provider = gesture_property_provider_.get();
#endif

  OpenInputDeviceReplyCallback reply_callback =
      base::Bind(&InputDeviceFactoryEvdev::AttachInputDevice,
                 weak_ptr_factory_.GetWeakPtr());

  // Dispatch task to open from the worker pool, since open may block.
  base::WorkerPool::PostTask(FROM_HERE,
                             base::Bind(&OpenInputDevice, base::Passed(&params),
                                        ui_task_runner_, reply_callback),
                             true /* task_is_slow */);
}

void InputDeviceFactoryEvdev::RemoveInputDevice(const base::FilePath& path) {
  DetachInputDevice(path);
}

void InputDeviceFactoryEvdev::AttachInputDevice(
    scoped_ptr<EventConverterEvdev> converter) {
  const base::FilePath& path = converter->path();

  TRACE_EVENT1("ozone", "AttachInputDevice", "path", path.value());
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  // If we have an existing device, detach it. We don't want two
  // devices with the same name open at the same time.
  if (converters_[path])
    DetachInputDevice(path);

  // Add initialized device to map.
  converters_[path] = converter.release();
  converters_[path]->Start();

  NotifyDeviceChange(*converters_[path]);
}

void InputDeviceFactoryEvdev::DetachInputDevice(const base::FilePath& path) {
  TRACE_EVENT1("ozone", "DetachInputDevice", "path", path.value());
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  // Remove device from map.
  scoped_ptr<EventConverterEvdev> converter(converters_[path]);
  converters_.erase(path);

  if (converter) {
    // Cancel libevent notifications from this converter. This part must be
    // on UI since the polling happens on UI.
    converter->Stop();

    NotifyDeviceChange(*converter);

    // Dispatch task to close from the worker pool, since close may block.
    base::WorkerPool::PostTask(
        FROM_HERE,
        base::Bind(&CloseInputDevice, path, base::Passed(&converter)), true);
  }
}

void InputDeviceFactoryEvdev::DisableInternalTouchpad() {
  for (const auto& it : converters_) {
    EventConverterEvdev* converter = it.second;
    if (converter->type() == InputDeviceType::INPUT_DEVICE_INTERNAL &&
        converter->HasTouchpad()) {
      DCHECK(!converter->HasKeyboard());
      converter->set_ignore_events(true);
    }
  }
}

void InputDeviceFactoryEvdev::EnableInternalTouchpad() {
  for (const auto& it : converters_) {
    EventConverterEvdev* converter = it.second;
    if (converter->type() == InputDeviceType::INPUT_DEVICE_INTERNAL &&
        converter->HasTouchpad()) {
      DCHECK(!converter->HasKeyboard());
      converter->set_ignore_events(false);
    }
  }
}

void InputDeviceFactoryEvdev::DisableInternalKeyboardExceptKeys(
    scoped_ptr<std::set<DomCode>> excepted_keys) {
  for (const auto& it : converters_) {
    EventConverterEvdev* converter = it.second;
    if (converter->type() == InputDeviceType::INPUT_DEVICE_INTERNAL &&
        converter->HasKeyboard()) {
      converter->SetAllowedKeys(excepted_keys.Pass());
    }
  }
}

void InputDeviceFactoryEvdev::EnableInternalKeyboard() {
  for (const auto& it : converters_) {
    EventConverterEvdev* converter = it.second;
    if (converter->type() == InputDeviceType::INPUT_DEVICE_INTERNAL &&
        converter->HasKeyboard()) {
      converter->AllowAllKeys();
    }
  }
}

void InputDeviceFactoryEvdev::SetTouchpadSensitivity(int value) {
  SetIntPropertyForOneType(DT_TOUCHPAD, "Pointer Sensitivity", value);
  SetIntPropertyForOneType(DT_TOUCHPAD, "Scroll Sensitivity", value);
}

void InputDeviceFactoryEvdev::SetTapToClick(bool enabled) {
  SetBoolPropertyForOneType(DT_TOUCHPAD, "Tap Enable", enabled);
}

void InputDeviceFactoryEvdev::SetThreeFingerClick(bool enabled) {
  SetBoolPropertyForOneType(DT_TOUCHPAD, "T5R2 Three Finger Click Enable",
                            enabled);
}

void InputDeviceFactoryEvdev::SetTapDragging(bool enabled) {
  SetBoolPropertyForOneType(DT_TOUCHPAD, "Tap Drag Enable", enabled);
}

void InputDeviceFactoryEvdev::SetNaturalScroll(bool enabled) {
  SetBoolPropertyForOneType(DT_MULTITOUCH, "Australian Scrolling", enabled);
}

void InputDeviceFactoryEvdev::SetMouseSensitivity(int value) {
  SetIntPropertyForOneType(DT_MOUSE, "Pointer Sensitivity", value);
  SetIntPropertyForOneType(DT_MOUSE, "Scroll Sensitivity", value);
}

void InputDeviceFactoryEvdev::SetTapToClickPaused(bool state) {
  SetBoolPropertyForOneType(DT_TOUCHPAD, "Tap Paused", state);
}

void InputDeviceFactoryEvdev::GetTouchDeviceStatus(
    const GetTouchDeviceStatusReply& reply) {
  scoped_ptr<std::string> status(new std::string);
#if defined(USE_EVDEV_GESTURES)
  DumpTouchDeviceStatus(gesture_property_provider_.get(), status.get());
#endif
  reply.Run(status.Pass());
}

void InputDeviceFactoryEvdev::NotifyDeviceChange(
    const EventConverterEvdev& converter) {
  if (converter.HasTouchscreen())
    NotifyTouchscreensUpdated();

  if (converter.HasKeyboard())
    NotifyKeyboardsUpdated();

  if (converter.HasMouse())
    NotifyMouseDevicesUpdated();

  if (converter.HasTouchpad())
    NotifyMouseDevicesUpdated();
}

void InputDeviceFactoryEvdev::NotifyTouchscreensUpdated() {
  std::vector<TouchscreenDevice> touchscreens;
  for (auto it = converters_.begin(); it != converters_.end(); ++it) {
    if (it->second->HasTouchscreen()) {
      // TODO(spang): Extract the number of touch-points supported by the
      // device.
      const int touch_points = 11;
      touchscreens.push_back(TouchscreenDevice(
          it->second->id(), it->second->type(), std::string() /* Device name */,
          it->second->GetTouchscreenSize(), touch_points));
    }
  }

  dispatcher_->DispatchTouchscreenDevicesUpdated(touchscreens);
}

void InputDeviceFactoryEvdev::NotifyKeyboardsUpdated() {
  std::vector<KeyboardDevice> keyboards;
  for (auto it = converters_.begin(); it != converters_.end(); ++it) {
    if (it->second->HasKeyboard()) {
      keyboards.push_back(KeyboardDevice(it->second->id(), it->second->type(),
                                         std::string() /* Device name */));
    }
  }

  dispatcher_->DispatchKeyboardDevicesUpdated(keyboards);
}

void InputDeviceFactoryEvdev::NotifyMouseDevicesUpdated() {
  std::vector<InputDevice> mice;
  for (auto it = converters_.begin(); it != converters_.end(); ++it) {
    if (it->second->HasMouse()) {
      mice.push_back(InputDevice(it->second->id(), it->second->type(),
                                 std::string() /* Device name */));
    }
  }

  dispatcher_->DispatchMouseDevicesUpdated(mice);
}

void InputDeviceFactoryEvdev::NotifyTouchpadDevicesUpdated() {
  std::vector<InputDevice> touchpads;
  for (auto it = converters_.begin(); it != converters_.end(); ++it) {
    if (it->second->HasTouchpad()) {
      touchpads.push_back(InputDevice(it->second->id(), it->second->type(),
                                      std::string() /* Device name */));
    }
  }

  dispatcher_->DispatchTouchpadDevicesUpdated(touchpads);
}

void InputDeviceFactoryEvdev::SetIntPropertyForOneType(
    const EventDeviceType type,
    const std::string& name,
    int value) {
#if defined(USE_EVDEV_GESTURES)
  std::vector<int> ids;
  gesture_property_provider_->GetDeviceIdsByType(type, &ids);
  for (size_t i = 0; i < ids.size(); ++i) {
    SetGestureIntProperty(gesture_property_provider_.get(), ids[i], name,
                          value);
  }
#endif
  // In the future, we may add property setting codes for other non-gesture
  // devices. One example would be keyboard settings.
  // TODO(sheckylin): See http://crbug.com/398518 for example.
}

void InputDeviceFactoryEvdev::SetBoolPropertyForOneType(
    const EventDeviceType type,
    const std::string& name,
    bool value) {
#if defined(USE_EVDEV_GESTURES)
  std::vector<int> ids;
  gesture_property_provider_->GetDeviceIdsByType(type, &ids);
  for (size_t i = 0; i < ids.size(); ++i) {
    SetGestureBoolProperty(gesture_property_provider_.get(), ids[i], name,
                           value);
  }
#endif
}

}  // namespace ui
