// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/event_executor.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/message_loop.h"
#include "remoting/host/capturer.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/message_decoder.h"

namespace remoting {

namespace {

// USB to Mac keycode mapping table.
#define USB_KEYMAP(usb, xkb, win, mac) {usb, mac}
#include "remoting/host/usb_keycode_map.h"
#define INVALID_KEYCODE 0xffff

using protocol::ClipboardEvent;
using protocol::KeyEvent;
using protocol::MouseEvent;

// A class to generate events on Mac.
class EventExecutorMac : public EventExecutor {
 public:
  EventExecutorMac(MessageLoop* message_loop, Capturer* capturer);
  virtual ~EventExecutorMac() {}

  // ClipboardStub interface.
  virtual void InjectClipboardEvent(const ClipboardEvent& event) OVERRIDE;

  // InputStub interface.
  virtual void InjectKeyEvent(const KeyEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const MouseEvent& event) OVERRIDE;

 private:
  MessageLoop* message_loop_;
  Capturer* capturer_;
  int last_x_, last_y_;
  int mouse_buttons_;

  DISALLOW_COPY_AND_ASSIGN(EventExecutorMac);
};

EventExecutorMac::EventExecutorMac(
    MessageLoop* message_loop, Capturer* capturer)
    : message_loop_(message_loop),
      capturer_(capturer), last_x_(0), last_y_(0), mouse_buttons_(0) {
}

// Hard-coded mapping from Virtual Key codes to Mac KeySyms.
// This mapping is only valid if both client and host are using a
// US English keyboard layout.
// Because we're passing VK codes on the wire, with no Scancode,
// "extended" flag, etc, things like distinguishing left & right
// Shift keys doesn't work.
//
// TODO(wez): Replace this with something more closely tied to what
// WebInputEventFactory does on Linux/GTK, and which respects the
// host's keyboard layout.
//
// TODO(garykac): Remove this table once we switch to using USB
// keycodes.
const int kUsVkeyToKeysym[256] = {
  // 0x00 - 0x07
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,
  // 0x04 - 0x07
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,
  // 0x08 - 0x0B
  kVK_Delete, kVK_Tab, INVALID_KEYCODE, INVALID_KEYCODE,
  // 0x0C - 0x0F
  INVALID_KEYCODE, kVK_Return, INVALID_KEYCODE, INVALID_KEYCODE,

  // 0x10 - 0x13
  kVK_Shift, kVK_Control, kVK_Option, INVALID_KEYCODE,
  // 0x14 - 0x17
  kVK_CapsLock, kVK_JIS_Kana, /* VKEY_HANGUL */ INVALID_KEYCODE,
  /* VKEY_JUNJA */ INVALID_KEYCODE,
  // 0x18 - 0x1B
  /* VKEY_FINAL */ INVALID_KEYCODE, /* VKEY_Kanji */ INVALID_KEYCODE,
  INVALID_KEYCODE, kVK_Escape,
  // 0x1C - 0x1F
  /* VKEY_CONVERT */ INVALID_KEYCODE, /* VKEY_NONCONVERT */ INVALID_KEYCODE,
  /* VKEY_ACCEPT */ INVALID_KEYCODE, /* VKEY_MODECHANGE */ INVALID_KEYCODE,

  // 0x20 - 0x23
  kVK_Space, kVK_PageUp, kVK_PageDown, kVK_End,
  // 0x24 - 0x27
  kVK_Home, kVK_LeftArrow, kVK_UpArrow, kVK_RightArrow,
  // 0x28 - 0x2B
  kVK_DownArrow, /* VKEY_SELECT */ INVALID_KEYCODE,
  /* VKEY_PRINT */ INVALID_KEYCODE, /* VKEY_EXECUTE */ INVALID_KEYCODE,
  // 0x2C - 0x2F
  /* VKEY_SNAPSHOT */ INVALID_KEYCODE, /* XK_INSERT */ INVALID_KEYCODE,
  kVK_ForwardDelete, kVK_Help,

  // 0x30 - 0x33
  kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3,
  // 0x34 - 0x37
  kVK_ANSI_4, kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7,
  // 0x38 - 0x3B
  kVK_ANSI_8, kVK_ANSI_9, INVALID_KEYCODE, INVALID_KEYCODE,
  // 0x3C - 0x3F
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,

  // 0x40 - 0x43
  INVALID_KEYCODE, kVK_ANSI_A, kVK_ANSI_B, kVK_ANSI_C,
  // 0x44 - 0x47
  kVK_ANSI_D, kVK_ANSI_E, kVK_ANSI_F, kVK_ANSI_G,
  // 0x48 - 0x4B
  kVK_ANSI_H, kVK_ANSI_I, kVK_ANSI_J, kVK_ANSI_K,
  // 0x4C - 0x4F
  kVK_ANSI_L, kVK_ANSI_M, kVK_ANSI_N, kVK_ANSI_O,

  // 0x50 - 0x53
  kVK_ANSI_P, kVK_ANSI_Q, kVK_ANSI_R, kVK_ANSI_S,
  // 0x54 - 0x57
  kVK_ANSI_T, kVK_ANSI_U, kVK_ANSI_V, kVK_ANSI_W,
  // 0x58 - 0x5B
  kVK_ANSI_X, kVK_ANSI_Y, kVK_ANSI_Z, kVK_Command,
  // 0x5C - 0x5F
  kVK_Command, kVK_Command, INVALID_KEYCODE, /* VKEY_SLEEP */ INVALID_KEYCODE,

  // 0x60 - 0x63
  kVK_ANSI_Keypad0, kVK_ANSI_Keypad1, kVK_ANSI_Keypad2, kVK_ANSI_Keypad3,
  // 0x64 - 0x67
  kVK_ANSI_Keypad4, kVK_ANSI_Keypad5, kVK_ANSI_Keypad6, kVK_ANSI_Keypad7,
  // 0x68 - 0x6B
  kVK_ANSI_Keypad8, kVK_ANSI_Keypad9, kVK_ANSI_KeypadMultiply,
  kVK_ANSI_KeypadPlus,
  // 0x6C - 0x6F
  /* VKEY_SEPARATOR */ INVALID_KEYCODE, kVK_ANSI_KeypadMinus,
  kVK_ANSI_KeypadDecimal, kVK_ANSI_KeypadDivide,

  // 0x70 - 0x73
  kVK_F1, kVK_F2, kVK_F3, kVK_F4,
  // 0x74 - 0x77
  kVK_F5, kVK_F6, kVK_F7, kVK_F8,
  // 0x78 - 0x7B
  kVK_F9, kVK_F10, kVK_F11, kVK_F12,
  // 0x7C - 0x7F
  kVK_F13, kVK_F14, kVK_F15, kVK_F16,

  // 0x80 - 0x83
  kVK_F17, kVK_F18, kVK_F19, kVK_F20,
  // 0x84 - 0x87
  /* VKEY_F21 */ INVALID_KEYCODE, /* VKEY_F22 */ INVALID_KEYCODE,
  /* VKEY_F23 */ INVALID_KEYCODE, /* XKEY_F24 */ INVALID_KEYCODE,
  // 0x88 - 0x8B
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,
  // 0x8C - 0x8F
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,

  // 0x90 - 0x93
  /* VKEY_NUMLOCK */ INVALID_KEYCODE, /* VKEY_SCROLL */ INVALID_KEYCODE,
  INVALID_KEYCODE, INVALID_KEYCODE,
  // 0x94 - 0x97
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,
  // 0x98 - 0x9B
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,
  // 0x9C - 0x9F
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,

  // 0xA0 - 0xA3
  kVK_Shift, kVK_RightShift, kVK_Control, kVK_RightControl,
  // 0xA4 - 0xA7
  kVK_Option, kVK_RightOption,
  /* XF86kVK_Back */ INVALID_KEYCODE, /* XF86kVK_Forward */ INVALID_KEYCODE,
  // 0xA8 - 0xAB
  /* XF86kVK_Refresh */ INVALID_KEYCODE, /* XF86kVK_Stop */ INVALID_KEYCODE,
  /* XF86kVK_Search */ INVALID_KEYCODE,
  /* XF86kVK_Favorites */ INVALID_KEYCODE,
  // 0xAC - 0xAF
  /* XF86kVK_HomePage */ INVALID_KEYCODE, kVK_Mute, kVK_VolumeDown,
  kVK_VolumeUp,

  // 0xB0 - 0xB3
  /* XF86kVK_AudioNext */ INVALID_KEYCODE,
  /* XF86kVK_AudioPrev */ INVALID_KEYCODE,
  /* XF86kVK_AudioStop */ INVALID_KEYCODE,
  /* XF86kVK_AudioPause */ INVALID_KEYCODE,
  // 0xB4 - 0xB7
  /* XF86kVK_Mail */ INVALID_KEYCODE, /* XF86kVK_AudioMedia */ INVALID_KEYCODE,
  /* XF86kVK_Launch0 */ INVALID_KEYCODE, /* XF86kVK_Launch1 */ INVALID_KEYCODE,
  // 0xB8 - 0xBB
  INVALID_KEYCODE, INVALID_KEYCODE, kVK_ANSI_Semicolon, kVK_ANSI_Equal,
  // 0xBC - 0xBF
  kVK_ANSI_Comma, kVK_ANSI_Minus, kVK_ANSI_Period, kVK_ANSI_Slash,

  // 0xC0 - 0xC3
  kVK_ANSI_Grave, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,
  // 0xC4 - 0xC7
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,
  // 0xC8 - 0xCB
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,
  // 0xCC - 0xCF
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,

  // 0xD0 - 0xD3
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,
  // 0xD4 - 0xD7
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,
  // 0xD8 - 0xDB
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, kVK_ANSI_LeftBracket,
  // 0xDC - 0xDF
  kVK_ANSI_Backslash, kVK_ANSI_RightBracket, kVK_ANSI_Quote,
  /* VKEY_OEM_8 */ INVALID_KEYCODE,

  // 0xE0 - 0xE3
  INVALID_KEYCODE, INVALID_KEYCODE, /* VKEY_OEM_102 */ INVALID_KEYCODE,
  INVALID_KEYCODE,
  // 0xE4 - 0xE7
  INVALID_KEYCODE, /* VKEY_PROCESSKEY */ INVALID_KEYCODE, INVALID_KEYCODE,
  /* VKEY_PACKET */ INVALID_KEYCODE,
  // 0xE8 - 0xEB
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,
  // 0xEC - 0xEF
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,

  // 0xF0 - 0xF3
  INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE, INVALID_KEYCODE,
  // 0xF4 - 0xF7
  INVALID_KEYCODE, INVALID_KEYCODE, /* VKEY_ATTN */ INVALID_KEYCODE,
  /* VKEY_CRSEL */ INVALID_KEYCODE,
  // 0xF8 - 0xFB
  /* VKEY_EXSEL */ INVALID_KEYCODE, /* VKEY_EREOF */ INVALID_KEYCODE,
  /* VKEY_PLAY */ INVALID_KEYCODE, /* VKEY_ZOOM */ INVALID_KEYCODE,
  // 0xFC - 0xFF
  /* VKEY_NONAME */ INVALID_KEYCODE, /* VKEY_PA1 */ INVALID_KEYCODE,
  /* VKEY_OEM_CLEAR */ INVALID_KEYCODE, INVALID_KEYCODE
};

uint16_t UsbKeycodeToMacKeycode(uint32_t usb_keycode) {
  if (usb_keycode == 0)
    return INVALID_KEYCODE;

  for (uint i = 0; i < arraysize(usb_keycode_map); i++) {
    if (usb_keycode_map[i].usb_keycode == usb_keycode)
      return usb_keycode_map[i].native_keycode;
  }

  return INVALID_KEYCODE;
}

void EventExecutorMac::InjectClipboardEvent(const ClipboardEvent& event) {
  // TODO(simonmorris): Implement clipboard injection.
}

void EventExecutorMac::InjectKeyEvent(const KeyEvent& event) {
  int keycode = 0;
  if (event.has_usb_keycode() && event.usb_keycode() != INVALID_KEYCODE) {
    keycode = UsbKeycodeToMacKeycode(event.usb_keycode());
    VLOG(1) << "USB keycode: " << std::hex << event.usb_keycode()
            << " to Mac keycode: " << keycode << std::dec;
  } else {
    int win_keycode = event.keycode();
    if (win_keycode >= 0 && win_keycode < 256) {
      keycode = kUsVkeyToKeysym[win_keycode];
    }
  }

  if (keycode != INVALID_KEYCODE) {
    // We use the deprecated event injection API because the new one doesn't
    // work with switched-out sessions (curtain mode).
    CGError error = CGPostKeyboardEvent(0, keycode, event.pressed());
    if (error != kCGErrorSuccess) {
      LOG(WARNING) << "CGPostKeyboardEvent error " << error;
    }
  }
}

void EventExecutorMac::InjectMouseEvent(const MouseEvent& event) {
  if (event.has_x() && event.has_y()) {
    // TODO(wez): Checking the validity of the MouseEvent should be done in core
    // cross-platform code, not here!
    // TODO(wez): This code assumes that MouseEvent(0,0) (top-left of client view)
    // corresponds to local (0,0) (top-left of primary monitor).  That won't in
    // general be true on multi-monitor systems, though.
    SkISize size = capturer_->size_most_recent();
    if (event.x() >= 0 || event.y() >= 0 ||
        event.x() < size.width() || event.y() < size.height()) {
      VLOG(3) << "Moving mouse to " << event.x() << "," << event.y();
      last_x_ = event.x();
      last_y_ = event.y();
    } else {
      VLOG(1) << "Invalid mouse position " << event.x() << "," << event.y();
    }
  }
  if (event.has_button() && event.has_button_down()) {
    if (event.button() >= 1 && event.button() <= 3) {
      VLOG(2) << "Button " << event.button()
              << (event.button_down() ? " down" : " up");
      int button_change = 1 << (event.button() - 1);
      if (event.button_down())
        mouse_buttons_ |= button_change;
      else
        mouse_buttons_ &= ~button_change;
    } else {
      VLOG(1) << "Unknown mouse button: " << event.button();
    }
  }
  // We use the deprecated CGPostMouseEvent API because we receive low-level
  // mouse events, whereas CGEventCreateMouseEvent is for injecting higher-level
  // events. For example, the deprecated APIs will detect double-clicks or drags
  // in a way that is consistent with how they would be generated using a local
  // mouse, whereas the new APIs expect us to inject these higher-level events
  // directly.
  CGPoint position = CGPointMake(last_x_, last_y_);
  enum {
    LeftBit = 1 << (MouseEvent::BUTTON_LEFT - 1),
    MiddleBit = 1 << (MouseEvent::BUTTON_MIDDLE - 1),
    RightBit = 1 << (MouseEvent::BUTTON_RIGHT - 1)
  };
  CGError error = CGPostMouseEvent(position, true, 3,
                                   (mouse_buttons_ & LeftBit) != 0,
                                   (mouse_buttons_ & RightBit) != 0,
                                   (mouse_buttons_ & MiddleBit) != 0);
  if (error != kCGErrorSuccess) {
    LOG(WARNING) << "CGPostMouseEvent error " << error;
  }

  if (event.has_wheel_offset_x() && event.has_wheel_offset_y()) {
    int dx = event.wheel_offset_x();
    int dy = event.wheel_offset_y();
    // Note that |dy| (the vertical wheel) is the primary wheel.
    error = CGPostScrollWheelEvent(2, dy, dx);
    if (error != kCGErrorSuccess) {
      LOG(WARNING) << "CGPostScrollWheelEvent error " << error;
    }
  }
}

}  // namespace

scoped_ptr<protocol::HostEventStub> EventExecutor::Create(
    MessageLoop* message_loop, Capturer* capturer) {
  return scoped_ptr<protocol::HostEventStub>(
      new EventExecutorMac(message_loop, capturer));
}

}  // namespace remoting
