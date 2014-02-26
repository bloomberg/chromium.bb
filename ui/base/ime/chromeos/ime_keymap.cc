// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/ime_keymap.h"

#define XK_MISCELLANY
#include <X11/keysymdef.h>
#include <X11/XF86keysym.h>

namespace ui {

std::string FromXKeycodeToKeyValue(int keyval) {
  // TODO: Ensure all keys are supported.
  switch (keyval) {
    case XK_Escape:
      return "Esc";
    case XK_F1:
    case XF86XK_Back:
      return "HistoryBack";
    case XK_F2:
    case XF86XK_Forward:
      return "HistoryForward";
    case XK_F3:
    case XF86XK_Reload:
      return "BrowserRefresh";
    case XK_F4:
    case XF86XK_LaunchB:
      return "ChromeOSFullscreen";  // TODO: Check this value
    case XK_F5:
    case XF86XK_LaunchA:
      return "ChromeOSSwitchWindow";  // TODO: Check this value
    case XK_F6:
    case XF86XK_MonBrightnessDown:
      return "BrightnessDown";
    case XK_F7:
    case XF86XK_MonBrightnessUp:
      return "BrightnessUp";
    case XK_F8:
    case XF86XK_AudioMute:
      return "AudioVolumeMute";
    case XK_F9:
    case XF86XK_AudioLowerVolume:
      return "AudioVolumeDown";
    case XK_F10:
    case XF86XK_AudioRaiseVolume:
      return "AudioVolumeUp";
    case XK_BackSpace:
      return "Backspace";
    case XK_Delete:
    case XK_KP_Delete:
      return "Delete";
    case XK_Tab:
      return "Tab";
    case XK_KP_Enter:
    case XK_Return:
      return "Enter";
    case XK_Meta_L:
      return "BrowserSearch";
    case XK_Up:
    case XK_KP_Up:
      return "Up";
    case XK_Down:
    case XK_KP_Down:
      return "Down";
    case XK_Left:
    case XK_KP_Left:
      return "Left";
    case XK_Right:
    case XK_KP_Right:
      return "Right";
    case XK_Page_Up:
      return "PageUp";
    case XK_Page_Down:
      return "PageDown";
    case XK_Home:
      return "Home";
    case XK_End:
      return "End";
    case XK_Shift_L:
    case XK_Shift_R:
      return "Shift";
    case XK_Alt_L:
    case XK_Alt_R:
      return "Alt";
    case XK_Control_L:
    case XK_Control_R:
      return "Ctrl";
    case XK_Caps_Lock:
      return "CapsLock";
    default: {
      // TODO: Properly support unicode characters.
      char value[2];
      value[0] = keyval;
      value[1] = '\0';
      return value;
    }
  }
}

}  // namespace ui
