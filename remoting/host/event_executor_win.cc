// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/event_executor.h"

#include <windows.h>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "remoting/host/capturer.h"
#include "remoting/proto/event.pb.h"
#include "ui/base/keycodes/keyboard_codes.h"

namespace remoting {

using protocol::MouseEvent;
using protocol::KeyEvent;

namespace {

// A class to generate events on Windows.
class EventExecutorWin : public EventExecutor {
 public:
  EventExecutorWin(MessageLoop* message_loop, Capturer* capturer);
  virtual ~EventExecutorWin() {}

  virtual void InjectKeyEvent(const KeyEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const MouseEvent& event) OVERRIDE;

 private:
  void HandleKey(const KeyEvent& event);
  void HandleMouse(const MouseEvent& event);

  MessageLoop* message_loop_;
  Capturer* capturer_;

  DISALLOW_COPY_AND_ASSIGN(EventExecutorWin);
};

EventExecutorWin::EventExecutorWin(MessageLoop* message_loop,
                                   Capturer* capturer)
    : message_loop_(message_loop),
      capturer_(capturer) {
}

void EventExecutorWin::InjectKeyEvent(const KeyEvent& event) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&EventExecutorWin::InjectKeyEvent, base::Unretained(this),
                   event));
    return;
  }

  HandleKey(event);
}

void EventExecutorWin::InjectMouseEvent(const MouseEvent& event) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&EventExecutorWin::InjectMouseEvent, base::Unretained(this),
                   event));
    return;
  }

  HandleMouse(event);
}

void EventExecutorWin::HandleKey(const KeyEvent& event) {
  int key = event.keycode();
  bool down = event.pressed();

  // Calculate scan code from virtual key.
  HKL hkl = GetKeyboardLayout(0);
  int scan_code = MapVirtualKeyEx(key, MAPVK_VK_TO_VSC_EX, hkl);

  INPUT input;
  memset(&input, 0, sizeof(input));

  input.type = INPUT_KEYBOARD;
  input.ki.time = 0;
  input.ki.wVk = key;
  input.ki.wScan = scan_code;

  // Flag to mark extended 'e0' key scancodes. Without this, the left and
  // right windows keys will not be handled properly (on US keyboard).
  if ((scan_code & 0xFF00) == 0xE000) {
    input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
  }

  // Flag to mark keyup events. Default is keydown.
  if (!down) {
    input.ki.dwFlags |= KEYEVENTF_KEYUP;
  }

  SendInput(1, &input, sizeof(INPUT));
}

void EventExecutorWin::HandleMouse(const MouseEvent& event) {
  // TODO(garykac) Collapse mouse (x,y) and button events into a single
  // input event when possible.
  if (event.has_x() && event.has_y()) {
    int x = event.x();
    int y = event.y();

    INPUT input;
    input.type = INPUT_MOUSE;
    input.mi.time = 0;
    SkISize screen_size = capturer_->size_most_recent();
    if ((screen_size.width() > 0) && (screen_size.height() > 0)) {
      input.mi.dx = static_cast<int>((x * 65535) / screen_size.width());
      input.mi.dy = static_cast<int>((y * 65535) / screen_size.height());
      input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
      SendInput(1, &input, sizeof(INPUT));
    }
  }

  if (event.has_wheel_offset_x() && event.has_wheel_offset_y()) {
    INPUT wheel;
    wheel.type = INPUT_MOUSE;
    wheel.mi.time = 0;

    int dx = event.wheel_offset_x();
    int dy = event.wheel_offset_y();

    if (dx != 0) {
      wheel.mi.mouseData = dx;
      wheel.mi.dwFlags = MOUSEEVENTF_HWHEEL;
      SendInput(1, &wheel, sizeof(INPUT));
    }
    if (dy != 0) {
      wheel.mi.mouseData = dy;
      wheel.mi.dwFlags = MOUSEEVENTF_WHEEL;
      SendInput(1, &wheel, sizeof(INPUT));
    }
  }

  if (event.has_button() && event.has_button_down()) {
    INPUT button_event;
    button_event.type = INPUT_MOUSE;
    button_event.mi.time = 0;
    button_event.mi.dx = 0;
    button_event.mi.dy = 0;

    MouseEvent::MouseButton button = event.button();
    bool down = event.button_down();
    if (button == MouseEvent::BUTTON_LEFT) {
      button_event.mi.dwFlags =
          down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    } else if (button == MouseEvent::BUTTON_MIDDLE) {
      button_event.mi.dwFlags =
          down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    } else if (button == MouseEvent::BUTTON_RIGHT) {
      button_event.mi.dwFlags =
          down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    } else {
      button_event.mi.dwFlags =
          down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    }

    SendInput(1, &button_event, sizeof(INPUT));
  }
}

}  // namespace

EventExecutor* EventExecutor::Create(MessageLoop* message_loop,
                                     Capturer* capturer) {
  return new EventExecutorWin(message_loop, capturer);
}

}  // namespace remoting
