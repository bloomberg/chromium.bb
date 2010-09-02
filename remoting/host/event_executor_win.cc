// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/event_executor_win.h"

#include <windows.h>
#include "base/keyboard_codes.h"
#include "base/stl_util-inl.h"
#include "remoting/host/capturer.h"

namespace remoting {

// TODO(hclam): Move this method to base.
// TODO(hclam): Using values look ugly, change it to something else.
static base::KeyboardCode WindowsKeyCodeForPosixKeyCode(int keycode) {
  switch (keycode) {
    case 0x08:
      return base::VKEY_BACK;
    case 0x09:
      return base::VKEY_TAB;
    case 0x0C:
      return base::VKEY_CLEAR;
    case 0x0D:
      return base::VKEY_RETURN;
    case 0x10:
      return base::VKEY_SHIFT;
    case 0x11:
      return base::VKEY_CONTROL;
    case 0x12:
      return base::VKEY_MENU;
    case 0x13:
      return base::VKEY_PAUSE;
    case 0x14:
      return base::VKEY_CAPITAL;
    case 0x15:
      return base::VKEY_KANA;
    case 0x17:
      return base::VKEY_JUNJA;
    case 0x18:
      return base::VKEY_FINAL;
    case 0x19:
      return base::VKEY_KANJI;
    case 0x1B:
      return base::VKEY_ESCAPE;
    case 0x1C:
      return base::VKEY_CONVERT;
    case 0x1D:
      return base::VKEY_NONCONVERT;
    case 0x1E:
      return base::VKEY_ACCEPT;
    case 0x1F:
      return base::VKEY_MODECHANGE;
    case 0x20:
      return base::VKEY_SPACE;
    case 0x21:
      return base::VKEY_PRIOR;
    case 0x22:
      return base::VKEY_NEXT;
    case 0x23:
      return base::VKEY_END;
    case 0x24:
      return base::VKEY_HOME;
    case 0x25:
      return base::VKEY_LEFT;
    case 0x26:
      return base::VKEY_UP;
    case 0x27:
      return base::VKEY_RIGHT;
    case 0x28:
      return base::VKEY_DOWN;
    case 0x29:
      return base::VKEY_SELECT;
    case 0x2A:
      return base::VKEY_PRINT;
    case 0x2B:
      return base::VKEY_EXECUTE;
    case 0x2C:
      return base::VKEY_SNAPSHOT;
    case 0x2D:
      return base::VKEY_INSERT;
    case 0x2E:
      return base::VKEY_DELETE;
    case 0x2F:
      return base::VKEY_HELP;
    case 0x30:
      return base::VKEY_0;
    case 0x31:
      return base::VKEY_1;
    case 0x32:
      return base::VKEY_2;
    case 0x33:
      return base::VKEY_3;
    case 0x34:
      return base::VKEY_4;
    case 0x35:
      return base::VKEY_5;
    case 0x36:
      return base::VKEY_6;
    case 0x37:
      return base::VKEY_7;
    case 0x38:
      return base::VKEY_8;
    case 0x39:
      return base::VKEY_9;
    case 0x41:
      return base::VKEY_A;
    case 0x42:
      return base::VKEY_B;
    case 0x43:
      return base::VKEY_C;
    case 0x44:
      return base::VKEY_D;
    case 0x45:
      return base::VKEY_E;
    case 0x46:
      return base::VKEY_F;
    case 0x47:
      return base::VKEY_G;
    case 0x48:
      return base::VKEY_H;
    case 0x49:
      return base::VKEY_I;
    case 0x4A:
      return base::VKEY_J;
    case 0x4B:
      return base::VKEY_K;
    case 0x4C:
      return base::VKEY_L;
    case 0x4D:
      return base::VKEY_M;
    case 0x4E:
      return base::VKEY_N;
    case 0x4F:
      return base::VKEY_O;
    case 0x50:
      return base::VKEY_P;
    case 0x51:
      return base::VKEY_Q;
    case 0x52:
      return base::VKEY_R;
    case 0x53:
      return base::VKEY_S;
    case 0x54:
      return base::VKEY_T;
    case 0x55:
      return base::VKEY_U;
    case 0x56:
      return base::VKEY_V;
    case 0x57:
      return base::VKEY_W;
    case 0x58:
      return base::VKEY_X;
    case 0x59:
      return base::VKEY_Y;
    case 0x5A:
      return base::VKEY_Z;
    case 0x5B:
      return base::VKEY_LWIN;
    case 0x5C:
      return base::VKEY_RWIN;
    case 0x5D:
      return base::VKEY_APPS;
    case 0x5F:
      return base::VKEY_SLEEP;
    case 0x60:
      return base::VKEY_NUMPAD0;
    case 0x61:
      return base::VKEY_NUMPAD1;
    case 0x62:
      return base::VKEY_NUMPAD2;
    case 0x63:
      return base::VKEY_NUMPAD3;
    case 0x64:
      return base::VKEY_NUMPAD4;
    case 0x65:
      return base::VKEY_NUMPAD5;
    case 0x66:
      return base::VKEY_NUMPAD6;
    case 0x67:
      return base::VKEY_NUMPAD7;
    case 0x68:
      return base::VKEY_NUMPAD8;
    case 0x69:
      return base::VKEY_NUMPAD9;
    case 0x6A:
      return base::VKEY_MULTIPLY;
    case 0x6B:
      return base::VKEY_ADD;
    case 0x6C:
      return base::VKEY_SEPARATOR;
    case 0x6D:
      return base::VKEY_SUBTRACT;
    case 0x6E:
      return base::VKEY_DECIMAL;
    case 0x6F:
      return base::VKEY_DIVIDE;
    case 0x70:
      return base::VKEY_F1;
    case 0x71:
      return base::VKEY_F2;
    case 0x72:
      return base::VKEY_F3;
    case 0x73:
      return base::VKEY_F4;
    case 0x74:
      return base::VKEY_F5;
    case 0x75:
      return base::VKEY_F6;
    case 0x76:
      return base::VKEY_F7;
    case 0x77:
      return base::VKEY_F8;
    case 0x78:
      return base::VKEY_F9;
    case 0x79:
      return base::VKEY_F10;
    case 0x7A:
      return base::VKEY_F11;
    case 0x7B:
      return base::VKEY_F12;
    case 0x7C:
      return base::VKEY_F13;
    case 0x7D:
      return base::VKEY_F14;
    case 0x7E:
      return base::VKEY_F15;
    case 0x7F:
      return base::VKEY_F16;
    case 0x80:
      return base::VKEY_F17;
    case 0x81:
      return base::VKEY_F18;
    case 0x82:
      return base::VKEY_F19;
    case 0x83:
      return base::VKEY_F20;
    case 0x84:
      return base::VKEY_F21;
    case 0x85:
      return base::VKEY_F22;
    case 0x86:
      return base::VKEY_F23;
    case 0x87:
      return base::VKEY_F24;
    case 0x90:
      return base::VKEY_NUMLOCK;
    case 0x91:
      return base::VKEY_SCROLL;
    case 0xA0:
      return base::VKEY_LSHIFT;
    case 0xA1:
      return base::VKEY_RSHIFT;
    case 0xA2:
      return base::VKEY_LCONTROL;
    case 0xA3:
      return base::VKEY_RCONTROL;
    case 0xA4:
      return base::VKEY_LMENU;
    case 0xA5:
      return base::VKEY_RMENU;
    case 0xA6:
      return base::VKEY_BROWSER_BACK;
    case 0xA7:
      return base::VKEY_BROWSER_FORWARD;
    case 0xA8:
      return base::VKEY_BROWSER_REFRESH;
    case 0xA9:
      return base::VKEY_BROWSER_STOP;
    case 0xAA:
      return base::VKEY_BROWSER_SEARCH;
    case 0xAB:
      return base::VKEY_BROWSER_FAVORITES;
    case 0xAC:
      return base::VKEY_BROWSER_HOME;
    case 0xAD:
      return base::VKEY_VOLUME_MUTE;
    case 0xAE:
      return base::VKEY_VOLUME_DOWN;
    case 0xAF:
      return base::VKEY_VOLUME_UP;
    case 0xB0:
      return base::VKEY_MEDIA_NEXT_TRACK;
    case 0xB1:
      return base::VKEY_MEDIA_PREV_TRACK;
    case 0xB2:
      return base::VKEY_MEDIA_STOP;
    case 0xB3:
      return base::VKEY_MEDIA_PLAY_PAUSE;
    case 0xB4:
      return base::VKEY_MEDIA_LAUNCH_MAIL;
    case 0xB5:
      return base::VKEY_MEDIA_LAUNCH_MEDIA_SELECT;
    case 0xB6:
      return base::VKEY_MEDIA_LAUNCH_APP1;
    case 0xB7:
      return base::VKEY_MEDIA_LAUNCH_APP2;
    case 0xBA:
      return base::VKEY_OEM_1;
    case 0xBB:
      return base::VKEY_OEM_PLUS;
    case 0xBC:
      return base::VKEY_OEM_COMMA;
    case 0xBD:
      return base::VKEY_OEM_MINUS;
    case 0xBE:
      return base::VKEY_OEM_PERIOD;
    case 0xBF:
      return base::VKEY_OEM_2;
    case 0xC0:
      return base::VKEY_OEM_3;
    case 0xDB:
      return base::VKEY_OEM_4;
    case 0xDC:
      return base::VKEY_OEM_5;
    case 0xDD:
      return base::VKEY_OEM_6;
    case 0xDE:
      return base::VKEY_OEM_7;
    case 0xDF:
      return base::VKEY_OEM_8;
    case 0xE2:
      return base::VKEY_OEM_102;
    case 0xE5:
      return base::VKEY_PROCESSKEY;
    case 0xE7:
      return base::VKEY_PACKET;
    case 0xF6:
      return base::VKEY_ATTN;
    case 0xF7:
      return base::VKEY_CRSEL;
    case 0xF8:
      return base::VKEY_EXSEL;
    case 0xF9:
      return base::VKEY_EREOF;
    case 0xFA:
      return base::VKEY_PLAY;
    case 0xFB:
      return base::VKEY_ZOOM;
    case 0xFC:
      return base::VKEY_NONAME;
    case 0xFD:
      return base::VKEY_PA1;
    case 0xFE:
      return base::VKEY_OEM_CLEAR;
    default:
      return base::VKEY_UNKNOWN;
  }
}

EventExecutorWin::EventExecutorWin(Capturer* capturer)
  : EventExecutor(capturer) {
}

EventExecutorWin::~EventExecutorWin() {
}

void EventExecutorWin::HandleInputEvents(ClientMessageList* messages) {
  for (size_t i = 0; i < messages->size(); ++i) {
    ChromotingClientMessage* msg = (*messages)[i];
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
  }
  // We simply delete all messages.
  // TODO(hclam): Delete messages processed.
  STLDeleteElements<ClientMessageList>(messages);
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
  INPUT input;
  input.type = INPUT_KEYBOARD;
  input.ki.time = 0;
  input.ki.wVk = 0;
  input.ki.wScan = msg->key_event().key();
  input.ki.dwFlags = KEYEVENTF_UNICODE;
  if (!msg->key_event().pressed()) {
    input.ki.dwFlags |= KEYEVENTF_KEYUP;
  }

  SendInput(1, &input, sizeof(INPUT));
}

}  // namespace remoting
