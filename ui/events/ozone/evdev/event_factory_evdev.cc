// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_factory_evdev.h"

#include <fcntl.h>
#include <linux/input.h>

#include "base/debug/trace_event.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/worker_pool.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/event_converter_evdev_impl.h"
#include "ui/events/ozone/evdev/input_injector_evdev.h"
#include "ui/events/ozone/evdev/tablet_event_converter_evdev.h"
#include "ui/events/ozone/evdev/touch_event_converter_evdev.h"

#if defined(USE_EVDEV_GESTURES)
#include "ui/events/ozone/evdev/libgestures_glue/event_reader_libevdev_cros.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_interpreter_libevdev_cros.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_property_provider.h"
#endif

#ifndef EVIOCSCLOCKID
#define EVIOCSCLOCKID  _IOW('E', 0xa0, int)
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

  // Callback for dispatching events. Call on UI thread only.
  EventDispatchCallback dispatch_callback;

  // State shared between devices. Must not be dereferenced on worker thread.
  EventModifiersEvdev* modifiers;
  MouseButtonMapEvdev* button_map;
  KeyboardEvdev* keyboard;
  CursorDelegateEvdev* cursor;
#if defined(USE_EVDEV_GESTURES)
  GesturePropertyProvider* gesture_property_provider;
#endif
};

#if defined(USE_EVDEV_GESTURES)
bool UseGesturesLibraryForDevice(const EventDeviceInfo& devinfo) {
  if ((devinfo.HasAbsXY() || devinfo.HasMTAbsXY()) &&
      !devinfo.IsMappedToScreen())
    return true;  // touchpad

  if (devinfo.HasRelXY())
    return true;  // mouse

  return false;
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
            params.id, params.modifiers, params.button_map, params.cursor,
            params.keyboard, params.gesture_property_provider,
            params.dispatch_callback));
    return make_scoped_ptr(new EventReaderLibevdevCros(
        fd, params.path, params.id, type, devinfo, gesture_interp.Pass()));
  }
#endif

  // Touchscreen: use TouchEventConverterEvdev.
  if (devinfo.HasMTAbsXY()) {
    scoped_ptr<TouchEventConverterEvdev> converter(new TouchEventConverterEvdev(
        fd, params.path, params.id, type, params.dispatch_callback));
    converter->Initialize(devinfo);
    return converter.Pass();
  }

  // Graphics tablet
  if (devinfo.HasAbsXY())
    return make_scoped_ptr<EventConverterEvdev>(new TabletEventConverterEvdev(
        fd, params.path, params.id, type, params.modifiers, params.cursor,
        devinfo, params.dispatch_callback));

  // Everything else: use EventConverterEvdevImpl.
  return make_scoped_ptr<EventConverterEvdevImpl>(new EventConverterEvdevImpl(
      fd, params.path, params.id, type, devinfo, params.modifiers,
      params.button_map, params.cursor, params.keyboard,
      params.dispatch_callback));
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

EventFactoryEvdev::EventFactoryEvdev(CursorDelegateEvdev* cursor,
                                     DeviceManager* device_manager,
                                     KeyboardLayoutEngine* keyboard_layout)
    : last_device_id_(0),
      device_manager_(device_manager),
      dispatch_callback_(
          base::Bind(&EventFactoryEvdev::PostUiEvent, base::Unretained(this))),
      keyboard_(&modifiers_, keyboard_layout, dispatch_callback_),
      cursor_(cursor),
#if defined(USE_EVDEV_GESTURES)
      gesture_property_provider_(new GesturePropertyProvider),
#endif
      input_controller_(this,
                        &button_map_
#if defined(USE_EVDEV_GESTURES)
                        ,
                        gesture_property_provider_.get()
#endif
                            ),
      weak_ptr_factory_(this) {
  DCHECK(device_manager_);
}

EventFactoryEvdev::~EventFactoryEvdev() { STLDeleteValues(&converters_); }

scoped_ptr<SystemInputInjector> EventFactoryEvdev::CreateSystemInputInjector() {
  return make_scoped_ptr(new InputInjectorEvdev(
      &modifiers_, cursor_, &keyboard_, dispatch_callback_));
}

void EventFactoryEvdev::PostUiEvent(scoped_ptr<Event> event) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EventFactoryEvdev::DispatchUiEventTask,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&event)));
}

void EventFactoryEvdev::DispatchUiEventTask(scoped_ptr<Event> event) {
  DispatchEvent(event.get());
}

void EventFactoryEvdev::AttachInputDevice(
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

void EventFactoryEvdev::OnDeviceEvent(const DeviceEvent& event) {
  if (event.device_type() != DeviceEvent::INPUT)
    return;

  switch (event.action_type()) {
    case DeviceEvent::ADD:
    case DeviceEvent::CHANGE: {
      TRACE_EVENT1("ozone", "OnDeviceAdded", "path", event.path().value());

      scoped_ptr<OpenInputDeviceParams> params(new OpenInputDeviceParams);
      params->id = NextDeviceId();
      params->path = event.path();
      params->dispatch_callback = dispatch_callback_;
      params->modifiers = &modifiers_;
      params->button_map = &button_map_;
      params->keyboard = &keyboard_;
      params->cursor = cursor_;
#if defined(USE_EVDEV_GESTURES)
      params->gesture_property_provider = gesture_property_provider_.get();
#endif

      OpenInputDeviceReplyCallback reply_callback =
          base::Bind(&EventFactoryEvdev::AttachInputDevice,
                     weak_ptr_factory_.GetWeakPtr());

      // Dispatch task to open from the worker pool, since open may block.
      base::WorkerPool::PostTask(FROM_HERE,
                                 base::Bind(&OpenInputDevice,
                                            base::Passed(&params),
                                            ui_task_runner_,
                                            reply_callback),
                                 true /* task_is_slow */);
    }
      break;
    case DeviceEvent::REMOVE: {
      TRACE_EVENT1("ozone", "OnDeviceRemoved", "path", event.path().value());
      DetachInputDevice(event.path());
    }
      break;
  }
}

void EventFactoryEvdev::OnDispatcherListChanged() {
  if (!ui_task_runner_.get()) {
    ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    // Scan & monitor devices.
    device_manager_->AddObserver(this);
    device_manager_->ScanDevices(this);
  }
}

void EventFactoryEvdev::DetachInputDevice(const base::FilePath& path) {
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
        base::Bind(&CloseInputDevice, path, base::Passed(&converter)),
        true);
  }
}

void EventFactoryEvdev::WarpCursorTo(gfx::AcceleratedWidget widget,
                                     const gfx::PointF& location) {
  if (cursor_) {
    cursor_->MoveCursorTo(widget, location);
    PostUiEvent(make_scoped_ptr(new MouseEvent(ET_MOUSE_MOVED,
                                               cursor_->GetLocation(),
                                               cursor_->GetLocation(),
                                               modifiers_.GetModifierFlags(),
                                               /* changed_button_flags */ 0)));
  }
}

void EventFactoryEvdev::NotifyDeviceChange(
    const EventConverterEvdev& converter) {
  if (converter.HasTouchscreen())
    NotifyTouchscreensUpdated();

  if (converter.HasKeyboard())
    NotifyKeyboardsUpdated();
}

void EventFactoryEvdev::NotifyTouchscreensUpdated() {
  DeviceHotplugEventObserver* observer = DeviceDataManager::GetInstance();
  std::vector<TouchscreenDevice> touchscreens;
  for (auto it = converters_.begin(); it != converters_.end(); ++it) {
    if (it->second->HasTouchscreen()) {
      touchscreens.push_back(TouchscreenDevice(
          it->second->id(), it->second->type(), std::string() /* Device name */,
          it->second->GetTouchscreenSize()));
    }
  }

  observer->OnTouchscreenDevicesUpdated(touchscreens);
}

void EventFactoryEvdev::NotifyKeyboardsUpdated() {
  DeviceHotplugEventObserver* observer = DeviceDataManager::GetInstance();
  std::vector<KeyboardDevice> keyboards;
  for (auto it = converters_.begin(); it != converters_.end(); ++it) {
    if (it->second->HasKeyboard()) {
      keyboards.push_back(KeyboardDevice(it->second->id(), it->second->type(),
                                         std::string() /* Device name */));
    }
  }

  observer->OnKeyboardDevicesUpdated(keyboards);
}

int EventFactoryEvdev::NextDeviceId() {
  return ++last_device_id_;
}

bool EventFactoryEvdev::GetDeviceIdsByType(const EventDeviceType type,
                                           std::vector<int>* device_ids) {
  if (device_ids)
    device_ids->clear();
  std::vector<int> ids;

#if defined(USE_EVDEV_GESTURES)
  // Ask GesturePropertyProvider for matching devices.
  gesture_property_provider_->GetDeviceIdsByType(type, &ids);
#endif
  // In the future we can add other device matching logics here.

  if (device_ids)
    device_ids->assign(ids.begin(), ids.end());
  return !ids.empty();
}

}  // namespace ui
