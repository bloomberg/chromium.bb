// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/key_event_converter_evdev.h"

#include <errno.h>
#include <linux/input.h>

#include "base/message_loop/message_loop.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom4/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/ozone/evdev/event_modifiers_evdev.h"

namespace ui {

namespace {

}  // namespace

KeyEventConverterEvdev::KeyEventConverterEvdev(int fd,
                                               base::FilePath path,
                                               int id,
                                               KeyboardEvdev* keyboard)
    : EventConverterEvdev(fd, path, id), keyboard_(keyboard) {
}

KeyEventConverterEvdev::~KeyEventConverterEvdev() {
  Stop();
  close(fd_);
}

void KeyEventConverterEvdev::OnFileCanReadWithoutBlocking(int fd) {
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

void KeyEventConverterEvdev::ProcessEvents(const input_event* inputs,
                                           int count) {
  for (int i = 0; i < count; ++i) {
    const input_event& input = inputs[i];
    if (input.type == EV_KEY) {
      keyboard_->OnKeyChange(input.code, input.value != 0);
    } else if (input.type == EV_SYN) {
      // TODO(sadrul): Handle this case appropriately.
    }
  }
}

}  // namespace ui
