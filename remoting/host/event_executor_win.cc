// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/event_executor_win.h"

#include <windows.h>

#include "app/keyboard_codes.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "remoting/host/capturer.h"
#include "remoting/proto/event.pb.h"

namespace remoting {

EventExecutorWin::EventExecutorWin(
    MessageLoop* message_loop, Capturer* capturer)
    : message_loop_(message_loop), capturer_(capturer) {
}

EventExecutorWin::~EventExecutorWin() {
}

void EventExecutorWin::InjectKeyEvent(const KeyEvent* event, Task* done) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &EventExecutorWin::InjectKeyEvent,
                          event, done));
    return;
  }
  HandleKey(*event);
  done->Run();
  delete done;
}

void EventExecutorWin::InjectMouseEvent(const MouseEvent* event,
                                        Task* done) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &EventExecutorWin::InjectMouseEvent,
                          event, done));
    return;
  }
  if (event->has_set_position()) {
    HandleMouseSetPosition(event->set_position());
  } else if (event->has_wheel()) {
    HandleMouseWheel(event->wheel());
  } else if (event->has_down()) {
    HandleMouseButtonDown(event->down());
  } else if (event->has_up()) {
    HandleMouseButtonUp(event->up());
  }
  done->Run();
  delete done;
}

void EventExecutorWin::HandleMouseSetPosition(
    const MouseSetPositionEvent& event) {
  int x = event.x();
  int y = event.y();
  int width = event.width();
  int height = event.height();

  // Get width and height from the capturer if they are missing from the
  // message.
  if (width == 0 || height == 0) {
    width = capturer_->width();
    height = capturer_->height();
  }
  if (width == 0 || height == 0) {
    return;
  }

  INPUT input;
  input.type = INPUT_MOUSE;
  input.mi.time = 0;
  input.mi.dx = static_cast<int>((x * 65535) / width);
  input.mi.dy = static_cast<int>((y * 65535) / height);
  input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
  SendInput(1, &input, sizeof(INPUT));
}

void EventExecutorWin::HandleMouseWheel(
    const MouseWheelEvent& event) {
  INPUT input;
  input.type = INPUT_MOUSE;
  input.mi.time = 0;

  int dx = event.offset_x();
  int dy = event.offset_y();

  if (dx != 0) {
    input.mi.mouseData = dx;
    input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
    SendInput(1, &input, sizeof(INPUT));
  }
  if (dy != 0) {
    input.mi.mouseData = dy;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    SendInput(1, &input, sizeof(INPUT));
  }
}

void EventExecutorWin::HandleMouseButtonDown(const MouseDownEvent& event) {
  INPUT input;
  input.type = INPUT_MOUSE;
  input.mi.time = 0;
  input.mi.dx = 0;
  input.mi.dy = 0;

  MouseButton button = event.button();
  if (button == MouseButtonLeft) {
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
  } else if (button == MouseButtonMiddle) {
    input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
  } else if (button == MouseButtonRight) {
    input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
  } else {
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
  }

  SendInput(1, &input, sizeof(INPUT));
}

void EventExecutorWin::HandleMouseButtonUp(const MouseUpEvent& event) {
  INPUT input;
  input.type = INPUT_MOUSE;
  input.mi.time = 0;
  input.mi.dx = 0;
  input.mi.dy = 0;

  MouseButton button = event.button();
  if (button == MouseButtonLeft) {
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
  } else if (button == MouseButtonMiddle) {
    input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
  } else if (button == MouseButtonRight) {
    input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
  } else {
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
  }

  SendInput(1, &input, sizeof(INPUT));
}

void EventExecutorWin::HandleKey(const KeyEvent& event) {
  int key = event.key();
  bool down = event.pressed();

  // Calculate scan code from virtual key.
  HKL hkl = GetKeyboardLayout(0);
  int scan_code = MapVirtualKeyEx(key, MAPVK_VK_TO_VSC_EX, hkl);

  INPUT input;
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

protocol::InputStub* CreateEventExecutor(MessageLoop* message_loop,
                                         Capturer* capturer) {
  return new EventExecutorWin(message_loop, capturer);
}

}  // namespace remoting
