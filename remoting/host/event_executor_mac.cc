// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/event_executor_mac.h"

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>

#include "base/message_loop.h"
#include "base/task.h"
#include "base/mac/scoped_cftyperef.h"
#include "remoting/host/capturer.h"
#include "remoting/protocol/message_decoder.h"
#include "remoting/proto/internal.pb.h"

namespace remoting {

using protocol::MouseEvent;
using protocol::KeyEvent;

EventExecutorMac::EventExecutorMac(
    MessageLoopForUI* message_loop, Capturer* capturer)
    : message_loop_(message_loop),
      capturer_(capturer), last_x_(0), last_y_(0), modifiers_(0) {
}

EventExecutorMac::~EventExecutorMac() {
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
const int kUsVkeyToKeysym[256] = {
  // 0x00 - 0x07
  -1, -1, -1, -1,
  // 0x04 - 0x07
  -1, -1, -1, -1,
  // 0x08 - 0x0B
  kVK_Delete, kVK_Tab, -1, -1,
  // 0x0C - 0x0F
  -1, kVK_Return, -1, -1,

  // 0x10 - 0x13
  kVK_Shift, kVK_Control, kVK_Option, -1,
  // 0x14 - 0x17
  kVK_CapsLock, kVK_JIS_Kana, /* VKEY_HANGUL */ -1, /* VKEY_JUNJA */ -1,
  // 0x18 - 0x1B
  /* VKEY_FINAL */ -1, /* VKEY_Kanji */ -1, -1, kVK_Escape,
  // 0x1C - 0x1F
  /* VKEY_CONVERT */ -1, /* VKEY_NONCONVERT */ -1,
  /* VKEY_ACCEPT */ -1, /* VKEY_MODECHANGE */ -1,

  // 0x20 - 0x23
  kVK_Space, kVK_PageUp, kVK_PageDown, kVK_End,
  // 0x24 - 0x27
  kVK_Home, kVK_LeftArrow, kVK_UpArrow, kVK_RightArrow,
  // 0x28 - 0x2B
  kVK_DownArrow, /* VKEY_SELECT */ -1, /* VKEY_PRINT */ -1,
  /* VKEY_EXECUTE */ -1,
  // 0x2C - 0x2F
  /* VKEY_SNAPSHOT */ -1, /* XK_INSERT */ -1, kVK_ForwardDelete, kVK_Help,

  // 0x30 - 0x33
  kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3,
  // 0x34 - 0x37
  kVK_ANSI_4, kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7,
  // 0x38 - 0x3B
  kVK_ANSI_8, kVK_ANSI_9, -1, -1,
  // 0x3C - 0x3F
  -1, -1, -1, -1,

  // 0x40 - 0x43
  -1, kVK_ANSI_A, kVK_ANSI_B, kVK_ANSI_C,
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
  kVK_Command, kVK_Command, -1, /* VKEY_SLEEP */ -1,

  // 0x60 - 0x63
  kVK_ANSI_Keypad0, kVK_ANSI_Keypad1, kVK_ANSI_Keypad2, kVK_ANSI_Keypad3,
  // 0x64 - 0x67
  kVK_ANSI_Keypad4, kVK_ANSI_Keypad5, kVK_ANSI_Keypad6, kVK_ANSI_Keypad7,
  // 0x68 - 0x6B
  kVK_ANSI_Keypad8, kVK_ANSI_Keypad9, kVK_ANSI_KeypadMultiply,
  kVK_ANSI_KeypadPlus,
  // 0x6C - 0x6F
  /* VKEY_SEPARATOR */ -1, kVK_ANSI_KeypadMinus,
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
  /* VKEY_F21 */ -1, /* VKEY_F22 */ -1, /* VKEY_F23 */ -1, /* XKEY_F24 */ -1,
  // 0x88 - 0x8B
  -1, -1, -1, -1,
  // 0x8C - 0x8F
  -1, -1, -1, -1,

  // 0x90 - 0x93
  /* VKEY_NUMLOCK */ -1, /* VKEY_SCROLL */ -1, -1, -1,
  // 0x94 - 0x97
  -1, -1, -1, -1,
  // 0x98 - 0x9B
  -1, -1, -1, -1,
  // 0x9C - 0x9F
  -1, -1, -1, -1,

  // 0xA0 - 0xA3
  kVK_Shift, kVK_RightShift, kVK_Control, kVK_RightControl,
  // 0xA4 - 0xA7
  kVK_Option, kVK_RightOption, /* XF86kVK_Back */ -1, /* XF86kVK_Forward */ -1,
  // 0xA8 - 0xAB
  /* XF86kVK_Refresh */ -1, /* XF86kVK_Stop */ -1, /* XF86kVK_Search */ -1,
  /* XF86kVK_Favorites */ -1,
  // 0xAC - 0xAF
  /* XF86kVK_HomePage */ -1, kVK_Mute, kVK_VolumeDown, kVK_VolumeUp,

  // 0xB0 - 0xB3
  /* XF86kVK_AudioNext */ -1, /* XF86kVK_AudioPrev */ -1,
  /* XF86kVK_AudioStop */ -1, /* XF86kVK_AudioPause */ -1,
  // 0xB4 - 0xB7
  /* XF86kVK_Mail */ -1, /* XF86kVK_AudioMedia */ -1, /* XF86kVK_Launch0 */ -1,
  /* XF86kVK_Launch1 */ -1,
  // 0xB8 - 0xBB
  -1, -1, kVK_ANSI_Semicolon, kVK_ANSI_KeypadPlus,
  // 0xBC - 0xBF
  kVK_ANSI_Comma, kVK_ANSI_KeypadMinus, kVK_ANSI_Period, kVK_ANSI_Slash,

  // 0xC0 - 0xC3
  kVK_ANSI_Grave, -1, -1, -1,
  // 0xC4 - 0xC7
  -1, -1, -1, -1,
  // 0xC8 - 0xCB
  -1, -1, -1, -1,
  // 0xCC - 0xCF
  -1, -1, -1, -1,

  // 0xD0 - 0xD3
  -1, -1, -1, -1,
  // 0xD4 - 0xD7
  -1, -1, -1, -1,
  // 0xD8 - 0xDB
  -1, -1, -1, kVK_ANSI_LeftBracket,
  // 0xDC - 0xDF
  kVK_ANSI_Backslash, kVK_ANSI_RightBracket, kVK_ANSI_Quote,
  /* VKEY_OEM_8 */ -1,

  // 0xE0 - 0xE3
  -1, -1, /* VKEY_OEM_102 */ -1, -1,
  // 0xE4 - 0xE7
  -1, /* VKEY_PROCESSKEY */ -1, -1, /* VKEY_PACKET */ -1,
  // 0xE8 - 0xEB
  -1, -1, -1, -1,
  // 0xEC - 0xEF
  -1, -1, -1, -1,

  // 0xF0 - 0xF3
  -1, -1, -1, -1,
  // 0xF4 - 0xF7
  -1, -1, /* VKEY_ATTN */ -1, /* VKEY_CRSEL */ -1,
  // 0xF8 - 0xFB
  /* VKEY_EXSEL */ -1, /* VKEY_EREOF */ -1, /* VKEY_PLAY */ -1,
  /* VKEY_ZOOM */ -1,
  // 0xFC - 0xFF
  /* VKEY_NONAME */ -1, /* VKEY_PA1 */ -1, /* VKEY_OEM_CLEAR */ -1, -1
};

void EventExecutorMac::InjectKeyEvent(const KeyEvent* event, Task* done) {
  int key_code = event->keycode();
  if (key_code >= 0 && key_code < 256) {
    int key_sym = kUsVkeyToKeysym[key_code];
    if (key_sym != -1) {
      base::mac::ScopedCFTypeRef<CGEventRef> kbd_event(
          CGEventCreateKeyboardEvent(0, kUsVkeyToKeysym[key_code],
                                     event->pressed()));
      int this_modifier = 0;
      switch (key_sym) {
        case kVK_Shift: case kVK_RightShift:
          this_modifier = kCGEventFlagMaskShift;
          break;
        case kVK_Control: case kVK_RightControl:
          this_modifier = kCGEventFlagMaskControl;
          break;
        case kVK_Command:
          this_modifier = kCGEventFlagMaskCommand;
          break;
        case kVK_Option: case kVK_RightOption:
          this_modifier = kCGEventFlagMaskAlternate;
          break;
      }
      if (this_modifier && event->pressed()) {
        modifiers_ |= this_modifier;
      } else if (this_modifier && !event->pressed()) {
        modifiers_ &= ~this_modifier;
      }
      CGEventSetFlags(kbd_event, modifiers_);
      CGEventPost(kCGSessionEventTap, kbd_event);
    }
  }
  done->Run();
  delete done;
}

void EventExecutorMac::InjectMouseEvent(const MouseEvent* event, Task* done) {
  CGEventType event_type = kCGEventNull;

  if (event->has_x() && event->has_y()) {
    // TODO(wez): Checking the validity of the MouseEvent should be done in core
    // cross-platform code, not here!
    // TODO(wez): This code assumes that MouseEvent(0,0) (top-left of client view)
    // corresponds to local (0,0) (top-left of primary monitor).  That won't in
    // general be true on multi-monitor systems, though.
    gfx::Size size = capturer_->size_most_recent();
    if (event->x() >= 0 || event->y() >= 0 ||
        event->x() < size.width() || event->y() < size.height()) {

      VLOG(3) << "Moving mouse to " << event->x() << "," << event->y();

      event_type = kCGEventMouseMoved;
      last_x_ = event->x();
      last_y_ = event->y();
    }
  }

  CGPoint position = CGPointMake(last_x_, last_y_);
  CGMouseButton mouse_button = 0;

  if (event->has_button() && event->has_button_down()) {

    VLOG(2) << "Button " << event->button()
            << (event->button_down() ? " down" : " up");

    event_type = event->button_down() ? kCGEventOtherMouseDown
                                      : kCGEventOtherMouseUp;
    if (MouseEvent::BUTTON_LEFT == event->button()) {
      event_type = event->button_down() ? kCGEventLeftMouseDown
                                        : kCGEventLeftMouseUp;
      mouse_button = kCGMouseButtonLeft; // not strictly necessary
    } else if (MouseEvent::BUTTON_RIGHT == event->button()) {
      event_type = event->button_down() ? kCGEventRightMouseDown
                                        : kCGEventRightMouseUp;
      mouse_button = kCGMouseButtonRight; // not strictly necessary
    } else if (MouseEvent::BUTTON_MIDDLE == event->button()) {
      mouse_button = kCGMouseButtonCenter;
    } else {
      VLOG(1) << "Unknown mouse button!" << event->button();
    }
  }

  if (event->has_wheel_offset_x() && event->has_wheel_offset_y()) {
    // TODO(wez): Should set event_type == kCGEventScrollWheel and
    // populate fields for a CGEventCreateScrollWheelEvent() here.
    NOTIMPLEMENTED() << "No scroll wheel support yet.";
  }

  if (event_type != kCGEventNull) {
    base::mac::ScopedCFTypeRef<CGEventRef>  mouse_event(
        CGEventCreateMouseEvent(NULL, event_type, position, mouse_button));
    CGEventSetFlags(mouse_event, modifiers_);
    CGEventPost(kCGSessionEventTap, mouse_event);
  }

  done->Run();
  delete done;
}

protocol::InputStub* CreateEventExecutor(MessageLoopForUI* message_loop,
                                         Capturer* capturer) {
  return new EventExecutorMac(message_loop, capturer);
}

}  // namespace remoting
