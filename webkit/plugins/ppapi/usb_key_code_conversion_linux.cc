// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/usb_key_code_conversion.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

using WebKit::WebKeyboardEvent;

namespace webkit {
namespace ppapi {

namespace {

// Mapping from 8-bit xkb scancode to 32-bit USB Usage Page and Id.
//
// TODO(garykac): This table covers only USB HID Boot Protocol.  The keys
// marked with '???' should be extended (e.g. with "media keys"), or derived
// automatically from the USB-to-linux keycode mapping.
uint32_t linux_xkb_code_to_usb[] = {
  // 00-04: unused
  0x000000, 0x000000, 0x000000, 0x000000,
  // 04-07: unused
  0x000000, 0x000000, 0x000000, 0x000000,
  // 08-0b: unused  Escape  1!  2@
  0x000000, 0x070029, 0x07001e, 0x07001f,
  // 0c-0f: 3#  4$  5%  6^
  0x070020, 0x070021, 0x070022, 0x070023,
  // 10-13: 7&  8*  9(  0)
  0x070024, 0x070025, 0x070026, 0x070027,
  // 14-17: -_  =+  Backspace Tab
  0x07002d, 0x07002e, 0x07002a, 0x07002b,
  // 18-1b: qQ  wW  eE  rR
  0x070014, 0x07001a, 0x070008, 0x070015,
  // 1c-1f: tT  yY  uU  iI
  0x070017, 0x07001c, 0x070018, 0x07000c,
  // 20-23: oO  pP  [{  ]}
  0x070012, 0x070013, 0x07002f, 0x070030,
  // 24-27: Return  LeftControl  aA  sS
  0x070028, 0x0700e0, 0x070004, 0x070016,
  // 28-2b: dD  fF  gG  hH
  0x070007, 0x070009, 0x07000a, 0x07000b,
  // 2c-2f: jJ  kK  lL  ;:
  0x07000d, 0x07000e, 0x07000f, 0x070033,
  // 30-33: '"  `~  LeftShift   \|
  0x070034, 0x070035, 0x0700e1, 0x070031,
  // 34-37: zZ  xX  cC  vV
  0x07001d, 0x07001b, 0x070006, 0x070019,
  // 38-3b: bB  nN  mM  ,<
  0x070005, 0x070011, 0x070010, 0x070036,
  // 3c-3f: .>  /?  RightShift  Keypad_*
  0x070037, 0x070038,  0x0700e5, 0x070055,
  // 40-43: LeftAlt  Space  CapsLock  F1
  0x0700e2, 0x07002c, 0x070039, 0x07003a,
  // 44-47: F2  F3  F4  F5
  0x07003b, 0x07003c, 0x07003d, 0x07003e,
  // 48-4b: F6  F7  F8  F9
  0x07003f, 0x070040, 0x070041, 0x070042,
  // 4c-4f: F10  NumLock  ScrollLock  Keypad_7
  0x070043, 0x070053, 0x070047, 0x07005f,
  // 50-53: Keypad_8  Keypad_9  Keypad_-  Keypad_4
  0x070060, 0x070061, 0x070056, 0x07005c,
  // 54-57: Keypad_5  Keypad_6  Keypad_+  Keypad_1
  0x07005d, 0x07005e, 0x070057, 0x070059,
  // 58-5b: Keypad_2  Keypad_3  Keypad_0  Keypad_.
  0x07005a, 0x07005b, 0x070062, 0x070063,
  // 5c-5f: ISO_Level3_Shift  unused  <>||  F11
  // For 5e, consider USB#0064 : Non-US \ and |
  0x000000, 0x000000, 0x000000, 0x070044,
  // 60-63: F12  unused  Katakana  Hiragana
  0x070045, 0x000000, 0x070092, 0x070093,
  // 64-67: Henkan  Hiragana_Katakana  Muhenkan  unused
  0x070094, 0x070088, 0x07008b, 0x000000,
  // 68-6b: Keypad_Enter  RightControl  Keypad_/  Print/SysReq
  // For 6b, USB#0046 is PrintScreen, USB#009a is SysReq/Attention
  0x070058, 0x0700e4, 0x070054, 0x070046,
  // 6c-6f: RightAlt  Linefeed  Home  UpArrow
  0x0700e6, 0x000000, 0x07004a, 0x070052,
  // 70-73: PageUp/Prior  LeftArrow  RightArrow  End
  0x07004b, 0x070050, 0x07004f, 0x07004d,
  // 74-77: DownArrow  PageDown/Next  Insert  Delete/ForwardDelete
  0x070051, 0x07004e, 0x070049, 0x07004c,
  // 78-7b: unused  Mute  VolumeDown  VolumeUp
  0x000000, 0x07007f, 0x070081, 0x070080,
  // 7c-7f: PowerOff  Keypad_=  Keypad_+-  Pause/Break
  0x070066, 0x070067, 0x0700d7, 0x070048,
  // 80-83: LaunchA  Keypad_Decimal  HangulToggle  HanjaConversion
  // For 81, cf. 5b: Keypad_.
  0x000000, 0x0700dc, 0x070090, 0x070091,
  // 84-87: unused  LeftSuper/LeftWin  RightSuper/RightWin  Menu
  0x000000, 0x0700e3, 0x0700e7, 0x070065,
  // 88-8b: Cancel  Again/Redo  CrSel/Props  Undo
  0x07009b, 0x070079, 0x0700a3, 0x07007a,
  // 8c-8f: SunFront  Copy  SunOpen  Paste
  0x000000, 0x07007c, 0x000000, 0x07007d,
  // 90-93: Find  Cut  Help  MenuKB
  // For 93, cf. 87 (Menu) and c1 (MenuKB)
  0x07007e, 0x07007b, 0x070075, 0x000000,
  // 94-97: AL_Calculator  unused  Sleep  WakeUp
  0x0c0192, 0x000000, 0x000000, 0x000000,
  // 98-9b: Explorer  Send  unused  Xfer
  0x000000, 0x000000, 0x000000, 0x000000,
  // 9c-9f: Launch1  Launch2  WWW  DOS
  0x000000, 0x000000, 0x000000, 0x000000,
  // a0-a3: ScreenSaver  unused  RotateWindows  Mail
  0x000000, 0x000000, 0x000000, 0x000000,
  // a4-a7: Favorites  MyComputer  Back  Forward
  0x000000, 0x000000, 0x000000, 0x000000,
  // a8-ab: unused  Eject  Eject  AudioNext
  0x000000, 0x000000, 0x000000, 0x000000,
  // ac-af: AudioPlay AudioPrev AudioStop AudioRecord
  0x000000, 0x000000, 0x000000, 0x000000,
  // b0-b3: AudioRewind  Phone  unused  Tools
  0x000000, 0x000000, 0x000000, 0x000000,
  // b4-b7: HomePage  Reload  Close  unused
  0x000000, 0x000000, 0x000000, 0x000000,
  // b8-bb: unused  ScrollUp  ScrollDown  (
  // For bb, mapped to Keypad_(
  0x000000, 0x000000, 0x000000, 0x0700b6,
  // bc-bf: )  New  Redo  Tools
  // For bc, mapped to Keypad_)
  0x0700b7, 0x000000, 0x000000, 0x000000,
  // c0-c3: Launch5  MenuKB  unused  unused
  0x000000, 0x000000, 0x000000, 0x000000,
  // c4-c7: unused
  0x000000, 0x000000, 0x000000, 0x000000,
  // c8-cb: TouchpadToggle  unused  unused  ModeSwitch
  0x000000, 0x000000, 0x000000, 0x000000,
  // cc-cf: Generates no symbol without Shift.
  // With shift: LeftAlt LeftMeta LeftSuper LeftHyper.
  0x000000, 0x000000, 0x000000, 0x000000,
  // d0-d3: AudioPlay  AudioPause  Launch3  Launch4
  0x000000, 0x000000, 0x000000, 0x000000,
  // d4-d7: LaunchB  Suspend  Close  AudioPlay
  0x000000, 0x000000, 0x000000, 0x000000,
  // d8-db: AudioForward  unused  Print  unused
  0x000000, 0x000000, 0x000000, 0x000000,
  // dc-df: WebCam  unused  unused  Mail
  0x000000, 0x000000, 0x000000, 0x000000,
  // e0-e3: unused  Search  unused  AL_Finance
  0x000000, 0x000000, 0x000000, 0x0c0191,
  // e4-e7: unused  Shop  unused  Cancel
  0x000000, 0x000000, 0x000000, 0x000000,
  // e8-eb: MonBrightnessDown  MonBrightnessUp  AudioMedia  Display
  0x000000, 0x000000, 0x000000, 0x000000,
  // ec-ef: KbdLightOnOff  KbdBightnessDown  KdbBrightnessUp  Send
  0x000000, 0x000000, 0x000000, 0x000000,
  // f0-f3: Reply  MailForward  Save  Documents
  0x000000, 0x000000, 0x000000, 0x000000,
  // f4-f7: Battery  Bluetooth  WLAN  unused
  0x000000, 0x000000, 0x000000, 0x000000,
  // f8-fb: unused
  0x000000, 0x000000, 0x000000, 0x000000,
  // fc-ff: unused
  0x000000, 0x000000, 0x000000, 0x000000
};

COMPILE_ASSERT(arraysize(linux_xkb_code_to_usb) == 256,
               linux_xkb_code_to_usb_table_incorrect_size);

} // anonymous namespace

uint32_t UsbKeyCodeForKeyboardEvent(const WebKeyboardEvent& key_event) {
  // TODO(garykac): This code assumes that on Linux we're receiving events via
  // the XKB driver.  We should detect between "XKB", "kbd" and "evdev" at
  // run-time and re-map accordingly, but that's not possible here, inside the
  // sandbox.
  if (key_event.nativeKeyCode < 0 || key_event.nativeKeyCode > 255) {
    return 0;
  }
  return linux_xkb_code_to_usb[key_event.nativeKeyCode];
}

}  // namespace ppapi
}  // namespace webkit
