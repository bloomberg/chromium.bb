// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_converter_evdev_impl.h"

#include <errno.h>
#include <linux/input.h>

#include "ui/events/event.h"
#include "ui/events/keycodes/dom4/keycode_converter.h"
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
    const KeyEventDispatchCallback& key_callback,
    const MouseMoveEventDispatchCallback& mouse_move_callback,
    const MouseButtonEventDispatchCallback& mouse_button_callback)
    : EventConverterEvdev(fd, path, id, type),
      has_keyboard_(devinfo.HasKeyboard()),
      has_touchpad_(devinfo.HasTouchpad()),
      x_offset_(0),
      y_offset_(0),
      cursor_(cursor),
      key_callback_(key_callback),
      mouse_move_callback_(mouse_move_callback),
      mouse_button_callback_(mouse_button_callback) {
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

void EventConverterEvdevImpl::SetAllowedKeys(
    scoped_ptr<std::set<DomCode>> allowed_keys) {
  DCHECK(HasKeyboard());
  allowed_keys_ = allowed_keys.Pass();
}

void EventConverterEvdevImpl::AllowAllKeys() {
  DCHECK(HasKeyboard());
  allowed_keys_.reset();
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
        FlushEvents();
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
  DomCode key_code = KeycodeConverter::NativeKeycodeToDomCode(
      EvdevCodeToNativeCode(input.code));
  if (!allowed_keys_ || allowed_keys_->count(key_code)) {
    key_callback_.Run(
        KeyEventParams(id_, input.code, input.value != kKeyReleaseValue));
  }
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

void EventConverterEvdevImpl::DispatchMouseButton(const input_event& input) {
  if (!cursor_)
    return;

  mouse_button_callback_.Run(MouseButtonEventParams(id_, cursor_->GetLocation(),
                                                    input.code, input.value,
                                                    /* allow_remap */ true));
}

void EventConverterEvdevImpl::FlushEvents() {
  if (!cursor_ || (x_offset_ == 0 && y_offset_ == 0))
    return;

  cursor_->MoveCursor(gfx::Vector2dF(x_offset_, y_offset_));

  mouse_move_callback_.Run(MouseMoveEventParams(id_, cursor_->GetLocation()));

  x_offset_ = 0;
  y_offset_ = 0;
}

}  // namespace ui
