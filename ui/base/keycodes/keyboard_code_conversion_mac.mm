// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/keycodes/keyboard_code_conversion_mac.h"

#include <algorithm>
#import <Carbon/Carbon.h>

#include "base/logging.h"

namespace ui {

namespace {

// A struct to hold a Windows keycode to Mac virtual keycode mapping.
struct KeyCodeMap {
  KeyboardCode keycode;
  int macKeycode;
  unichar characterIgnoringModifiers;
};

// Customized less operator for using std::lower_bound() on a KeyCodeMap array.
bool operator<(const KeyCodeMap& a, const KeyCodeMap& b) {
  return a.keycode < b.keycode;
}

// This array must keep sorted ascending according to the value of |keycode|,
// so that we can binary search it.
// TODO(suzhe): This map is not complete, missing entries have macKeycode == -1.
const KeyCodeMap kKeyCodesMap[] = {
  { VKEY_BACK /* 0x08 */, kVK_Delete, kBackspaceCharCode },
  { VKEY_TAB /* 0x09 */, kVK_Tab, kTabCharCode },
  { VKEY_CLEAR /* 0x0C */, kVK_ANSI_KeypadClear, kClearCharCode },
  { VKEY_RETURN /* 0x0D */, kVK_Return, kReturnCharCode },
  { VKEY_SHIFT /* 0x10 */, kVK_Shift, 0 },
  { VKEY_CONTROL /* 0x11 */, kVK_Control, 0 },
  { VKEY_MENU /* 0x12 */, kVK_Option, 0 },
  { VKEY_PAUSE /* 0x13 */, -1, NSPauseFunctionKey },
  { VKEY_CAPITAL /* 0x14 */, kVK_CapsLock, 0 },
  { VKEY_KANA /* 0x15 */, kVK_JIS_Kana, 0 },
  { VKEY_HANGUL /* 0x15 */, -1, 0 },
  { VKEY_JUNJA /* 0x17 */, -1, 0 },
  { VKEY_FINAL /* 0x18 */, -1, 0 },
  { VKEY_HANJA /* 0x19 */, -1, 0 },
  { VKEY_KANJI /* 0x19 */, -1, 0 },
  { VKEY_ESCAPE /* 0x1B */, kVK_Escape, kEscapeCharCode },
  { VKEY_CONVERT /* 0x1C */, -1, 0 },
  { VKEY_NONCONVERT /* 0x1D */, -1, 0 },
  { VKEY_ACCEPT /* 0x1E */, -1, 0 },
  { VKEY_MODECHANGE /* 0x1F */, -1, 0 },
  { VKEY_SPACE /* 0x20 */, kVK_Space, kSpaceCharCode },
  { VKEY_PRIOR /* 0x21 */, kVK_PageUp, NSPageUpFunctionKey },
  { VKEY_NEXT /* 0x22 */, kVK_PageDown, NSPageDownFunctionKey },
  { VKEY_END /* 0x23 */, kVK_End, NSEndFunctionKey },
  { VKEY_HOME /* 0x24 */, kVK_Home, NSHomeFunctionKey },
  { VKEY_LEFT /* 0x25 */, kVK_LeftArrow, NSLeftArrowFunctionKey },
  { VKEY_UP /* 0x26 */, kVK_UpArrow, NSUpArrowFunctionKey },
  { VKEY_RIGHT /* 0x27 */, kVK_RightArrow, NSRightArrowFunctionKey },
  { VKEY_DOWN /* 0x28 */, kVK_DownArrow, NSDownArrowFunctionKey },
  { VKEY_SELECT /* 0x29 */, -1, 0 },
  { VKEY_PRINT /* 0x2A */, -1, NSPrintFunctionKey },
  { VKEY_EXECUTE /* 0x2B */, -1, NSExecuteFunctionKey },
  { VKEY_SNAPSHOT /* 0x2C */, -1, NSPrintScreenFunctionKey },
  { VKEY_INSERT /* 0x2D */, -1, NSInsertFunctionKey },
  { VKEY_DELETE /* 0x2E */, kVK_ForwardDelete, kDeleteCharCode },
  { VKEY_HELP /* 0x2F */, kVK_Help, kHelpCharCode },
  { VKEY_0 /* 0x30 */, kVK_ANSI_0, '0' },
  { VKEY_1 /* 0x31 */, kVK_ANSI_1, '1' },
  { VKEY_2 /* 0x32 */, kVK_ANSI_2, '2' },
  { VKEY_3 /* 0x33 */, kVK_ANSI_3, '3' },
  { VKEY_4 /* 0x34 */, kVK_ANSI_4, '4' },
  { VKEY_5 /* 0x35 */, kVK_ANSI_5, '5' },
  { VKEY_6 /* 0x36 */, kVK_ANSI_6, '6' },
  { VKEY_7 /* 0x37 */, kVK_ANSI_7, '7' },
  { VKEY_8 /* 0x38 */, kVK_ANSI_8, '8' },
  { VKEY_9 /* 0x39 */, kVK_ANSI_9, '9' },
  { VKEY_A /* 0x41 */, kVK_ANSI_A, 'a' },
  { VKEY_B /* 0x42 */, kVK_ANSI_B, 'b' },
  { VKEY_C /* 0x43 */, kVK_ANSI_C, 'c' },
  { VKEY_D /* 0x44 */, kVK_ANSI_D, 'd' },
  { VKEY_E /* 0x45 */, kVK_ANSI_E, 'e' },
  { VKEY_F /* 0x46 */, kVK_ANSI_F, 'f' },
  { VKEY_G /* 0x47 */, kVK_ANSI_G, 'g' },
  { VKEY_H /* 0x48 */, kVK_ANSI_H, 'h' },
  { VKEY_I /* 0x49 */, kVK_ANSI_I, 'i' },
  { VKEY_J /* 0x4A */, kVK_ANSI_J, 'j' },
  { VKEY_K /* 0x4B */, kVK_ANSI_K, 'k' },
  { VKEY_L /* 0x4C */, kVK_ANSI_L, 'l' },
  { VKEY_M /* 0x4D */, kVK_ANSI_M, 'm' },
  { VKEY_N /* 0x4E */, kVK_ANSI_N, 'n' },
  { VKEY_O /* 0x4F */, kVK_ANSI_O, 'o' },
  { VKEY_P /* 0x50 */, kVK_ANSI_P, 'p' },
  { VKEY_Q /* 0x51 */, kVK_ANSI_Q, 'q' },
  { VKEY_R /* 0x52 */, kVK_ANSI_R, 'r' },
  { VKEY_S /* 0x53 */, kVK_ANSI_S, 's' },
  { VKEY_T /* 0x54 */, kVK_ANSI_T, 't' },
  { VKEY_U /* 0x55 */, kVK_ANSI_U, 'u' },
  { VKEY_V /* 0x56 */, kVK_ANSI_V, 'v' },
  { VKEY_W /* 0x57 */, kVK_ANSI_W, 'w' },
  { VKEY_X /* 0x58 */, kVK_ANSI_X, 'x' },
  { VKEY_Y /* 0x59 */, kVK_ANSI_Y, 'y' },
  { VKEY_Z /* 0x5A */, kVK_ANSI_Z, 'z' },
  { VKEY_LWIN /* 0x5B */, kVK_Command, 0 },
  { VKEY_RWIN /* 0x5C */, 0x36, 0 },
  { VKEY_APPS /* 0x5D */, 0x36, 0 },
  { VKEY_SLEEP /* 0x5F */, -1, 0 },
  { VKEY_NUMPAD0 /* 0x60 */, kVK_ANSI_Keypad0, '0' },
  { VKEY_NUMPAD1 /* 0x61 */, kVK_ANSI_Keypad1, '1' },
  { VKEY_NUMPAD2 /* 0x62 */, kVK_ANSI_Keypad2, '2' },
  { VKEY_NUMPAD3 /* 0x63 */, kVK_ANSI_Keypad3, '3' },
  { VKEY_NUMPAD4 /* 0x64 */, kVK_ANSI_Keypad4, '4' },
  { VKEY_NUMPAD5 /* 0x65 */, kVK_ANSI_Keypad5, '5' },
  { VKEY_NUMPAD6 /* 0x66 */, kVK_ANSI_Keypad6, '6' },
  { VKEY_NUMPAD7 /* 0x67 */, kVK_ANSI_Keypad7, '7' },
  { VKEY_NUMPAD8 /* 0x68 */, kVK_ANSI_Keypad8, '8' },
  { VKEY_NUMPAD9 /* 0x69 */, kVK_ANSI_Keypad9, '9' },
  { VKEY_MULTIPLY /* 0x6A */, kVK_ANSI_KeypadMultiply, '*' },
  { VKEY_ADD /* 0x6B */, kVK_ANSI_KeypadPlus, '+' },
  { VKEY_SEPARATOR /* 0x6C */, -1, 0 },
  { VKEY_SUBTRACT /* 0x6D */, kVK_ANSI_KeypadMinus, '-' },
  { VKEY_DECIMAL /* 0x6E */, kVK_ANSI_KeypadDecimal, '.' },
  { VKEY_DIVIDE /* 0x6F */, kVK_ANSI_KeypadDivide, '/' },
  { VKEY_F1 /* 0x70 */, kVK_F1, NSF1FunctionKey },
  { VKEY_F2 /* 0x71 */, kVK_F2, NSF2FunctionKey },
  { VKEY_F3 /* 0x72 */, kVK_F3, NSF3FunctionKey },
  { VKEY_F4 /* 0x73 */, kVK_F4, NSF4FunctionKey },
  { VKEY_F5 /* 0x74 */, kVK_F5, NSF5FunctionKey },
  { VKEY_F6 /* 0x75 */, kVK_F6, NSF6FunctionKey },
  { VKEY_F7 /* 0x76 */, kVK_F7, NSF7FunctionKey },
  { VKEY_F8 /* 0x77 */, kVK_F8, NSF8FunctionKey },
  { VKEY_F9 /* 0x78 */, kVK_F9, NSF9FunctionKey },
  { VKEY_F10 /* 0x79 */, kVK_F10, NSF10FunctionKey },
  { VKEY_F11 /* 0x7A */, kVK_F11, NSF11FunctionKey },
  { VKEY_F12 /* 0x7B */, kVK_F12, NSF12FunctionKey },
  { VKEY_F13 /* 0x7C */, kVK_F13, NSF13FunctionKey },
  { VKEY_F14 /* 0x7D */, kVK_F14, NSF14FunctionKey },
  { VKEY_F15 /* 0x7E */, kVK_F15, NSF15FunctionKey },
  { VKEY_F16 /* 0x7F */, kVK_F16, NSF16FunctionKey },
  { VKEY_F17 /* 0x80 */, kVK_F17, NSF17FunctionKey },
  { VKEY_F18 /* 0x81 */, kVK_F18, NSF18FunctionKey },
  { VKEY_F19 /* 0x82 */, kVK_F19, NSF19FunctionKey },
  { VKEY_F20 /* 0x83 */, kVK_F20, NSF20FunctionKey },
  { VKEY_F21 /* 0x84 */, -1, NSF21FunctionKey },
  { VKEY_F22 /* 0x85 */, -1, NSF22FunctionKey },
  { VKEY_F23 /* 0x86 */, -1, NSF23FunctionKey },
  { VKEY_F24 /* 0x87 */, -1, NSF24FunctionKey },
  { VKEY_NUMLOCK /* 0x90 */, -1, 0 },
  { VKEY_SCROLL /* 0x91 */, -1, NSScrollLockFunctionKey },
  { VKEY_LSHIFT /* 0xA0 */, kVK_Shift, 0 },
  { VKEY_RSHIFT /* 0xA1 */, kVK_Shift, 0 },
  { VKEY_LCONTROL /* 0xA2 */, kVK_Control, 0 },
  { VKEY_RCONTROL /* 0xA3 */, kVK_Control, 0 },
  { VKEY_LMENU /* 0xA4 */, -1, 0 },
  { VKEY_RMENU /* 0xA5 */, -1, 0 },
  { VKEY_BROWSER_BACK /* 0xA6 */, -1, 0 },
  { VKEY_BROWSER_FORWARD /* 0xA7 */, -1, 0 },
  { VKEY_BROWSER_REFRESH /* 0xA8 */, -1, 0 },
  { VKEY_BROWSER_STOP /* 0xA9 */, -1, 0 },
  { VKEY_BROWSER_SEARCH /* 0xAA */, -1, 0 },
  { VKEY_BROWSER_FAVORITES /* 0xAB */, -1, 0 },
  { VKEY_BROWSER_HOME /* 0xAC */, -1, 0 },
  { VKEY_VOLUME_MUTE /* 0xAD */, -1, 0 },
  { VKEY_VOLUME_DOWN /* 0xAE */, -1, 0 },
  { VKEY_VOLUME_UP /* 0xAF */, -1, 0 },
  { VKEY_MEDIA_NEXT_TRACK /* 0xB0 */, -1, 0 },
  { VKEY_MEDIA_PREV_TRACK /* 0xB1 */, -1, 0 },
  { VKEY_MEDIA_STOP /* 0xB2 */, -1, 0 },
  { VKEY_MEDIA_PLAY_PAUSE /* 0xB3 */, -1, 0 },
  { VKEY_MEDIA_LAUNCH_MAIL /* 0xB4 */, -1, 0 },
  { VKEY_MEDIA_LAUNCH_MEDIA_SELECT /* 0xB5 */, -1, 0 },
  { VKEY_MEDIA_LAUNCH_APP1 /* 0xB6 */, -1, 0 },
  { VKEY_MEDIA_LAUNCH_APP2 /* 0xB7 */, -1, 0 },
  { VKEY_OEM_1 /* 0xBA */, kVK_ANSI_Semicolon, ';' },
  { VKEY_OEM_PLUS /* 0xBB */, kVK_ANSI_Equal, '=' },
  { VKEY_OEM_COMMA /* 0xBC */, kVK_ANSI_Comma, ',' },
  { VKEY_OEM_MINUS /* 0xBD */, kVK_ANSI_Minus, '-' },
  { VKEY_OEM_PERIOD /* 0xBE */, kVK_ANSI_Period, '.' },
  { VKEY_OEM_2 /* 0xBF */, kVK_ANSI_Slash, '/' },
  { VKEY_OEM_3 /* 0xC0 */, kVK_ANSI_Grave, '`' },
  { VKEY_OEM_4 /* 0xDB */, kVK_ANSI_LeftBracket, '[' },
  { VKEY_OEM_5 /* 0xDC */, kVK_ANSI_Backslash, '\\' },
  { VKEY_OEM_6 /* 0xDD */, kVK_ANSI_RightBracket, ']' },
  { VKEY_OEM_7 /* 0xDE */, kVK_ANSI_Quote, '\'' },
  { VKEY_OEM_8 /* 0xDF */, -1, 0 },
  { VKEY_OEM_102 /* 0xE2 */, -1, 0 },
  { VKEY_PROCESSKEY /* 0xE5 */, -1, 0 },
  { VKEY_PACKET /* 0xE7 */, -1, 0 },
  { VKEY_ATTN /* 0xF6 */, -1, 0 },
  { VKEY_CRSEL /* 0xF7 */, -1, 0 },
  { VKEY_EXSEL /* 0xF8 */, -1, 0 },
  { VKEY_EREOF /* 0xF9 */, -1, 0 },
  { VKEY_PLAY /* 0xFA */, -1, 0 },
  { VKEY_ZOOM /* 0xFB */, -1, 0 },
  { VKEY_NONAME /* 0xFC */, -1, 0 },
  { VKEY_PA1 /* 0xFD */, -1, 0 },
  { VKEY_OEM_CLEAR /* 0xFE */, kVK_ANSI_KeypadClear, kClearCharCode }
};

// A convenient array for getting symbol characters on the number keys.
const char kShiftCharsForNumberKeys[] = ")!@#$%^&*(";

}  // anonymous namespace

int MacKeyCodeForWindowsKeyCode(KeyboardCode keycode,
                                NSUInteger flags,
                                unichar* character,
                                unichar* characterIgnoringModifiers) {
  KeyCodeMap from;
  from.keycode = keycode;

  const KeyCodeMap* ptr = std::lower_bound(
      kKeyCodesMap, kKeyCodesMap + arraysize(kKeyCodesMap), from);

  if (ptr >= kKeyCodesMap + arraysize(kKeyCodesMap) ||
      ptr->keycode != keycode || ptr->macKeycode == -1)
    return -1;

  int macKeycode = ptr->macKeycode;
  if (characterIgnoringModifiers)
    *characterIgnoringModifiers = ptr->characterIgnoringModifiers;

  if (!character)
    return macKeycode;

  *character = ptr->characterIgnoringModifiers;

  // Fill in |character| according to flags.
  if (flags & NSShiftKeyMask) {
    if (keycode >= VKEY_0 && keycode <= VKEY_9) {
      *character = kShiftCharsForNumberKeys[keycode - VKEY_0];
    } else if (keycode >= VKEY_A && keycode <= VKEY_Z) {
      *character = 'A' + (keycode - VKEY_A);
    } else {
      switch (macKeycode) {
        case kVK_ANSI_Grave:
          *character = '~';
          break;
        case kVK_ANSI_Minus:
          *character = '_';
          break;
        case kVK_ANSI_Equal:
          *character = '+';
          break;
        case kVK_ANSI_LeftBracket:
          *character = '{';
          break;
        case kVK_ANSI_RightBracket:
          *character = '}';
          break;
        case kVK_ANSI_Backslash:
          *character = '|';
          break;
        case kVK_ANSI_Semicolon:
          *character = ':';
          break;
        case kVK_ANSI_Quote:
          *character = '\"';
          break;
        case kVK_ANSI_Comma:
          *character = '<';
          break;
        case kVK_ANSI_Period:
          *character = '>';
          break;
        case kVK_ANSI_Slash:
          *character = '?';
          break;
        default:
          break;
      }
    }
  }

  // Control characters.
  if (flags & NSControlKeyMask) {
    if (keycode >= VKEY_A && keycode <= VKEY_Z)
      *character = 1 + keycode - VKEY_A;
    else if (macKeycode == kVK_ANSI_LeftBracket)
      *character = 27;
    else if (macKeycode == kVK_ANSI_Backslash)
      *character = 28;
    else if (macKeycode == kVK_ANSI_RightBracket)
      *character = 29;
  }

  // TODO(suzhe): Support characters for Option key bindings.
  return macKeycode;
}

}  // namespace ui
