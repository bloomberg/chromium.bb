// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/event_executor_win.h"

#include <windows.h>
#include "app/keyboard_codes.h"
#include "base/stl_util-inl.h"
#include "remoting/host/capturer.h"

namespace remoting {

// TODO(hclam): Move this method to base.
// TODO(hclam): Using values look ugly, change it to something else.
static app::KeyboardCode WindowsKeyCodeForPosixKeyCode(int keycode) {
  switch (keycode) {
    case 0x08:
      return app::VKEY_BACK;
    case 0x09:
      return app::VKEY_TAB;
    case 0x0C:
      return app::VKEY_CLEAR;
    case 0x0D:
      return app::VKEY_RETURN;
    case 0x10:
      return app::VKEY_SHIFT;
    case 0x11:
      return app::VKEY_CONTROL;
    case 0x12:
      return app::VKEY_MENU;
    case 0x13:
      return app::VKEY_PAUSE;
    case 0x14:
      return app::VKEY_CAPITAL;
    case 0x15:
      return app::VKEY_KANA;
    case 0x17:
      return app::VKEY_JUNJA;
    case 0x18:
      return app::VKEY_FINAL;
    case 0x19:
      return app::VKEY_KANJI;
    case 0x1B:
      return app::VKEY_ESCAPE;
    case 0x1C:
      return app::VKEY_CONVERT;
    case 0x1D:
      return app::VKEY_NONCONVERT;
    case 0x1E:
      return app::VKEY_ACCEPT;
    case 0x1F:
      return app::VKEY_MODECHANGE;
    case 0x20:
      return app::VKEY_SPACE;
    case 0x21:
      return app::VKEY_PRIOR;
    case 0x22:
      return app::VKEY_NEXT;
    case 0x23:
      return app::VKEY_END;
    case 0x24:
      return app::VKEY_HOME;
    case 0x25:
      return app::VKEY_LEFT;
    case 0x26:
      return app::VKEY_UP;
    case 0x27:
      return app::VKEY_RIGHT;
    case 0x28:
      return app::VKEY_DOWN;
    case 0x29:
      return app::VKEY_SELECT;
    case 0x2A:
      return app::VKEY_PRINT;
    case 0x2B:
      return app::VKEY_EXECUTE;
    case 0x2C:
      return app::VKEY_SNAPSHOT;
    case 0x2D:
      return app::VKEY_INSERT;
    case 0x2E:
      return app::VKEY_DELETE;
    case 0x2F:
      return app::VKEY_HELP;
    case 0x30:
      return app::VKEY_0;
    case 0x31:
      return app::VKEY_1;
    case 0x32:
      return app::VKEY_2;
    case 0x33:
      return app::VKEY_3;
    case 0x34:
      return app::VKEY_4;
    case 0x35:
      return app::VKEY_5;
    case 0x36:
      return app::VKEY_6;
    case 0x37:
      return app::VKEY_7;
    case 0x38:
      return app::VKEY_8;
    case 0x39:
      return app::VKEY_9;
    case 0x41:
      return app::VKEY_A;
    case 0x42:
      return app::VKEY_B;
    case 0x43:
      return app::VKEY_C;
    case 0x44:
      return app::VKEY_D;
    case 0x45:
      return app::VKEY_E;
    case 0x46:
      return app::VKEY_F;
    case 0x47:
      return app::VKEY_G;
    case 0x48:
      return app::VKEY_H;
    case 0x49:
      return app::VKEY_I;
    case 0x4A:
      return app::VKEY_J;
    case 0x4B:
      return app::VKEY_K;
    case 0x4C:
      return app::VKEY_L;
    case 0x4D:
      return app::VKEY_M;
    case 0x4E:
      return app::VKEY_N;
    case 0x4F:
      return app::VKEY_O;
    case 0x50:
      return app::VKEY_P;
    case 0x51:
      return app::VKEY_Q;
    case 0x52:
      return app::VKEY_R;
    case 0x53:
      return app::VKEY_S;
    case 0x54:
      return app::VKEY_T;
    case 0x55:
      return app::VKEY_U;
    case 0x56:
      return app::VKEY_V;
    case 0x57:
      return app::VKEY_W;
    case 0x58:
      return app::VKEY_X;
    case 0x59:
      return app::VKEY_Y;
    case 0x5A:
      return app::VKEY_Z;
    case 0x5B:
      return app::VKEY_LWIN;
    case 0x5C:
      return app::VKEY_RWIN;
    case 0x5D:
      return app::VKEY_APPS;
    case 0x5F:
      return app::VKEY_SLEEP;
    case 0x60:
      return app::VKEY_NUMPAD0;
    case 0x61:
      return app::VKEY_NUMPAD1;
    case 0x62:
      return app::VKEY_NUMPAD2;
    case 0x63:
      return app::VKEY_NUMPAD3;
    case 0x64:
      return app::VKEY_NUMPAD4;
    case 0x65:
      return app::VKEY_NUMPAD5;
    case 0x66:
      return app::VKEY_NUMPAD6;
    case 0x67:
      return app::VKEY_NUMPAD7;
    case 0x68:
      return app::VKEY_NUMPAD8;
    case 0x69:
      return app::VKEY_NUMPAD9;
    case 0x6A:
      return app::VKEY_MULTIPLY;
    case 0x6B:
      return app::VKEY_ADD;
    case 0x6C:
      return app::VKEY_SEPARATOR;
    case 0x6D:
      return app::VKEY_SUBTRACT;
    case 0x6E:
      return app::VKEY_DECIMAL;
    case 0x6F:
      return app::VKEY_DIVIDE;
    case 0x70:
      return app::VKEY_F1;
    case 0x71:
      return app::VKEY_F2;
    case 0x72:
      return app::VKEY_F3;
    case 0x73:
      return app::VKEY_F4;
    case 0x74:
      return app::VKEY_F5;
    case 0x75:
      return app::VKEY_F6;
    case 0x76:
      return app::VKEY_F7;
    case 0x77:
      return app::VKEY_F8;
    case 0x78:
      return app::VKEY_F9;
    case 0x79:
      return app::VKEY_F10;
    case 0x7A:
      return app::VKEY_F11;
    case 0x7B:
      return app::VKEY_F12;
    case 0x7C:
      return app::VKEY_F13;
    case 0x7D:
      return app::VKEY_F14;
    case 0x7E:
      return app::VKEY_F15;
    case 0x7F:
      return app::VKEY_F16;
    case 0x80:
      return app::VKEY_F17;
    case 0x81:
      return app::VKEY_F18;
    case 0x82:
      return app::VKEY_F19;
    case 0x83:
      return app::VKEY_F20;
    case 0x84:
      return app::VKEY_F21;
    case 0x85:
      return app::VKEY_F22;
    case 0x86:
      return app::VKEY_F23;
    case 0x87:
      return app::VKEY_F24;
    case 0x90:
      return app::VKEY_NUMLOCK;
    case 0x91:
      return app::VKEY_SCROLL;
    case 0xA0:
      return app::VKEY_LSHIFT;
    case 0xA1:
      return app::VKEY_RSHIFT;
    case 0xA2:
      return app::VKEY_LCONTROL;
    case 0xA3:
      return app::VKEY_RCONTROL;
    case 0xA4:
      return app::VKEY_LMENU;
    case 0xA5:
      return app::VKEY_RMENU;
    case 0xA6:
      return app::VKEY_BROWSER_BACK;
    case 0xA7:
      return app::VKEY_BROWSER_FORWARD;
    case 0xA8:
      return app::VKEY_BROWSER_REFRESH;
    case 0xA9:
      return app::VKEY_BROWSER_STOP;
    case 0xAA:
      return app::VKEY_BROWSER_SEARCH;
    case 0xAB:
      return app::VKEY_BROWSER_FAVORITES;
    case 0xAC:
      return app::VKEY_BROWSER_HOME;
    case 0xAD:
      return app::VKEY_VOLUME_MUTE;
    case 0xAE:
      return app::VKEY_VOLUME_DOWN;
    case 0xAF:
      return app::VKEY_VOLUME_UP;
    case 0xB0:
      return app::VKEY_MEDIA_NEXT_TRACK;
    case 0xB1:
      return app::VKEY_MEDIA_PREV_TRACK;
    case 0xB2:
      return app::VKEY_MEDIA_STOP;
    case 0xB3:
      return app::VKEY_MEDIA_PLAY_PAUSE;
    case 0xB4:
      return app::VKEY_MEDIA_LAUNCH_MAIL;
    case 0xB5:
      return app::VKEY_MEDIA_LAUNCH_MEDIA_SELECT;
    case 0xB6:
      return app::VKEY_MEDIA_LAUNCH_APP1;
    case 0xB7:
      return app::VKEY_MEDIA_LAUNCH_APP2;
    case 0xBA:
      return app::VKEY_OEM_1;
    case 0xBB:
      return app::VKEY_OEM_PLUS;
    case 0xBC:
      return app::VKEY_OEM_COMMA;
    case 0xBD:
      return app::VKEY_OEM_MINUS;
    case 0xBE:
      return app::VKEY_OEM_PERIOD;
    case 0xBF:
      return app::VKEY_OEM_2;
    case 0xC0:
      return app::VKEY_OEM_3;
    case 0xDB:
      return app::VKEY_OEM_4;
    case 0xDC:
      return app::VKEY_OEM_5;
    case 0xDD:
      return app::VKEY_OEM_6;
    case 0xDE:
      return app::VKEY_OEM_7;
    case 0xDF:
      return app::VKEY_OEM_8;
    case 0xE2:
      return app::VKEY_OEM_102;
    case 0xE5:
      return app::VKEY_PROCESSKEY;
    case 0xE7:
      return app::VKEY_PACKET;
    case 0xF6:
      return app::VKEY_ATTN;
    case 0xF7:
      return app::VKEY_CRSEL;
    case 0xF8:
      return app::VKEY_EXSEL;
    case 0xF9:
      return app::VKEY_EREOF;
    case 0xFA:
      return app::VKEY_PLAY;
    case 0xFB:
      return app::VKEY_ZOOM;
    case 0xFC:
      return app::VKEY_NONAME;
    case 0xFD:
      return app::VKEY_PA1;
    case 0xFE:
      return app::VKEY_OEM_CLEAR;
    default:
      return app::VKEY_UNKNOWN;
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
