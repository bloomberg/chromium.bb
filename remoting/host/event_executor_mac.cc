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
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/message_decoder.h"
#include "third_party/skia/include/core/SkPoint.h"

namespace remoting {

namespace {

using protocol::ClipboardEvent;
using protocol::KeyEvent;
using protocol::MouseEvent;

// USB to Mac keycode mapping table.
#define USB_KEYMAP(usb, xkb, win, mac) {usb, mac}
#include "remoting/host/usb_keycode_map.h"
#undef USB_KEYMAP

// A class to generate events on Mac.
class EventExecutorMac : public EventExecutor {
 public:
  EventExecutorMac(MessageLoop* message_loop);
  virtual ~EventExecutorMac() {}

  // ClipboardStub interface.
  virtual void InjectClipboardEvent(const ClipboardEvent& event) OVERRIDE;

  // InputStub interface.
  virtual void InjectKeyEvent(const KeyEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const MouseEvent& event) OVERRIDE;

  // EventExecutor interface.
  virtual void OnSessionStarted() OVERRIDE;
  virtual void OnSessionFinished() OVERRIDE;

 private:
  MessageLoop* message_loop_;
  SkIPoint mouse_pos_;
  uint32 mouse_button_state_;

  DISALLOW_COPY_AND_ASSIGN(EventExecutorMac);
};

EventExecutorMac::EventExecutorMac(
    MessageLoop* message_loop)
    : message_loop_(message_loop), mouse_button_state_(0) {
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
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,
  // 0x04 - 0x07
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,
  // 0x08 - 0x0B
  kVK_Delete, kVK_Tab, kInvalidKeycode, kInvalidKeycode,
  // 0x0C - 0x0F
  kInvalidKeycode, kVK_Return, kInvalidKeycode, kInvalidKeycode,

  // 0x10 - 0x13
  kVK_Shift, kVK_Control, kVK_Option, kInvalidKeycode,
  // 0x14 - 0x17
  kVK_CapsLock, kVK_JIS_Kana, /* VKEY_HANGUL */ kInvalidKeycode,
  /* VKEY_JUNJA */ kInvalidKeycode,
  // 0x18 - 0x1B
  /* VKEY_FINAL */ kInvalidKeycode, /* VKEY_Kanji */ kInvalidKeycode,
  kInvalidKeycode, kVK_Escape,
  // 0x1C - 0x1F
  /* VKEY_CONVERT */ kInvalidKeycode, /* VKEY_NONCONVERT */ kInvalidKeycode,
  /* VKEY_ACCEPT */ kInvalidKeycode, /* VKEY_MODECHANGE */ kInvalidKeycode,

  // 0x20 - 0x23
  kVK_Space, kVK_PageUp, kVK_PageDown, kVK_End,
  // 0x24 - 0x27
  kVK_Home, kVK_LeftArrow, kVK_UpArrow, kVK_RightArrow,
  // 0x28 - 0x2B
  kVK_DownArrow, /* VKEY_SELECT */ kInvalidKeycode,
  /* VKEY_PRINT */ kInvalidKeycode, /* VKEY_EXECUTE */ kInvalidKeycode,
  // 0x2C - 0x2F
  /* VKEY_SNAPSHOT */ kInvalidKeycode, /* XK_INSERT */ kInvalidKeycode,
  kVK_ForwardDelete, kVK_Help,

  // 0x30 - 0x33
  kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3,
  // 0x34 - 0x37
  kVK_ANSI_4, kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7,
  // 0x38 - 0x3B
  kVK_ANSI_8, kVK_ANSI_9, kInvalidKeycode, kInvalidKeycode,
  // 0x3C - 0x3F
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,

  // 0x40 - 0x43
  kInvalidKeycode, kVK_ANSI_A, kVK_ANSI_B, kVK_ANSI_C,
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
  kVK_Command, kVK_Command, kInvalidKeycode, /* VKEY_SLEEP */ kInvalidKeycode,

  // 0x60 - 0x63
  kVK_ANSI_Keypad0, kVK_ANSI_Keypad1, kVK_ANSI_Keypad2, kVK_ANSI_Keypad3,
  // 0x64 - 0x67
  kVK_ANSI_Keypad4, kVK_ANSI_Keypad5, kVK_ANSI_Keypad6, kVK_ANSI_Keypad7,
  // 0x68 - 0x6B
  kVK_ANSI_Keypad8, kVK_ANSI_Keypad9, kVK_ANSI_KeypadMultiply,
  kVK_ANSI_KeypadPlus,
  // 0x6C - 0x6F
  /* VKEY_SEPARATOR */ kInvalidKeycode, kVK_ANSI_KeypadMinus,
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
  /* VKEY_F21 */ kInvalidKeycode, /* VKEY_F22 */ kInvalidKeycode,
  /* VKEY_F23 */ kInvalidKeycode, /* XKEY_F24 */ kInvalidKeycode,
  // 0x88 - 0x8B
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,
  // 0x8C - 0x8F
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,

  // 0x90 - 0x93
  /* VKEY_NUMLOCK */ kInvalidKeycode, /* VKEY_SCROLL */ kInvalidKeycode,
  kInvalidKeycode, kInvalidKeycode,
  // 0x94 - 0x97
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,
  // 0x98 - 0x9B
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,
  // 0x9C - 0x9F
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,

  // 0xA0 - 0xA3
  kVK_Shift, kVK_RightShift, kVK_Control, kVK_RightControl,
  // 0xA4 - 0xA7
  kVK_Option, kVK_RightOption,
  /* XF86kVK_Back */ kInvalidKeycode, /* XF86kVK_Forward */ kInvalidKeycode,
  // 0xA8 - 0xAB
  /* XF86kVK_Refresh */ kInvalidKeycode, /* XF86kVK_Stop */ kInvalidKeycode,
  /* XF86kVK_Search */ kInvalidKeycode,
  /* XF86kVK_Favorites */ kInvalidKeycode,
  // 0xAC - 0xAF
  /* XF86kVK_HomePage */ kInvalidKeycode, kVK_Mute, kVK_VolumeDown,
  kVK_VolumeUp,

  // 0xB0 - 0xB3
  /* XF86kVK_AudioNext */ kInvalidKeycode,
  /* XF86kVK_AudioPrev */ kInvalidKeycode,
  /* XF86kVK_AudioStop */ kInvalidKeycode,
  /* XF86kVK_AudioPause */ kInvalidKeycode,
  // 0xB4 - 0xB7
  /* XF86kVK_Mail */ kInvalidKeycode, /* XF86kVK_AudioMedia */ kInvalidKeycode,
  /* XF86kVK_Launch0 */ kInvalidKeycode, /* XF86kVK_Launch1 */ kInvalidKeycode,
  // 0xB8 - 0xBB
  kInvalidKeycode, kInvalidKeycode, kVK_ANSI_Semicolon, kVK_ANSI_Equal,
  // 0xBC - 0xBF
  kVK_ANSI_Comma, kVK_ANSI_Minus, kVK_ANSI_Period, kVK_ANSI_Slash,

  // 0xC0 - 0xC3
  kVK_ANSI_Grave, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,
  // 0xC4 - 0xC7
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,
  // 0xC8 - 0xCB
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,
  // 0xCC - 0xCF
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,

  // 0xD0 - 0xD3
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,
  // 0xD4 - 0xD7
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,
  // 0xD8 - 0xDB
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kVK_ANSI_LeftBracket,
  // 0xDC - 0xDF
  kVK_ANSI_Backslash, kVK_ANSI_RightBracket, kVK_ANSI_Quote,
  /* VKEY_OEM_8 */ kInvalidKeycode,

  // 0xE0 - 0xE3
  kInvalidKeycode, kInvalidKeycode, /* VKEY_OEM_102 */ kInvalidKeycode,
  kInvalidKeycode,
  // 0xE4 - 0xE7
  kInvalidKeycode, /* VKEY_PROCESSKEY */ kInvalidKeycode, kInvalidKeycode,
  /* VKEY_PACKET */ kInvalidKeycode,
  // 0xE8 - 0xEB
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,
  // 0xEC - 0xEF
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,

  // 0xF0 - 0xF3
  kInvalidKeycode, kInvalidKeycode, kInvalidKeycode, kInvalidKeycode,
  // 0xF4 - 0xF7
  kInvalidKeycode, kInvalidKeycode, /* VKEY_ATTN */ kInvalidKeycode,
  /* VKEY_CRSEL */ kInvalidKeycode,
  // 0xF8 - 0xFB
  /* VKEY_EXSEL */ kInvalidKeycode, /* VKEY_EREOF */ kInvalidKeycode,
  /* VKEY_PLAY */ kInvalidKeycode, /* VKEY_ZOOM */ kInvalidKeycode,
  // 0xFC - 0xFF
  /* VKEY_NONAME */ kInvalidKeycode, /* VKEY_PA1 */ kInvalidKeycode,
  /* VKEY_OEM_CLEAR */ kInvalidKeycode, kInvalidKeycode
};

void EventExecutorMac::InjectClipboardEvent(const ClipboardEvent& event) {
  // TODO(simonmorris): Implement clipboard injection.
}

void EventExecutorMac::InjectKeyEvent(const KeyEvent& event) {
  // HostEventDispatcher should filter events missing the pressed field.
  DCHECK(event.has_pressed());

  int keycode = kInvalidKeycode;
  if (event.has_usb_keycode()) {
    keycode = UsbKeycodeToNativeKeycode(event.usb_keycode());
    VLOG(3) << "Converting USB keycode: " << std::hex << event.usb_keycode()
            << " to keycode: " << keycode << std::dec;
  } else if (event.has_keycode()) {
    int win_keycode = event.keycode();
    if (win_keycode >= 0 && win_keycode < 256) {
      keycode = kUsVkeyToKeysym[win_keycode];
    }
    VLOG(3) << "Converting VKEY: " << std::hex << event.keycode()
            << " to keycode: " << keycode << std::dec;
  }

  // If we couldn't determine the Mac virtual key code then ignore the event.
  if (keycode == kInvalidKeycode)
    return;

  // We use the deprecated event injection API because the new one doesn't
  // work with switched-out sessions (curtain mode).
  CGError error = CGPostKeyboardEvent(0, keycode, event.pressed());
  if (error != kCGErrorSuccess) {
    LOG(WARNING) << "CGPostKeyboardEvent error " << error;
  }
}

void EventExecutorMac::InjectMouseEvent(const MouseEvent& event) {
  if (event.has_x() && event.has_y()) {
    // TODO(wez): This code assumes that MouseEvent(0,0) (top-left of client view)
    // corresponds to local (0,0) (top-left of primary monitor).  That won't in
    // general be true on multi-monitor systems, though.
    VLOG(3) << "Moving mouse to " << event.x() << "," << event.y();
    mouse_pos_ = SkIPoint::Make(event.x(), event.y());
  }
  if (event.has_button() && event.has_button_down()) {
    if (event.button() >= 1 && event.button() <= 3) {
      VLOG(2) << "Button " << event.button()
              << (event.button_down() ? " down" : " up");
      int button_change = 1 << (event.button() - 1);
      if (event.button_down())
        mouse_button_state_ |= button_change;
      else
        mouse_button_state_ &= ~button_change;
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
  CGPoint position = CGPointMake(mouse_pos_.x(), mouse_pos_.y());
  enum {
    LeftBit = 1 << (MouseEvent::BUTTON_LEFT - 1),
    MiddleBit = 1 << (MouseEvent::BUTTON_MIDDLE - 1),
    RightBit = 1 << (MouseEvent::BUTTON_RIGHT - 1)
  };
  CGError error = CGPostMouseEvent(position, true, 3,
                                   (mouse_button_state_ & LeftBit) != 0,
                                   (mouse_button_state_ & RightBit) != 0,
                                   (mouse_button_state_ & MiddleBit) != 0);
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

void EventExecutorMac::OnSessionStarted() {
  return;
}

void EventExecutorMac::OnSessionFinished() {
  return;
}

}  // namespace

scoped_ptr<EventExecutor> EventExecutor::Create(
    MessageLoop* message_loop, Capturer* capturer) {
  return scoped_ptr<EventExecutor>(
      new EventExecutorMac(message_loop));
}

}  // namespace remoting
