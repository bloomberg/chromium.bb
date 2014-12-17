// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_converter_evdev_impl.h"

#include <errno.h>
#include <linux/input.h>

#include "base/message_loop/message_loop.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom4/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

// Values for EV_KEY.
const int kKeyReleaseValue = 0;
const int kKeyRepeatValue = 2;

EventConverterEvdevImpl::EventConverterEvdevImpl(
    int fd,
    base::FilePath path,
    int id,
    InputDeviceType type,
    const EventDeviceInfo& devinfo,
    EventModifiersEvdev* modifiers,
    MouseButtonMapEvdev* button_map,
    CursorDelegateEvdev* cursor,
    KeyboardEvdev* keyboard,
    const EventDispatchCallback& callback)
    : EventConverterEvdev(fd, path, id, type),
      has_keyboard_(devinfo.HasKeyboard()),
      x_offset_(0),
      y_offset_(0),
      cursor_(cursor),
      keyboard_(keyboard),
      modifiers_(modifiers),
      button_map_(button_map),
      callback_(callback) {
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

  DCHECK_EQ(read_size % sizeof(*inputs), 0u);
  ProcessEvents(inputs, read_size / sizeof(*inputs));
}

bool EventConverterEvdevImpl::HasKeyboard() const {
  return has_keyboard_;
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
  keyboard_->OnKeyChange(input.code, input.value != kKeyReleaseValue);
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

  unsigned int modifier;
  const int button = button_map_->GetMappedButton(input.code);
  if (button == BTN_LEFT)
    modifier = EVDEV_MODIFIER_LEFT_MOUSE_BUTTON;
  else if (button == BTN_RIGHT)
    modifier = EVDEV_MODIFIER_RIGHT_MOUSE_BUTTON;
  else if (button == BTN_MIDDLE)
    modifier = EVDEV_MODIFIER_MIDDLE_MOUSE_BUTTON;
  else
    return;

  int flag = modifiers_->GetEventFlagFromModifier(modifier);
  modifiers_->UpdateModifier(modifier, input.value);
  callback_.Run(make_scoped_ptr(
      new MouseEvent(input.value ? ET_MOUSE_PRESSED : ET_MOUSE_RELEASED,
                     cursor_->GetLocation(),
                     cursor_->GetLocation(),
                     modifiers_->GetModifierFlags() | flag,
                     flag)));
}

void EventConverterEvdevImpl::FlushEvents() {
  if (!cursor_ || (x_offset_ == 0 && y_offset_ == 0))
    return;

  cursor_->MoveCursor(gfx::Vector2dF(x_offset_, y_offset_));

  callback_.Run(make_scoped_ptr(
      new MouseEvent(ui::ET_MOUSE_MOVED,
                     cursor_->GetLocation(),
                     cursor_->GetLocation(),
                     modifiers_->GetModifierFlags(),
                     /* changed_button_flags */ 0)));
  x_offset_ = 0;
  y_offset_ = 0;
}

}  // namespace ui
