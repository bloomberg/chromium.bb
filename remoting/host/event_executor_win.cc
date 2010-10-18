// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/event_executor_win.h"

#include <windows.h>
#include "app/keyboard_codes.h"
#include "base/stl_util-inl.h"
#include "remoting/host/capturer.h"
// TODO(hclam): Should not include internal.pb.h here.
#include "remoting/proto/internal.pb.h"

namespace remoting {

EventExecutorWin::EventExecutorWin(Capturer* capturer)
  : EventExecutor(capturer) {
}

EventExecutorWin::~EventExecutorWin() {
}

void EventExecutorWin::HandleInputEvent(ChromotingClientMessage* msg) {
  if (msg->has_mouse_set_position_event()) {
    HandleMouseSetPosition(msg);
  } else if (msg->has_mouse_move_event()) {
    HandleMouseMove(msg);
  } else if (msg->has_mouse_wheel_event()) {
    HandleMouseWheel(msg);
  } else if (msg->has_mouse_down_event()) {
    HandleMouseButtonDown(msg);
  } else if (msg->has_mouse_up_event()) {
    HandleMouseButtonUp(msg);
  } else if (msg->has_key_event()) {
    HandleKey(msg);
  }
  delete msg;
}

void EventExecutorWin::HandleMouseSetPosition(ChromotingClientMessage* msg) {
  int x = msg->mouse_set_position_event().x();
  int y = msg->mouse_set_position_event().y();
  int width = msg->mouse_set_position_event().width();
  int height = msg->mouse_set_position_event().height();

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

void EventExecutorWin::HandleMouseMove(ChromotingClientMessage* msg) {
  INPUT input;
  input.type = INPUT_MOUSE;
  input.mi.time = 0;
  input.mi.dx = msg->mouse_move_event().offset_x();
  input.mi.dy = msg->mouse_move_event().offset_y();
  input.mi.dwFlags = MOUSEEVENTF_MOVE;
  SendInput(1, &input, sizeof(INPUT));
}

void EventExecutorWin::HandleMouseWheel(ChromotingClientMessage* msg) {
  INPUT input;
  input.type = INPUT_MOUSE;
  input.mi.time = 0;

  int dx = msg->mouse_wheel_event().offset_x();
  int dy = msg->mouse_wheel_event().offset_y();

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

void EventExecutorWin::HandleMouseButtonDown(ChromotingClientMessage* msg) {
  INPUT input;
  input.type = INPUT_MOUSE;
  input.mi.time = 0;
  input.mi.dx = 0;
  input.mi.dy = 0;

  MouseButton button = msg->mouse_down_event().button();
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

void EventExecutorWin::HandleMouseButtonUp(ChromotingClientMessage* msg) {
  INPUT input;
  input.type = INPUT_MOUSE;
  input.mi.time = 0;
  input.mi.dx = 0;
  input.mi.dy = 0;

  MouseButton button = msg->mouse_down_event().button();
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

void EventExecutorWin::HandleKey(ChromotingClientMessage* msg) {
  int key = msg->key_event().key();
  bool down = msg->key_event().pressed();

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

}  // namespace remoting
