// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ozone/evdev/key_event_converter_ozone.h"

#include <linux/input.h>

#include "ui/base/events/event.h"
#include "ui/base/keycodes/keyboard_codes.h"

namespace {

ui::KeyboardCode KeyboardCodeFromButton(int code) {
  switch (code) {
    case KEY_VOLUMEDOWN:
      return ui::VKEY_VOLUME_DOWN;

    case KEY_VOLUMEUP:
      return ui::VKEY_VOLUME_UP;

    case KEY_POWER:
      return ui::VKEY_POWER;
  }

  LOG(ERROR) << "Unknown key code: " << code;
  return static_cast<ui::KeyboardCode>(0);
}

}  // namespace

namespace ui {

// TODO(rjkroege): Stop leaking file descriptor.
KeyEventConverterOzone::KeyEventConverterOzone() {}
KeyEventConverterOzone::~KeyEventConverterOzone() {}

void KeyEventConverterOzone::OnFileCanReadWithoutBlocking(int fd) {
  input_event inputs[4];
  ssize_t read_size = read(fd, inputs, sizeof(inputs));
  if (read_size <= 0)
    return;

  CHECK_EQ(read_size % sizeof(*inputs), 0u);
  for (unsigned i = 0; i < read_size / sizeof(*inputs); ++i) {
    const input_event& input = inputs[i];
    if (input.type == EV_KEY) {
      scoped_ptr<KeyEvent> key(
          new KeyEvent(input.value == 1 ? ET_KEY_PRESSED : ET_KEY_RELEASED,
                       KeyboardCodeFromButton(input.code),
                       0,
                       true));
      DispatchEvent(key.PassAs<ui::Event>());
    } else if (input.type == EV_SYN) {
      // TODO(sadrul): Handle this case appropriately.
    }
  }
}

void KeyEventConverterOzone::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

}  // namespace ui
