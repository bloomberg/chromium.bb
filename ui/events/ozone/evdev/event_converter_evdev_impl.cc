// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_converter_evdev_impl.h"

#include <errno.h>
#include <linux/input.h>

#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom4/keycode_converter.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/keyboard_util_evdev.h"

namespace ui {

namespace {

// Values for EV_KEY.
const int kKeyReleaseValue = 0;
const int kKeyRepeatValue = 2;

}  // namespace

EventConverterEvdevImpl::EventConverterEvdevImpl(
    int fd,
    base::FilePath path,
    int id,
    InputDeviceType type,
    const EventDeviceInfo& devinfo,
    CursorDelegateEvdev* cursor,
    DeviceEventDispatcherEvdev* dispatcher)
    : EventConverterEvdev(fd, path, id, type),
      has_keyboard_(devinfo.HasKeyboard()),
      has_touchpad_(devinfo.HasTouchpad()),
      has_caps_lock_led_(devinfo.HasLedEvent(LED_CAPSL)),
      x_offset_(0),
      y_offset_(0),
      cursor_(cursor),
      dispatcher_(dispatcher) {
}

EventConverterEvdevImpl::~EventConverterEvdevImpl() {
  Stop();
  close(fd_);
}

void EventConverterEvdevImpl::OnFileCanReadWithoutBlocking(int fd) {
  input_event inputs[4];
  ssize_t read_size = read(fd, inputs, sizeof(inputs));
  if (read_size < 0) {
    if (errno == EINTR || errno == EAGAIN)
      return;
    if (errno != ENODEV)
      PLOG(ERROR) << "error reading device " << path_.value();
    Stop();
    return;
  }

  // TODO(spang): Re-implement this by releasing buttons & temporarily closing
  // the device.
  if (ignore_events_)
    return;

  DCHECK_EQ(read_size % sizeof(*inputs), 0u);
  ProcessEvents(inputs, read_size / sizeof(*inputs));
}

bool EventConverterEvdevImpl::HasKeyboard() const {
  return has_keyboard_;
}

bool EventConverterEvdevImpl::HasTouchpad() const {
  return has_touchpad_;
}

bool EventConverterEvdevImpl::HasCapsLockLed() const {
  return has_caps_lock_led_;
}

void EventConverterEvdevImpl::SetAllowedKeys(
    scoped_ptr<std::set<DomCode>> allowed_keys) {
  DCHECK(HasKeyboard());
  allowed_keys_ = allowed_keys.Pass();
}

void EventConverterEvdevImpl::AllowAllKeys() {
  DCHECK(HasKeyboard());
  allowed_keys_.reset();
}

void EventConverterEvdevImpl::OnStopped() {
  ReleaseKeys();
}

void EventConverterEvdevImpl::ProcessEvents(const input_event* inputs,
                                            int count) {
  for (int i = 0; i < count; ++i) {
    const input_event& input = inputs[i];
    switch (input.type) {
      case EV_KEY:
        ConvertKeyEvent(input);
        break;
      case EV_REL:
        ConvertMouseMoveEvent(input);
        break;
      case EV_SYN:
        if (input.code == SYN_DROPPED)
          OnLostSync();
        else if (input.code == SYN_REPORT)
          FlushEvents(input);
        break;
    }
  }
}

void EventConverterEvdevImpl::ConvertKeyEvent(const input_event& input) {
  // Ignore repeat events.
  if (input.value == kKeyRepeatValue)
    return;

  // Mouse processing.
  if (input.code >= BTN_MOUSE && input.code < BTN_JOYSTICK) {
    DispatchMouseButton(input);
    return;
  }

  // Keyboard processing.
  OnKeyChange(input.code, input.value != kKeyReleaseValue,
              TimeDeltaFromInputEvent(input));
}

void EventConverterEvdevImpl::ConvertMouseMoveEvent(const input_event& input) {
  if (!cursor_)
    return;
  switch (input.code) {
    case REL_X:
      x_offset_ = input.value;
      break;
    case REL_Y:
      y_offset_ = input.value;
      break;
  }
}

void EventConverterEvdevImpl::OnKeyChange(unsigned int key,
                                          bool down,
                                          const base::TimeDelta& timestamp) {
  if (key > KEY_MAX)
    return;

  if (down == key_state_.test(key))
    return;

  // Apply key filter (releases for previously pressed keys are excepted).
  DomCode key_code =
      KeycodeConverter::NativeKeycodeToDomCode(EvdevCodeToNativeCode(key));
  if (down && allowed_keys_ && allowed_keys_->count(key_code))
    return;

  // State transition: !(down) -> (down)
  if (down)
    key_state_.set(key);
  else
    key_state_.reset(key);

  dispatcher_->DispatchKeyEvent(KeyEventParams(id_, key, down, timestamp));
}

void EventConverterEvdevImpl::ReleaseKeys() {
  base::TimeDelta timestamp = ui::EventTimeForNow();
  for (int key = 0; key < KEY_CNT; ++key)
    OnKeyChange(key, false /* down */, timestamp);
}

void EventConverterEvdevImpl::OnLostSync() {
  LOG(WARNING) << "kernel dropped input events";

  // We may have missed key releases. Release everything.
  // TODO(spang): Use EVIOCGKEY to avoid releasing keys that are still held.
  ReleaseKeys();
}

void EventConverterEvdevImpl::DispatchMouseButton(const input_event& input) {
  if (!cursor_)
    return;

  dispatcher_->DispatchMouseButtonEvent(MouseButtonEventParams(
      id_, cursor_->GetLocation(), input.code, input.value,
      /* allow_remap */ true, TimeDeltaFromInputEvent(input)));
}

void EventConverterEvdevImpl::FlushEvents(const input_event& input) {
  if (!cursor_ || (x_offset_ == 0 && y_offset_ == 0))
    return;

  cursor_->MoveCursor(gfx::Vector2dF(x_offset_, y_offset_));

  dispatcher_->DispatchMouseMoveEvent(MouseMoveEventParams(
      id_, cursor_->GetLocation(), TimeDeltaFromInputEvent(input)));

  x_offset_ = 0;
  y_offset_ = 0;
}

}  // namespace ui
