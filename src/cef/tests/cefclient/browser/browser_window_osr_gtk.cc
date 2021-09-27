// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/browser_window_osr_gtk.h"

#include <GL/gl.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms-compat.h>
#include <gdk/gdkx.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#define XK_3270  // for XK_3270_BackTab
#include <X11/XF86keysym.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/keysym.h>

#include <algorithm>

#include "include/base/cef_cxx17_backports.h"
#include "include/base/cef_logging.h"
#include "include/base/cef_macros.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/cefclient/browser/util_gtk.h"
#include "tests/shared/browser/geometry_util.h"
#include "tests/shared/browser/main_message_loop.h"

namespace client {

namespace {

// Static BrowserWindowOsrGtk::EventFilter needs to forward touch events
// to correct browser, so we maintain a vector of all windows.
std::vector<BrowserWindowOsrGtk*> g_browser_windows;

int GetCefStateModifiers(guint state) {
  int modifiers = 0;
  if (state & GDK_SHIFT_MASK)
    modifiers |= EVENTFLAG_SHIFT_DOWN;
  if (state & GDK_LOCK_MASK)
    modifiers |= EVENTFLAG_CAPS_LOCK_ON;
  if (state & GDK_CONTROL_MASK)
    modifiers |= EVENTFLAG_CONTROL_DOWN;
  if (state & GDK_MOD1_MASK)
    modifiers |= EVENTFLAG_ALT_DOWN;
  if (state & GDK_BUTTON1_MASK)
    modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
  if (state & GDK_BUTTON2_MASK)
    modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
  if (state & GDK_BUTTON3_MASK)
    modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
  return modifiers;
}

// From ui/events/keycodes/keyboard_codes_posix.h.
enum KeyboardCode {
  VKEY_BACK = 0x08,
  VKEY_TAB = 0x09,
  VKEY_BACKTAB = 0x0A,
  VKEY_CLEAR = 0x0C,
  VKEY_RETURN = 0x0D,
  VKEY_SHIFT = 0x10,
  VKEY_CONTROL = 0x11,
  VKEY_MENU = 0x12,
  VKEY_PAUSE = 0x13,
  VKEY_CAPITAL = 0x14,
  VKEY_KANA = 0x15,
  VKEY_HANGUL = 0x15,
  VKEY_JUNJA = 0x17,
  VKEY_FINAL = 0x18,
  VKEY_HANJA = 0x19,
  VKEY_KANJI = 0x19,
  VKEY_ESCAPE = 0x1B,
  VKEY_CONVERT = 0x1C,
  VKEY_NONCONVERT = 0x1D,
  VKEY_ACCEPT = 0x1E,
  VKEY_MODECHANGE = 0x1F,
  VKEY_SPACE = 0x20,
  VKEY_PRIOR = 0x21,
  VKEY_NEXT = 0x22,
  VKEY_END = 0x23,
  VKEY_HOME = 0x24,
  VKEY_LEFT = 0x25,
  VKEY_UP = 0x26,
  VKEY_RIGHT = 0x27,
  VKEY_DOWN = 0x28,
  VKEY_SELECT = 0x29,
  VKEY_PRINT = 0x2A,
  VKEY_EXECUTE = 0x2B,
  VKEY_SNAPSHOT = 0x2C,
  VKEY_INSERT = 0x2D,
  VKEY_DELETE = 0x2E,
  VKEY_HELP = 0x2F,
  VKEY_0 = 0x30,
  VKEY_1 = 0x31,
  VKEY_2 = 0x32,
  VKEY_3 = 0x33,
  VKEY_4 = 0x34,
  VKEY_5 = 0x35,
  VKEY_6 = 0x36,
  VKEY_7 = 0x37,
  VKEY_8 = 0x38,
  VKEY_9 = 0x39,
  VKEY_A = 0x41,
  VKEY_B = 0x42,
  VKEY_C = 0x43,
  VKEY_D = 0x44,
  VKEY_E = 0x45,
  VKEY_F = 0x46,
  VKEY_G = 0x47,
  VKEY_H = 0x48,
  VKEY_I = 0x49,
  VKEY_J = 0x4A,
  VKEY_K = 0x4B,
  VKEY_L = 0x4C,
  VKEY_M = 0x4D,
  VKEY_N = 0x4E,
  VKEY_O = 0x4F,
  VKEY_P = 0x50,
  VKEY_Q = 0x51,
  VKEY_R = 0x52,
  VKEY_S = 0x53,
  VKEY_T = 0x54,
  VKEY_U = 0x55,
  VKEY_V = 0x56,
  VKEY_W = 0x57,
  VKEY_X = 0x58,
  VKEY_Y = 0x59,
  VKEY_Z = 0x5A,
  VKEY_LWIN = 0x5B,
  VKEY_COMMAND = VKEY_LWIN,  // Provide the Mac name for convenience.
  VKEY_RWIN = 0x5C,
  VKEY_APPS = 0x5D,
  VKEY_SLEEP = 0x5F,
  VKEY_NUMPAD0 = 0x60,
  VKEY_NUMPAD1 = 0x61,
  VKEY_NUMPAD2 = 0x62,
  VKEY_NUMPAD3 = 0x63,
  VKEY_NUMPAD4 = 0x64,
  VKEY_NUMPAD5 = 0x65,
  VKEY_NUMPAD6 = 0x66,
  VKEY_NUMPAD7 = 0x67,
  VKEY_NUMPAD8 = 0x68,
  VKEY_NUMPAD9 = 0x69,
  VKEY_MULTIPLY = 0x6A,
  VKEY_ADD = 0x6B,
  VKEY_SEPARATOR = 0x6C,
  VKEY_SUBTRACT = 0x6D,
  VKEY_DECIMAL = 0x6E,
  VKEY_DIVIDE = 0x6F,
  VKEY_F1 = 0x70,
  VKEY_F2 = 0x71,
  VKEY_F3 = 0x72,
  VKEY_F4 = 0x73,
  VKEY_F5 = 0x74,
  VKEY_F6 = 0x75,
  VKEY_F7 = 0x76,
  VKEY_F8 = 0x77,
  VKEY_F9 = 0x78,
  VKEY_F10 = 0x79,
  VKEY_F11 = 0x7A,
  VKEY_F12 = 0x7B,
  VKEY_F13 = 0x7C,
  VKEY_F14 = 0x7D,
  VKEY_F15 = 0x7E,
  VKEY_F16 = 0x7F,
  VKEY_F17 = 0x80,
  VKEY_F18 = 0x81,
  VKEY_F19 = 0x82,
  VKEY_F20 = 0x83,
  VKEY_F21 = 0x84,
  VKEY_F22 = 0x85,
  VKEY_F23 = 0x86,
  VKEY_F24 = 0x87,
  VKEY_NUMLOCK = 0x90,
  VKEY_SCROLL = 0x91,
  VKEY_LSHIFT = 0xA0,
  VKEY_RSHIFT = 0xA1,
  VKEY_LCONTROL = 0xA2,
  VKEY_RCONTROL = 0xA3,
  VKEY_LMENU = 0xA4,
  VKEY_RMENU = 0xA5,
  VKEY_BROWSER_BACK = 0xA6,
  VKEY_BROWSER_FORWARD = 0xA7,
  VKEY_BROWSER_REFRESH = 0xA8,
  VKEY_BROWSER_STOP = 0xA9,
  VKEY_BROWSER_SEARCH = 0xAA,
  VKEY_BROWSER_FAVORITES = 0xAB,
  VKEY_BROWSER_HOME = 0xAC,
  VKEY_VOLUME_MUTE = 0xAD,
  VKEY_VOLUME_DOWN = 0xAE,
  VKEY_VOLUME_UP = 0xAF,
  VKEY_MEDIA_NEXT_TRACK = 0xB0,
  VKEY_MEDIA_PREV_TRACK = 0xB1,
  VKEY_MEDIA_STOP = 0xB2,
  VKEY_MEDIA_PLAY_PAUSE = 0xB3,
  VKEY_MEDIA_LAUNCH_MAIL = 0xB4,
  VKEY_MEDIA_LAUNCH_MEDIA_SELECT = 0xB5,
  VKEY_MEDIA_LAUNCH_APP1 = 0xB6,
  VKEY_MEDIA_LAUNCH_APP2 = 0xB7,
  VKEY_OEM_1 = 0xBA,
  VKEY_OEM_PLUS = 0xBB,
  VKEY_OEM_COMMA = 0xBC,
  VKEY_OEM_MINUS = 0xBD,
  VKEY_OEM_PERIOD = 0xBE,
  VKEY_OEM_2 = 0xBF,
  VKEY_OEM_3 = 0xC0,
  VKEY_OEM_4 = 0xDB,
  VKEY_OEM_5 = 0xDC,
  VKEY_OEM_6 = 0xDD,
  VKEY_OEM_7 = 0xDE,
  VKEY_OEM_8 = 0xDF,
  VKEY_OEM_102 = 0xE2,
  VKEY_OEM_103 = 0xE3,  // GTV KEYCODE_MEDIA_REWIND
  VKEY_OEM_104 = 0xE4,  // GTV KEYCODE_MEDIA_FAST_FORWARD
  VKEY_PROCESSKEY = 0xE5,
  VKEY_PACKET = 0xE7,
  VKEY_DBE_SBCSCHAR = 0xF3,
  VKEY_DBE_DBCSCHAR = 0xF4,
  VKEY_ATTN = 0xF6,
  VKEY_CRSEL = 0xF7,
  VKEY_EXSEL = 0xF8,
  VKEY_EREOF = 0xF9,
  VKEY_PLAY = 0xFA,
  VKEY_ZOOM = 0xFB,
  VKEY_NONAME = 0xFC,
  VKEY_PA1 = 0xFD,
  VKEY_OEM_CLEAR = 0xFE,
  VKEY_UNKNOWN = 0,

  // POSIX specific VKEYs. Note that as of Windows SDK 7.1, 0x97-9F, 0xD8-DA,
  // and 0xE8 are unassigned.
  VKEY_WLAN = 0x97,
  VKEY_POWER = 0x98,
  VKEY_BRIGHTNESS_DOWN = 0xD8,
  VKEY_BRIGHTNESS_UP = 0xD9,
  VKEY_KBD_BRIGHTNESS_DOWN = 0xDA,
  VKEY_KBD_BRIGHTNESS_UP = 0xE8,

  // Windows does not have a specific key code for AltGr. We use the unused 0xE1
  // (VK_OEM_AX) code to represent AltGr, matching the behaviour of Firefox on
  // Linux.
  VKEY_ALTGR = 0xE1,
  // Windows does not have a specific key code for Compose. We use the unused
  // 0xE6 (VK_ICO_CLEAR) code to represent Compose.
  VKEY_COMPOSE = 0xE6,
};

// From ui/events/keycodes/keyboard_code_conversion_x.cc.
// Gdk key codes (e.g. GDK_BackSpace) and X keysyms (e.g. XK_BackSpace) share
// the same values.
KeyboardCode KeyboardCodeFromXKeysym(unsigned int keysym) {
  switch (keysym) {
    case XK_BackSpace:
      return VKEY_BACK;
    case XK_Delete:
    case XK_KP_Delete:
      return VKEY_DELETE;
    case XK_Tab:
    case XK_KP_Tab:
    case XK_ISO_Left_Tab:
    case XK_3270_BackTab:
      return VKEY_TAB;
    case XK_Linefeed:
    case XK_Return:
    case XK_KP_Enter:
    case XK_ISO_Enter:
      return VKEY_RETURN;
    case XK_Clear:
    case XK_KP_Begin:  // NumPad 5 without Num Lock, for crosbug.com/29169.
      return VKEY_CLEAR;
    case XK_KP_Space:
    case XK_space:
      return VKEY_SPACE;
    case XK_Home:
    case XK_KP_Home:
      return VKEY_HOME;
    case XK_End:
    case XK_KP_End:
      return VKEY_END;
    case XK_Page_Up:
    case XK_KP_Page_Up:  // aka XK_KP_Prior
      return VKEY_PRIOR;
    case XK_Page_Down:
    case XK_KP_Page_Down:  // aka XK_KP_Next
      return VKEY_NEXT;
    case XK_Left:
    case XK_KP_Left:
      return VKEY_LEFT;
    case XK_Right:
    case XK_KP_Right:
      return VKEY_RIGHT;
    case XK_Down:
    case XK_KP_Down:
      return VKEY_DOWN;
    case XK_Up:
    case XK_KP_Up:
      return VKEY_UP;
    case XK_Escape:
      return VKEY_ESCAPE;
    case XK_Kana_Lock:
    case XK_Kana_Shift:
      return VKEY_KANA;
    case XK_Hangul:
      return VKEY_HANGUL;
    case XK_Hangul_Hanja:
      return VKEY_HANJA;
    case XK_Kanji:
      return VKEY_KANJI;
    case XK_Henkan:
      return VKEY_CONVERT;
    case XK_Muhenkan:
      return VKEY_NONCONVERT;
    case XK_Zenkaku_Hankaku:
      return VKEY_DBE_DBCSCHAR;
    case XK_A:
    case XK_a:
      return VKEY_A;
    case XK_B:
    case XK_b:
      return VKEY_B;
    case XK_C:
    case XK_c:
      return VKEY_C;
    case XK_D:
    case XK_d:
      return VKEY_D;
    case XK_E:
    case XK_e:
      return VKEY_E;
    case XK_F:
    case XK_f:
      return VKEY_F;
    case XK_G:
    case XK_g:
      return VKEY_G;
    case XK_H:
    case XK_h:
      return VKEY_H;
    case XK_I:
    case XK_i:
      return VKEY_I;
    case XK_J:
    case XK_j:
      return VKEY_J;
    case XK_K:
    case XK_k:
      return VKEY_K;
    case XK_L:
    case XK_l:
      return VKEY_L;
    case XK_M:
    case XK_m:
      return VKEY_M;
    case XK_N:
    case XK_n:
      return VKEY_N;
    case XK_O:
    case XK_o:
      return VKEY_O;
    case XK_P:
    case XK_p:
      return VKEY_P;
    case XK_Q:
    case XK_q:
      return VKEY_Q;
    case XK_R:
    case XK_r:
      return VKEY_R;
    case XK_S:
    case XK_s:
      return VKEY_S;
    case XK_T:
    case XK_t:
      return VKEY_T;
    case XK_U:
    case XK_u:
      return VKEY_U;
    case XK_V:
    case XK_v:
      return VKEY_V;
    case XK_W:
    case XK_w:
      return VKEY_W;
    case XK_X:
    case XK_x:
      return VKEY_X;
    case XK_Y:
    case XK_y:
      return VKEY_Y;
    case XK_Z:
    case XK_z:
      return VKEY_Z;

    case XK_0:
    case XK_1:
    case XK_2:
    case XK_3:
    case XK_4:
    case XK_5:
    case XK_6:
    case XK_7:
    case XK_8:
    case XK_9:
      return static_cast<KeyboardCode>(VKEY_0 + (keysym - XK_0));

    case XK_parenright:
      return VKEY_0;
    case XK_exclam:
      return VKEY_1;
    case XK_at:
      return VKEY_2;
    case XK_numbersign:
      return VKEY_3;
    case XK_dollar:
      return VKEY_4;
    case XK_percent:
      return VKEY_5;
    case XK_asciicircum:
      return VKEY_6;
    case XK_ampersand:
      return VKEY_7;
    case XK_asterisk:
      return VKEY_8;
    case XK_parenleft:
      return VKEY_9;

    case XK_KP_0:
    case XK_KP_1:
    case XK_KP_2:
    case XK_KP_3:
    case XK_KP_4:
    case XK_KP_5:
    case XK_KP_6:
    case XK_KP_7:
    case XK_KP_8:
    case XK_KP_9:
      return static_cast<KeyboardCode>(VKEY_NUMPAD0 + (keysym - XK_KP_0));

    case XK_multiply:
    case XK_KP_Multiply:
      return VKEY_MULTIPLY;
    case XK_KP_Add:
      return VKEY_ADD;
    case XK_KP_Separator:
      return VKEY_SEPARATOR;
    case XK_KP_Subtract:
      return VKEY_SUBTRACT;
    case XK_KP_Decimal:
      return VKEY_DECIMAL;
    case XK_KP_Divide:
      return VKEY_DIVIDE;
    case XK_KP_Equal:
    case XK_equal:
    case XK_plus:
      return VKEY_OEM_PLUS;
    case XK_comma:
    case XK_less:
      return VKEY_OEM_COMMA;
    case XK_minus:
    case XK_underscore:
      return VKEY_OEM_MINUS;
    case XK_greater:
    case XK_period:
      return VKEY_OEM_PERIOD;
    case XK_colon:
    case XK_semicolon:
      return VKEY_OEM_1;
    case XK_question:
    case XK_slash:
      return VKEY_OEM_2;
    case XK_asciitilde:
    case XK_quoteleft:
      return VKEY_OEM_3;
    case XK_bracketleft:
    case XK_braceleft:
      return VKEY_OEM_4;
    case XK_backslash:
    case XK_bar:
      return VKEY_OEM_5;
    case XK_bracketright:
    case XK_braceright:
      return VKEY_OEM_6;
    case XK_quoteright:
    case XK_quotedbl:
      return VKEY_OEM_7;
    case XK_ISO_Level5_Shift:
      return VKEY_OEM_8;
    case XK_Shift_L:
    case XK_Shift_R:
      return VKEY_SHIFT;
    case XK_Control_L:
    case XK_Control_R:
      return VKEY_CONTROL;
    case XK_Meta_L:
    case XK_Meta_R:
    case XK_Alt_L:
    case XK_Alt_R:
      return VKEY_MENU;
    case XK_ISO_Level3_Shift:
      return VKEY_ALTGR;
    case XK_Multi_key:
      return VKEY_COMPOSE;
    case XK_Pause:
      return VKEY_PAUSE;
    case XK_Caps_Lock:
      return VKEY_CAPITAL;
    case XK_Num_Lock:
      return VKEY_NUMLOCK;
    case XK_Scroll_Lock:
      return VKEY_SCROLL;
    case XK_Select:
      return VKEY_SELECT;
    case XK_Print:
      return VKEY_PRINT;
    case XK_Execute:
      return VKEY_EXECUTE;
    case XK_Insert:
    case XK_KP_Insert:
      return VKEY_INSERT;
    case XK_Help:
      return VKEY_HELP;
    case XK_Super_L:
      return VKEY_LWIN;
    case XK_Super_R:
      return VKEY_RWIN;
    case XK_Menu:
      return VKEY_APPS;
    case XK_F1:
    case XK_F2:
    case XK_F3:
    case XK_F4:
    case XK_F5:
    case XK_F6:
    case XK_F7:
    case XK_F8:
    case XK_F9:
    case XK_F10:
    case XK_F11:
    case XK_F12:
    case XK_F13:
    case XK_F14:
    case XK_F15:
    case XK_F16:
    case XK_F17:
    case XK_F18:
    case XK_F19:
    case XK_F20:
    case XK_F21:
    case XK_F22:
    case XK_F23:
    case XK_F24:
      return static_cast<KeyboardCode>(VKEY_F1 + (keysym - XK_F1));
    case XK_KP_F1:
    case XK_KP_F2:
    case XK_KP_F3:
    case XK_KP_F4:
      return static_cast<KeyboardCode>(VKEY_F1 + (keysym - XK_KP_F1));

    case XK_guillemotleft:
    case XK_guillemotright:
    case XK_degree:
    // In the case of canadian multilingual keyboard layout, VKEY_OEM_102 is
    // assigned to ugrave key.
    case XK_ugrave:
    case XK_Ugrave:
    case XK_brokenbar:
      return VKEY_OEM_102;  // international backslash key in 102 keyboard.

    // When evdev is in use, /usr/share/X11/xkb/symbols/inet maps F13-18 keys
    // to the special XF86XK symbols to support Microsoft Ergonomic keyboards:
    // https://bugs.freedesktop.org/show_bug.cgi?id=5783
    // In Chrome, we map these X key symbols back to F13-18 since we don't have
    // VKEYs for these XF86XK symbols.
    case XF86XK_Tools:
      return VKEY_F13;
    case XF86XK_Launch5:
      return VKEY_F14;
    case XF86XK_Launch6:
      return VKEY_F15;
    case XF86XK_Launch7:
      return VKEY_F16;
    case XF86XK_Launch8:
      return VKEY_F17;
    case XF86XK_Launch9:
      return VKEY_F18;
    case XF86XK_Refresh:
    case XF86XK_History:
    case XF86XK_OpenURL:
    case XF86XK_AddFavorite:
    case XF86XK_Go:
    case XF86XK_ZoomIn:
    case XF86XK_ZoomOut:
      // ui::AcceleratorGtk tries to convert the XF86XK_ keysyms on Chrome
      // startup. It's safe to return VKEY_UNKNOWN here since ui::AcceleratorGtk
      // also checks a Gdk keysym. http://crbug.com/109843
      return VKEY_UNKNOWN;
    // For supporting multimedia buttons on a USB keyboard.
    case XF86XK_Back:
      return VKEY_BROWSER_BACK;
    case XF86XK_Forward:
      return VKEY_BROWSER_FORWARD;
    case XF86XK_Reload:
      return VKEY_BROWSER_REFRESH;
    case XF86XK_Stop:
      return VKEY_BROWSER_STOP;
    case XF86XK_Search:
      return VKEY_BROWSER_SEARCH;
    case XF86XK_Favorites:
      return VKEY_BROWSER_FAVORITES;
    case XF86XK_HomePage:
      return VKEY_BROWSER_HOME;
    case XF86XK_AudioMute:
      return VKEY_VOLUME_MUTE;
    case XF86XK_AudioLowerVolume:
      return VKEY_VOLUME_DOWN;
    case XF86XK_AudioRaiseVolume:
      return VKEY_VOLUME_UP;
    case XF86XK_AudioNext:
      return VKEY_MEDIA_NEXT_TRACK;
    case XF86XK_AudioPrev:
      return VKEY_MEDIA_PREV_TRACK;
    case XF86XK_AudioStop:
      return VKEY_MEDIA_STOP;
    case XF86XK_AudioPlay:
      return VKEY_MEDIA_PLAY_PAUSE;
    case XF86XK_Mail:
      return VKEY_MEDIA_LAUNCH_MAIL;
    case XF86XK_LaunchA:  // F3 on an Apple keyboard.
      return VKEY_MEDIA_LAUNCH_APP1;
    case XF86XK_LaunchB:  // F4 on an Apple keyboard.
    case XF86XK_Calculator:
      return VKEY_MEDIA_LAUNCH_APP2;
    case XF86XK_WLAN:
      return VKEY_WLAN;
    case XF86XK_PowerOff:
      return VKEY_POWER;
    case XF86XK_MonBrightnessDown:
      return VKEY_BRIGHTNESS_DOWN;
    case XF86XK_MonBrightnessUp:
      return VKEY_BRIGHTNESS_UP;
    case XF86XK_KbdBrightnessDown:
      return VKEY_KBD_BRIGHTNESS_DOWN;
    case XF86XK_KbdBrightnessUp:
      return VKEY_KBD_BRIGHTNESS_UP;

      // TODO(sad): some keycodes are still missing.
  }
  return VKEY_UNKNOWN;
}

// From content/browser/renderer_host/input/web_input_event_util_posix.cc.
KeyboardCode GdkEventToWindowsKeyCode(const GdkEventKey* event) {
  static const unsigned int kHardwareCodeToGDKKeyval[] = {
      0,                 // 0x00:
      0,                 // 0x01:
      0,                 // 0x02:
      0,                 // 0x03:
      0,                 // 0x04:
      0,                 // 0x05:
      0,                 // 0x06:
      0,                 // 0x07:
      0,                 // 0x08:
      0,                 // 0x09: GDK_Escape
      GDK_1,             // 0x0A: GDK_1
      GDK_2,             // 0x0B: GDK_2
      GDK_3,             // 0x0C: GDK_3
      GDK_4,             // 0x0D: GDK_4
      GDK_5,             // 0x0E: GDK_5
      GDK_6,             // 0x0F: GDK_6
      GDK_7,             // 0x10: GDK_7
      GDK_8,             // 0x11: GDK_8
      GDK_9,             // 0x12: GDK_9
      GDK_0,             // 0x13: GDK_0
      GDK_minus,         // 0x14: GDK_minus
      GDK_equal,         // 0x15: GDK_equal
      0,                 // 0x16: GDK_BackSpace
      0,                 // 0x17: GDK_Tab
      GDK_q,             // 0x18: GDK_q
      GDK_w,             // 0x19: GDK_w
      GDK_e,             // 0x1A: GDK_e
      GDK_r,             // 0x1B: GDK_r
      GDK_t,             // 0x1C: GDK_t
      GDK_y,             // 0x1D: GDK_y
      GDK_u,             // 0x1E: GDK_u
      GDK_i,             // 0x1F: GDK_i
      GDK_o,             // 0x20: GDK_o
      GDK_p,             // 0x21: GDK_p
      GDK_bracketleft,   // 0x22: GDK_bracketleft
      GDK_bracketright,  // 0x23: GDK_bracketright
      0,                 // 0x24: GDK_Return
      0,                 // 0x25: GDK_Control_L
      GDK_a,             // 0x26: GDK_a
      GDK_s,             // 0x27: GDK_s
      GDK_d,             // 0x28: GDK_d
      GDK_f,             // 0x29: GDK_f
      GDK_g,             // 0x2A: GDK_g
      GDK_h,             // 0x2B: GDK_h
      GDK_j,             // 0x2C: GDK_j
      GDK_k,             // 0x2D: GDK_k
      GDK_l,             // 0x2E: GDK_l
      GDK_semicolon,     // 0x2F: GDK_semicolon
      GDK_apostrophe,    // 0x30: GDK_apostrophe
      GDK_grave,         // 0x31: GDK_grave
      0,                 // 0x32: GDK_Shift_L
      GDK_backslash,     // 0x33: GDK_backslash
      GDK_z,             // 0x34: GDK_z
      GDK_x,             // 0x35: GDK_x
      GDK_c,             // 0x36: GDK_c
      GDK_v,             // 0x37: GDK_v
      GDK_b,             // 0x38: GDK_b
      GDK_n,             // 0x39: GDK_n
      GDK_m,             // 0x3A: GDK_m
      GDK_comma,         // 0x3B: GDK_comma
      GDK_period,        // 0x3C: GDK_period
      GDK_slash,         // 0x3D: GDK_slash
      0,                 // 0x3E: GDK_Shift_R
      0,                 // 0x3F:
      0,                 // 0x40:
      0,                 // 0x41:
      0,                 // 0x42:
      0,                 // 0x43:
      0,                 // 0x44:
      0,                 // 0x45:
      0,                 // 0x46:
      0,                 // 0x47:
      0,                 // 0x48:
      0,                 // 0x49:
      0,                 // 0x4A:
      0,                 // 0x4B:
      0,                 // 0x4C:
      0,                 // 0x4D:
      0,                 // 0x4E:
      0,                 // 0x4F:
      0,                 // 0x50:
      0,                 // 0x51:
      0,                 // 0x52:
      0,                 // 0x53:
      0,                 // 0x54:
      0,                 // 0x55:
      0,                 // 0x56:
      0,                 // 0x57:
      0,                 // 0x58:
      0,                 // 0x59:
      0,                 // 0x5A:
      0,                 // 0x5B:
      0,                 // 0x5C:
      0,                 // 0x5D:
      0,                 // 0x5E:
      0,                 // 0x5F:
      0,                 // 0x60:
      0,                 // 0x61:
      0,                 // 0x62:
      0,                 // 0x63:
      0,                 // 0x64:
      0,                 // 0x65:
      0,                 // 0x66:
      0,                 // 0x67:
      0,                 // 0x68:
      0,                 // 0x69:
      0,                 // 0x6A:
      0,                 // 0x6B:
      0,                 // 0x6C:
      0,                 // 0x6D:
      0,                 // 0x6E:
      0,                 // 0x6F:
      0,                 // 0x70:
      0,                 // 0x71:
      0,                 // 0x72:
      GDK_Super_L,       // 0x73: GDK_Super_L
      GDK_Super_R,       // 0x74: GDK_Super_R
  };

  // |windows_key_code| has to include a valid virtual-key code even when we
  // use non-US layouts, e.g. even when we type an 'A' key of a US keyboard
  // on the Hebrew layout, |windows_key_code| should be VK_A.
  // On the other hand, |event->keyval| value depends on the current
  // GdkKeymap object, i.e. when we type an 'A' key of a US keyboard on
  // the Hebrew layout, |event->keyval| becomes GDK_hebrew_shin and this
  // KeyboardCodeFromXKeysym() call returns 0.
  // To improve compatibilty with Windows, we use |event->hardware_keycode|
  // for retrieving its Windows key-code for the keys when the
  // WebCore::windows_key_codeForEvent() call returns 0.
  // We shouldn't use |event->hardware_keycode| for keys that GdkKeymap
  // objects cannot change because |event->hardware_keycode| doesn't change
  // even when we change the layout options, e.g. when we swap a control
  // key and a caps-lock key, GTK doesn't swap their
  // |event->hardware_keycode| values but swap their |event->keyval| values.
  KeyboardCode windows_key_code = KeyboardCodeFromXKeysym(event->keyval);
  if (windows_key_code)
    return windows_key_code;

  if (event->hardware_keycode < base::size(kHardwareCodeToGDKKeyval)) {
    int keyval = kHardwareCodeToGDKKeyval[event->hardware_keycode];
    if (keyval)
      return KeyboardCodeFromXKeysym(keyval);
  }

  // This key is one that keyboard-layout drivers cannot change.
  // Use |event->keyval| to retrieve its |windows_key_code| value.
  return KeyboardCodeFromXKeysym(event->keyval);
}

// From content/browser/renderer_host/input/web_input_event_util_posix.cc.
KeyboardCode GetWindowsKeyCodeWithoutLocation(KeyboardCode key_code) {
  switch (key_code) {
    case VKEY_LCONTROL:
    case VKEY_RCONTROL:
      return VKEY_CONTROL;
    case VKEY_LSHIFT:
    case VKEY_RSHIFT:
      return VKEY_SHIFT;
    case VKEY_LMENU:
    case VKEY_RMENU:
      return VKEY_MENU;
    default:
      return key_code;
  }
}

// From content/browser/renderer_host/input/web_input_event_builders_gtk.cc.
// Gets the corresponding control character of a specified key code. See:
// http://en.wikipedia.org/wiki/Control_characters
// We emulate Windows behavior here.
int GetControlCharacter(KeyboardCode windows_key_code, bool shift) {
  if (windows_key_code >= VKEY_A && windows_key_code <= VKEY_Z) {
    // ctrl-A ~ ctrl-Z map to \x01 ~ \x1A
    return windows_key_code - VKEY_A + 1;
  }
  if (shift) {
    // following graphics chars require shift key to input.
    switch (windows_key_code) {
      // ctrl-@ maps to \x00 (Null byte)
      case VKEY_2:
        return 0;
      // ctrl-^ maps to \x1E (Record separator, Information separator two)
      case VKEY_6:
        return 0x1E;
      // ctrl-_ maps to \x1F (Unit separator, Information separator one)
      case VKEY_OEM_MINUS:
        return 0x1F;
      // Returns 0 for all other keys to avoid inputting unexpected chars.
      default:
        return 0;
    }
  } else {
    switch (windows_key_code) {
      // ctrl-[ maps to \x1B (Escape)
      case VKEY_OEM_4:
        return 0x1B;
      // ctrl-\ maps to \x1C (File separator, Information separator four)
      case VKEY_OEM_5:
        return 0x1C;
      // ctrl-] maps to \x1D (Group separator, Information separator three)
      case VKEY_OEM_6:
        return 0x1D;
      // ctrl-Enter maps to \x0A (Line feed)
      case VKEY_RETURN:
        return 0x0A;
      // Returns 0 for all other keys to avoid inputting unexpected chars.
      default:
        return 0;
    }
  }
}

void GetWidgetRectInScreen(GtkWidget* widget, GdkRectangle* r) {
  gint x, y, w, h;
  GdkRectangle extents;

  GdkWindow* window = gtk_widget_get_parent_window(widget);

  // Get parent's left-top screen coordinates.
  gdk_window_get_root_origin(window, &x, &y);
  // Get parent's width and height.
  w = gdk_window_get_width(window);
  h = gdk_window_get_height(window);
  // Get parent's extents including decorations.
  gdk_window_get_frame_extents(window, &extents);

  // X and Y calculations assume that left, right and bottom border sizes are
  // all the same.
  const gint border = (extents.width - w) / 2;
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);
  r->x = x + border + allocation.x;
  r->y = y + (extents.height - h) - border + allocation.y;
  r->width = allocation.width;
  r->height = allocation.height;
}

CefBrowserHost::DragOperationsMask GetDragOperationsMask(
    GdkDragContext* drag_context) {
  int allowed_ops = DRAG_OPERATION_NONE;
  GdkDragAction drag_action = gdk_drag_context_get_actions(drag_context);
  if (drag_action & GDK_ACTION_COPY)
    allowed_ops |= DRAG_OPERATION_COPY;
  if (drag_action & GDK_ACTION_MOVE)
    allowed_ops |= DRAG_OPERATION_MOVE;
  if (drag_action & GDK_ACTION_LINK)
    allowed_ops |= DRAG_OPERATION_LINK;
  if (drag_action & GDK_ACTION_PRIVATE)
    allowed_ops |= DRAG_OPERATION_PRIVATE;
  return static_cast<CefBrowserHost::DragOperationsMask>(allowed_ops);
}

class ScopedGLContext {
 public:
  ScopedGLContext(GtkWidget* widget, bool swap_buffers)
      : swap_buffers_(swap_buffers), widget_(widget) {
    gtk_gl_area_make_current(GTK_GL_AREA(widget));
    is_valid_ = gtk_gl_area_get_error(GTK_GL_AREA(widget)) == nullptr;
    if (swap_buffers_ && is_valid_) {
      gtk_gl_area_queue_render(GTK_GL_AREA(widget_));
      gtk_gl_area_attach_buffers(GTK_GL_AREA(widget));
    }
  }

  virtual ~ScopedGLContext() {
    if (swap_buffers_ && is_valid_)
      glFlush();
  }

  bool IsValid() const { return is_valid_; }

 private:
  bool swap_buffers_;
  GtkWidget* const widget_;
  bool is_valid_;
  ScopedGdkThreadsEnter scoped_gdk_threads_;
};

}  // namespace

BrowserWindowOsrGtk::BrowserWindowOsrGtk(BrowserWindow::Delegate* delegate,
                                         const std::string& startup_url,
                                         const OsrRendererSettings& settings)
    : BrowserWindow(delegate),
      xdisplay_(nullptr),
      renderer_(settings),
      gl_enabled_(false),
      painting_popup_(false),
      hidden_(false),
      glarea_(nullptr),
      drag_trigger_event_(nullptr),
      drag_data_(nullptr),
      drag_operation_(DRAG_OPERATION_NONE),
      drag_context_(nullptr),
      drag_targets_(gtk_target_list_new(nullptr, 0)),
      drag_leave_(false),
      drag_drop_(false),
      device_scale_factor_(1.0f) {
  client_handler_ = new ClientHandlerOsr(this, this, startup_url);
  g_browser_windows.push_back(this);
}

BrowserWindowOsrGtk::~BrowserWindowOsrGtk() {
  g_browser_windows.erase(
      std::find(g_browser_windows.begin(), g_browser_windows.end(), this));
  ScopedGdkThreadsEnter scoped_gdk_threads;

  if (drag_trigger_event_) {
    gdk_event_free(drag_trigger_event_);
  }
  if (drag_context_) {
    g_object_unref(drag_context_);
  }
  gtk_target_list_unref(drag_targets_);
}

void BrowserWindowOsrGtk::set_xdisplay(XDisplay* xdisplay) {
  REQUIRE_MAIN_THREAD();
  DCHECK(!xdisplay_);
  xdisplay_ = xdisplay;
}

void BrowserWindowOsrGtk::CreateBrowser(
    ClientWindowHandle parent_handle,
    const CefRect& rect,
    const CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue> extra_info,
    CefRefPtr<CefRequestContext> request_context) {
  REQUIRE_MAIN_THREAD();

  // Create the native window.
  Create(parent_handle);

  ScopedGdkThreadsEnter scoped_gdk_threads;

  // Retrieve the X11 Window ID for the GTK parent window.
  GtkWidget* window =
      gtk_widget_get_ancestor(GTK_WIDGET(parent_handle), GTK_TYPE_WINDOW);
  CefWindowHandle handle = GDK_WINDOW_XID(gtk_widget_get_window(window));
  DCHECK(handle);

  CefWindowInfo window_info;
  window_info.SetAsWindowless(handle);

  // Create the browser asynchronously.
  CefBrowserHost::CreateBrowser(window_info, client_handler_,
                                client_handler_->startup_url(), settings,
                                extra_info, request_context);
}

void BrowserWindowOsrGtk::GetPopupConfig(CefWindowHandle temp_handle,
                                         CefWindowInfo& windowInfo,
                                         CefRefPtr<CefClient>& client,
                                         CefBrowserSettings& settings) {
  CEF_REQUIRE_UI_THREAD();

  windowInfo.SetAsWindowless(temp_handle);
  client = client_handler_;
}

void BrowserWindowOsrGtk::ShowPopup(ClientWindowHandle parent_handle,
                                    int x,
                                    int y,
                                    size_t width,
                                    size_t height) {
  REQUIRE_MAIN_THREAD();
  DCHECK(browser_.get());

  // Create the native window.
  Create(parent_handle);

  // Send resize notification so the compositor is assigned the correct
  // viewport size and begins rendering.
  browser_->GetHost()->WasResized();

  Show();
}

void BrowserWindowOsrGtk::Show() {
  REQUIRE_MAIN_THREAD();

  if (hidden_) {
    // Set the browser as visible.
    browser_->GetHost()->WasHidden(false);
    hidden_ = false;
  }

  // Give focus to the browser.
  browser_->GetHost()->SetFocus(true);
}

void BrowserWindowOsrGtk::Hide() {
  REQUIRE_MAIN_THREAD();

  if (!browser_)
    return;

  // Remove focus from the browser.
  browser_->GetHost()->SetFocus(false);

  if (!hidden_) {
    // Set the browser as hidden.
    browser_->GetHost()->WasHidden(true);
    hidden_ = true;
  }
}

void BrowserWindowOsrGtk::SetBounds(int x, int y, size_t width, size_t height) {
  REQUIRE_MAIN_THREAD();
  // Nothing to do here. GTK will take care of positioning in the container.
}

void BrowserWindowOsrGtk::SetFocus(bool focus) {
  REQUIRE_MAIN_THREAD();
  if (glarea_ && focus) {
    gtk_widget_grab_focus(glarea_);
  }
}

void BrowserWindowOsrGtk::SetDeviceScaleFactor(float device_scale_factor) {
  REQUIRE_MAIN_THREAD();
  {
    base::AutoLock lock_scope(lock_);
    if (device_scale_factor == device_scale_factor_)
      return;

    // Apply some sanity checks.
    if (device_scale_factor < 1.0f || device_scale_factor > 4.0f)
      return;

    device_scale_factor_ = device_scale_factor;
  }

  if (browser_) {
    browser_->GetHost()->NotifyScreenInfoChanged();
    browser_->GetHost()->WasResized();
  }
}

float BrowserWindowOsrGtk::GetDeviceScaleFactor() const {
  REQUIRE_MAIN_THREAD();
  base::AutoLock lock_scope(lock_);
  return device_scale_factor_;
}

ClientWindowHandle BrowserWindowOsrGtk::GetWindowHandle() const {
  REQUIRE_MAIN_THREAD();
  return glarea_;
}

void BrowserWindowOsrGtk::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrGtk::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Detach |this| from the ClientHandlerOsr.
  static_cast<ClientHandlerOsr*>(client_handler_.get())->DetachOsrDelegate();

  ScopedGdkThreadsEnter scoped_gdk_threads;

  UnregisterDragDrop();

  // Disconnect all signal handlers that reference |this|.
  g_signal_handlers_disconnect_matched(glarea_, G_SIGNAL_MATCH_DATA, 0, 0,
                                       nullptr, nullptr, this);

  DisableGL();
}

bool BrowserWindowOsrGtk::GetRootScreenRect(CefRefPtr<CefBrowser> browser,
                                            CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();
  return false;
}

void BrowserWindowOsrGtk::GetViewRect(CefRefPtr<CefBrowser> browser,
                                      CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();

  rect.x = rect.y = 0;

  if (!glarea_) {
    // Never return an empty rectangle.
    rect.width = rect.height = 1;
    return;
  }

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  // The simulated screen and view rectangle are the same. This is necessary
  // for popup menus to be located and sized inside the view.
  GtkAllocation allocation;
  gtk_widget_get_allocation(glarea_, &allocation);
  rect.width = DeviceToLogical(allocation.width, device_scale_factor);
  if (rect.width == 0)
    rect.width = 1;
  rect.height = DeviceToLogical(allocation.height, device_scale_factor);
  if (rect.height == 0)
    rect.height = 1;
}

bool BrowserWindowOsrGtk::GetScreenPoint(CefRefPtr<CefBrowser> browser,
                                         int viewX,
                                         int viewY,
                                         int& screenX,
                                         int& screenY) {
  CEF_REQUIRE_UI_THREAD();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  GdkRectangle screen_rect;
  GetWidgetRectInScreen(glarea_, &screen_rect);
  screenX = screen_rect.x + LogicalToDevice(viewX, device_scale_factor);
  screenY = screen_rect.y + LogicalToDevice(viewY, device_scale_factor);
  return true;
}

bool BrowserWindowOsrGtk::GetScreenInfo(CefRefPtr<CefBrowser> browser,
                                        CefScreenInfo& screen_info) {
  CEF_REQUIRE_UI_THREAD();

  CefRect view_rect;
  GetViewRect(browser, view_rect);

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  screen_info.device_scale_factor = device_scale_factor;

  // The screen info rectangles are used by the renderer to create and position
  // popups. Keep popups inside the view rectangle.
  screen_info.rect = view_rect;
  screen_info.available_rect = view_rect;
  return true;
}

void BrowserWindowOsrGtk::OnPopupShow(CefRefPtr<CefBrowser> browser,
                                      bool show) {
  CEF_REQUIRE_UI_THREAD();

  if (!show) {
    renderer_.ClearPopupRects();
    browser->GetHost()->Invalidate(PET_VIEW);
  }
  renderer_.OnPopupShow(browser, show);
}

void BrowserWindowOsrGtk::OnPopupSize(CefRefPtr<CefBrowser> browser,
                                      const CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(lock_);
    device_scale_factor = device_scale_factor_;
  }

  renderer_.OnPopupSize(browser, LogicalToDevice(rect, device_scale_factor));
}

void BrowserWindowOsrGtk::OnPaint(CefRefPtr<CefBrowser> browser,
                                  CefRenderHandler::PaintElementType type,
                                  const CefRenderHandler::RectList& dirtyRects,
                                  const void* buffer,
                                  int width,
                                  int height) {
  CEF_REQUIRE_UI_THREAD();

  if (width <= 2 && height <= 2) {
    // Ignore really small buffer sizes while the widget is starting up.
    return;
  }

  if (painting_popup_) {
    renderer_.OnPaint(browser, type, dirtyRects, buffer, width, height);
    return;
  }

  if (!gl_enabled_)
    EnableGL();

  ScopedGLContext scoped_gl_context(glarea_, true);
  if (!scoped_gl_context.IsValid())
    return;

  renderer_.OnPaint(browser, type, dirtyRects, buffer, width, height);
  if (type == PET_VIEW && !renderer_.popup_rect().IsEmpty()) {
    painting_popup_ = true;
    browser->GetHost()->Invalidate(PET_POPUP);
    painting_popup_ = false;
  }
  renderer_.Render();
}

void BrowserWindowOsrGtk::OnCursorChange(
    CefRefPtr<CefBrowser> browser,
    CefCursorHandle cursor,
    cef_cursor_type_t type,
    const CefCursorInfo& custom_cursor_info) {
  CEF_REQUIRE_UI_THREAD();

  // Retrieve the X11 display shared with Chromium.
  CHECK(xdisplay_ != 0);

  ScopedGdkThreadsEnter scoped_gdk_threads;

  // Retrieve the X11 window handle for the GTK widget.
  ::Window xwindow = GDK_WINDOW_XID(gtk_widget_get_window(glarea_));

  // Set the cursor.
  XDefineCursor(xdisplay_, xwindow, cursor);
}

bool BrowserWindowOsrGtk::StartDragging(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDragData> drag_data,
    CefRenderHandler::DragOperationsMask allowed_ops,
    int x,
    int y) {
  CEF_REQUIRE_UI_THREAD();

  if (!drag_data->HasImage()) {
    LOG(ERROR) << "Drag image representation not available";
    return false;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;

  DragReset();
  drag_data_ = drag_data;

  // Begin drag.
  if (drag_trigger_event_) {
    LOG(ERROR) << "Dragging started, but last mouse event is missing";
    DragReset();
    return false;
  }
  drag_context_ = gtk_drag_begin(glarea_, drag_targets_, GDK_ACTION_COPY,
                                 1,  // left mouse button
                                 drag_trigger_event_);
  if (!drag_context_) {
    LOG(ERROR) << "GTK drag begin failed";
    DragReset();
    return false;
  }
  g_object_ref(drag_context_);

  // Send drag enter event.
  CefMouseEvent ev;
  ev.x = x;
  ev.y = y;
  ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
  browser->GetHost()->DragTargetDragEnter(drag_data, ev, allowed_ops);

  return true;
}

void BrowserWindowOsrGtk::UpdateDragCursor(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::DragOperation operation) {
  CEF_REQUIRE_UI_THREAD();
  drag_operation_ = operation;
}

void BrowserWindowOsrGtk::OnImeCompositionRangeChanged(
    CefRefPtr<CefBrowser> browser,
    const CefRange& selection_range,
    const CefRenderHandler::RectList& character_bounds) {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrGtk::UpdateAccessibilityTree(CefRefPtr<CefValue> value) {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrGtk::UpdateAccessibilityLocation(
    CefRefPtr<CefValue> value) {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrGtk::Create(ClientWindowHandle parent_handle) {
  REQUIRE_MAIN_THREAD();
  DCHECK(!glarea_);

  ScopedGdkThreadsEnter scoped_gdk_threads;

  glarea_ = gtk_gl_area_new();
  DCHECK(glarea_);

  gtk_widget_set_can_focus(glarea_, TRUE);

  gtk_gl_area_set_auto_render(GTK_GL_AREA(glarea_), FALSE);

  g_signal_connect(G_OBJECT(glarea_), "size_allocate",
                   G_CALLBACK(&BrowserWindowOsrGtk::SizeAllocation), this);

  gtk_widget_set_events(
      glarea_, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                   GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
                   GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
                   GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK |
                   GDK_SCROLL_MASK | GDK_FOCUS_CHANGE_MASK);
  g_signal_connect(G_OBJECT(glarea_), "button_press_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::ClickEvent), this);
  g_signal_connect(G_OBJECT(glarea_), "button_release_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::ClickEvent), this);
  g_signal_connect(G_OBJECT(glarea_), "key_press_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::KeyEvent), this);
  g_signal_connect(G_OBJECT(glarea_), "key_release_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::KeyEvent), this);
  g_signal_connect(G_OBJECT(glarea_), "enter_notify_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::MoveEvent), this);
  g_signal_connect(G_OBJECT(glarea_), "leave_notify_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::MoveEvent), this);
  g_signal_connect(G_OBJECT(glarea_), "motion_notify_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::MoveEvent), this);
  g_signal_connect(G_OBJECT(glarea_), "scroll_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::ScrollEvent), this);
  g_signal_connect(G_OBJECT(glarea_), "focus_in_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::FocusEvent), this);
  g_signal_connect(G_OBJECT(glarea_), "focus_out_event",
                   G_CALLBACK(&BrowserWindowOsrGtk::FocusEvent), this);
  g_signal_connect(G_OBJECT(glarea_), "touch-event",
                   G_CALLBACK(&BrowserWindowOsrGtk::TouchEvent), this);

  RegisterDragDrop();

  gtk_widget_set_vexpand(glarea_, TRUE);
  gtk_grid_attach(GTK_GRID(parent_handle), glarea_, 0, 3, 1, 1);

  // Make the GlArea visible in the parent container.
  gtk_widget_show_all(parent_handle);
}

// static
gint BrowserWindowOsrGtk::SizeAllocation(GtkWidget* widget,
                                         GtkAllocation* allocation,
                                         BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();
  if (self->browser_.get()) {
    // Results in a call to GetViewRect().
    self->browser_->GetHost()->WasResized();
  }
  return TRUE;
}

// static
gint BrowserWindowOsrGtk::ClickEvent(GtkWidget* widget,
                                     GdkEventButton* event,
                                     BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  if (!self->browser_.get())
    return TRUE;

  CefRefPtr<CefBrowserHost> host = self->browser_->GetHost();

  CefBrowserHost::MouseButtonType button_type = MBT_LEFT;
  switch (event->button) {
    case 1:
      break;
    case 2:
      button_type = MBT_MIDDLE;
      break;
    case 3:
      button_type = MBT_RIGHT;
      break;
    default:
      // Other mouse buttons are not handled here.
      return FALSE;
  }

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  CefMouseEvent mouse_event;
  mouse_event.x = event->x;
  mouse_event.y = event->y;
  self->ApplyPopupOffset(mouse_event.x, mouse_event.y);
  DeviceToLogical(mouse_event, device_scale_factor);
  mouse_event.modifiers = GetCefStateModifiers(event->state);

  bool mouse_up = (event->type == GDK_BUTTON_RELEASE);
  if (!mouse_up) {
    gtk_widget_grab_focus(widget);
  }

  int click_count = 1;
  switch (event->type) {
    case GDK_2BUTTON_PRESS:
      click_count = 2;
      break;
    case GDK_3BUTTON_PRESS:
      click_count = 3;
      break;
    default:
      break;
  }

  host->SendMouseClickEvent(mouse_event, button_type, mouse_up, click_count);

  // Save mouse event that can be a possible trigger for drag.
  if (!self->drag_context_ && button_type == MBT_LEFT) {
    if (self->drag_trigger_event_) {
      gdk_event_free(self->drag_trigger_event_);
    }
    self->drag_trigger_event_ =
        gdk_event_copy(reinterpret_cast<GdkEvent*>(event));
  }

  return TRUE;
}

// static
gint BrowserWindowOsrGtk::KeyEvent(GtkWidget* widget,
                                   GdkEventKey* event,
                                   BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  if (!self->browser_.get())
    return TRUE;

  CefRefPtr<CefBrowserHost> host = self->browser_->GetHost();

  // Based on WebKeyboardEventBuilder::Build from
  // content/browser/renderer_host/input/web_input_event_builders_gtk.cc.
  CefKeyEvent key_event;
  KeyboardCode windows_key_code = GdkEventToWindowsKeyCode(event);
  key_event.windows_key_code =
      GetWindowsKeyCodeWithoutLocation(windows_key_code);
  key_event.native_key_code = event->hardware_keycode;

  key_event.modifiers = GetCefStateModifiers(event->state);
  if (event->keyval >= GDK_KP_Space && event->keyval <= GDK_KP_9)
    key_event.modifiers |= EVENTFLAG_IS_KEY_PAD;
  if (key_event.modifiers & EVENTFLAG_ALT_DOWN)
    key_event.is_system_key = true;

  if (windows_key_code == VKEY_RETURN) {
    // We need to treat the enter key as a key press of character \r.  This
    // is apparently just how webkit handles it and what it expects.
    key_event.unmodified_character = '\r';
  } else {
    // FIXME: fix for non BMP chars
    key_event.unmodified_character =
        static_cast<int>(gdk_keyval_to_unicode(event->keyval));
  }

  // If ctrl key is pressed down, then control character shall be input.
  if (key_event.modifiers & EVENTFLAG_CONTROL_DOWN) {
    key_event.character = GetControlCharacter(
        windows_key_code, key_event.modifiers & EVENTFLAG_SHIFT_DOWN);
  } else {
    key_event.character = key_event.unmodified_character;
  }

  if (event->type == GDK_KEY_PRESS) {
    key_event.type = KEYEVENT_RAWKEYDOWN;
    host->SendKeyEvent(key_event);
    key_event.type = KEYEVENT_CHAR;
    host->SendKeyEvent(key_event);
  } else {
    key_event.type = KEYEVENT_KEYUP;
    host->SendKeyEvent(key_event);
  }

  return TRUE;
}

// static
gint BrowserWindowOsrGtk::MoveEvent(GtkWidget* widget,
                                    GdkEventMotion* event,
                                    BrowserWindowOsrGtk* self) {
  if (!self->browser_.get())
    return TRUE;

  CefRefPtr<CefBrowserHost> host = self->browser_->GetHost();

  gint x, y;
  GdkModifierType state;

  if (event->is_hint) {
    gdk_window_get_pointer(event->window, &x, &y, &state);
  } else {
    x = (gint)event->x;
    y = (gint)event->y;
    state = (GdkModifierType)event->state;
    if (x == 0 && y == 0) {
      // Invalid coordinates of (0,0) appear from time to time in
      // enter-notify-event and leave-notify-event events. Sending them may
      // cause StartDragging to never get called, so just ignore these.
      return TRUE;
    }
  }

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  CefMouseEvent mouse_event;
  mouse_event.x = x;
  mouse_event.y = y;
  self->ApplyPopupOffset(mouse_event.x, mouse_event.y);
  DeviceToLogical(mouse_event, device_scale_factor);
  mouse_event.modifiers = GetCefStateModifiers(state);

  bool mouse_leave = (event->type == GDK_LEAVE_NOTIFY);
  host->SendMouseMoveEvent(mouse_event, mouse_leave);

  // Save mouse event that can be a possible trigger for drag.
  if (!self->drag_context_ &&
      (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)) {
    if (self->drag_trigger_event_) {
      gdk_event_free(self->drag_trigger_event_);
    }
    self->drag_trigger_event_ =
        gdk_event_copy(reinterpret_cast<GdkEvent*>(event));
  }

  return TRUE;
}

// static
gint BrowserWindowOsrGtk::ScrollEvent(GtkWidget* widget,
                                      GdkEventScroll* event,
                                      BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  if (!self->browser_.get())
    return TRUE;

  CefRefPtr<CefBrowserHost> host = self->browser_->GetHost();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  CefMouseEvent mouse_event;
  mouse_event.x = event->x;
  mouse_event.y = event->y;
  self->ApplyPopupOffset(mouse_event.x, mouse_event.y);
  DeviceToLogical(mouse_event, device_scale_factor);
  mouse_event.modifiers = GetCefStateModifiers(event->state);

  static const int scrollbarPixelsPerGtkTick = 40;
  int deltaX = 0;
  int deltaY = 0;
  switch (event->direction) {
    case GDK_SCROLL_UP:
      deltaY = scrollbarPixelsPerGtkTick;
      break;
    case GDK_SCROLL_DOWN:
      deltaY = -scrollbarPixelsPerGtkTick;
      break;
    case GDK_SCROLL_LEFT:
      deltaX = scrollbarPixelsPerGtkTick;
      break;
    case GDK_SCROLL_RIGHT:
      deltaX = -scrollbarPixelsPerGtkTick;
      break;
    case GDK_SCROLL_SMOOTH:
      NOTIMPLEMENTED();
      break;
  }

  host->SendMouseWheelEvent(mouse_event, deltaX, deltaY);
  return TRUE;
}

// static
gint BrowserWindowOsrGtk::FocusEvent(GtkWidget* widget,
                                     GdkEventFocus* event,
                                     BrowserWindowOsrGtk* self) {
  // May be called on the main thread and the UI thread.
  if (self->browser_.get())
    self->browser_->GetHost()->SetFocus(event->in == TRUE);
  return TRUE;
}

// static
gboolean BrowserWindowOsrGtk::TouchEvent(GtkWidget* widget,
                                         GdkEventTouch* event,
                                         BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  if (!self->browser_.get())
    return TRUE;

  CefRefPtr<CefBrowserHost> host = self->browser_->GetHost();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  CefTouchEvent touch_event;
  switch (event->type) {
    case GDK_TOUCH_BEGIN:
      touch_event.type = CEF_TET_PRESSED;
      break;
    case GDK_TOUCH_UPDATE:
      touch_event.type = CEF_TET_MOVED;
      break;
    case GDK_TOUCH_END:
      touch_event.type = CEF_TET_RELEASED;
      break;
    default:
      return TRUE;
  }

  touch_event.x = event->x;
  touch_event.y = event->y;
  touch_event.radius_x = 0;
  touch_event.radius_y = 0;
  touch_event.rotation_angle = 0;
  touch_event.pressure = 0;
  DeviceToLogical(touch_event, device_scale_factor);
  touch_event.modifiers = GetCefStateModifiers(event->state);

  host->SendTouchEvent(touch_event);
  return TRUE;
}

bool BrowserWindowOsrGtk::IsOverPopupWidget(int x, int y) const {
  const CefRect& rc = renderer_.popup_rect();
  int popup_right = rc.x + rc.width;
  int popup_bottom = rc.y + rc.height;
  return (x >= rc.x) && (x < popup_right) && (y >= rc.y) && (y < popup_bottom);
}

int BrowserWindowOsrGtk::GetPopupXOffset() const {
  return renderer_.original_popup_rect().x - renderer_.popup_rect().x;
}

int BrowserWindowOsrGtk::GetPopupYOffset() const {
  return renderer_.original_popup_rect().y - renderer_.popup_rect().y;
}

void BrowserWindowOsrGtk::ApplyPopupOffset(int& x, int& y) const {
  if (IsOverPopupWidget(x, y)) {
    x += GetPopupXOffset();
    y += GetPopupYOffset();
  }
}

void BrowserWindowOsrGtk::EnableGL() {
  CEF_REQUIRE_UI_THREAD();

  if (gl_enabled_)
    return;

  ScopedGLContext scoped_gl_context(glarea_, false);
  if (!scoped_gl_context.IsValid())
    return;

  renderer_.Initialize();

  gl_enabled_ = true;
}

void BrowserWindowOsrGtk::DisableGL() {
  CEF_REQUIRE_UI_THREAD();

  if (!gl_enabled_)
    return;

  ScopedGLContext scoped_gl_context(glarea_, false);
  if (!scoped_gl_context.IsValid())
    return;

  renderer_.Cleanup();

  gl_enabled_ = false;
}

void BrowserWindowOsrGtk::RegisterDragDrop() {
  REQUIRE_MAIN_THREAD();

  ScopedGdkThreadsEnter scoped_gdk_threads;

  // Succession of CEF d&d calls:
  // 1. DragTargetDragEnter
  // 2. DragTargetDragOver
  // 3. DragTargetDragLeave - optional
  // 4. DragSourceSystemDragEnded - optional, to cancel dragging
  // 5. DragTargetDrop
  // 6. DragSourceEndedAt
  // 7. DragSourceSystemDragEnded

  // Succession of GTK d&d events:
  // 1. drag-begin-event, drag-data-get
  // 2. drag-motion
  // 3. drag-leave
  // 4. drag-failed
  // 5. drag-drop, drag-data-received
  // 6. 7. drag-end-event

  // Using gtk_drag_begin in StartDragging instead of calling
  // gtk_drag_source_set here. Doing so because when using gtk_drag_source_set
  // then StartDragging is being called very late, about ten DragMotion events
  // after DragBegin, and drag icon can be set only when beginning drag.
  // Default values for drag threshold are set to 8 pixels in both GTK and
  // Chromium, but doesn't work as expected.
  // --OFF--
  // gtk_drag_source_set(glarea_, GDK_BUTTON1_MASK, nullptr, 0,
  // GDK_ACTION_COPY);

  // Source widget events.
  g_signal_connect(G_OBJECT(glarea_), "drag_begin",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragBegin), this);
  g_signal_connect(G_OBJECT(glarea_), "drag_data_get",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragDataGet), this);
  g_signal_connect(G_OBJECT(glarea_), "drag_end",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragEnd), this);

  // Destination widget and its events.
  gtk_drag_dest_set(glarea_, (GtkDestDefaults)0, (GtkTargetEntry*)nullptr, 0,
                    (GdkDragAction)GDK_ACTION_COPY);
  g_signal_connect(G_OBJECT(glarea_), "drag_motion",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragMotion), this);
  g_signal_connect(G_OBJECT(glarea_), "drag_leave",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragLeave), this);
  g_signal_connect(G_OBJECT(glarea_), "drag_failed",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragFailed), this);
  g_signal_connect(G_OBJECT(glarea_), "drag_drop",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragDrop), this);
  g_signal_connect(G_OBJECT(glarea_), "drag_data_received",
                   G_CALLBACK(&BrowserWindowOsrGtk::DragDataReceived), this);
}

void BrowserWindowOsrGtk::UnregisterDragDrop() {
  ScopedGdkThreadsEnter scoped_gdk_threads;
  gtk_drag_dest_unset(glarea_);
  // Drag events are unregistered in OnBeforeClose by calling
  // g_signal_handlers_disconnect_matched.
}

void BrowserWindowOsrGtk::DragReset() {
  if (drag_trigger_event_) {
    gdk_event_free(drag_trigger_event_);
    drag_trigger_event_ = nullptr;
  }
  drag_data_ = nullptr;
  drag_operation_ = DRAG_OPERATION_NONE;
  if (drag_context_) {
    g_object_unref(drag_context_);
    drag_context_ = nullptr;
  }
  drag_leave_ = false;
  drag_drop_ = false;
}

// static
void BrowserWindowOsrGtk::DragBegin(GtkWidget* widget,
                                    GdkDragContext* drag_context,
                                    BrowserWindowOsrGtk* self) {
  // Load drag icon.
  if (!self->drag_data_->HasImage()) {
    LOG(ERROR) << "Failed to set drag icon, drag image not available";
    return;
  }

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  int pixel_width = 0;
  int pixel_height = 0;
  CefRefPtr<CefBinaryValue> image_binary =
      self->drag_data_->GetImage()->GetAsPNG(device_scale_factor, true,
                                             pixel_width, pixel_height);
  if (!image_binary) {
    LOG(ERROR) << "Failed to set drag icon, drag image error";
    return;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;

  size_t image_size = image_binary->GetSize();
  guint8* image_buffer = (guint8*)malloc(image_size);  // must free
  image_binary->GetData((void*)image_buffer, image_size, 0);
  GdkPixbufLoader* loader = nullptr;  // must unref
  GError* error = nullptr;            // must free
  GdkPixbuf* pixbuf = nullptr;        // owned by loader
  gboolean success = FALSE;
  loader = gdk_pixbuf_loader_new_with_type("png", &error);
  if (error == nullptr && loader) {
    success =
        gdk_pixbuf_loader_write(loader, image_buffer, image_size, nullptr);
    if (success) {
      success = gdk_pixbuf_loader_close(loader, nullptr);
      if (success) {
        pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
        if (pixbuf) {
          CefPoint image_hotspot = self->drag_data_->GetImageHotspot();
          int hotspot_x = image_hotspot.x;
          int hotspot_y = image_hotspot.y;
          gtk_drag_set_icon_pixbuf(drag_context, pixbuf, hotspot_x, hotspot_y);
        } else {
          LOG(ERROR) << "Failed to set drag icon, pixbuf error";
        }
      } else {
        LOG(ERROR) << "Failed to set drag icon, loader close error";
      }
    } else {
      LOG(ERROR) << "Failed to set drag icon, loader write error";
    }
  } else {
    LOG(ERROR) << "Failed to set drag icon, loader creation error";
  }
  if (loader) {
    g_object_unref(loader);  // unref
  }
  if (error) {
    g_error_free(error);  // free
  }
  free(image_buffer);  // free
}

// static
void BrowserWindowOsrGtk::DragDataGet(GtkWidget* widget,
                                      GdkDragContext* drag_context,
                                      GtkSelectionData* data,
                                      guint info,
                                      guint time,
                                      BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();
  // No drag targets are set so this callback is never called.
}

// static
void BrowserWindowOsrGtk::DragEnd(GtkWidget* widget,
                                  GdkDragContext* drag_context,
                                  BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  if (self->browser_) {
    // Sometimes there is DragEnd event generated without prior DragDrop.
    // Maybe related to drag-leave bug described in comments in DragLeave.
    if (!self->drag_drop_) {
      // Real coordinates not available.
      self->browser_->GetHost()->DragSourceEndedAt(-1, -1,
                                                   self->drag_operation_);
    }
    self->browser_->GetHost()->DragSourceSystemDragEnded();
  }

  self->DragReset();
}

// static
gboolean BrowserWindowOsrGtk::DragMotion(GtkWidget* widget,
                                         GdkDragContext* drag_context,
                                         gint x,
                                         gint y,
                                         guint time,
                                         BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  float device_scale_factor;
  {
    base::AutoLock lock_scope(self->lock_);
    device_scale_factor = self->device_scale_factor_;
  }

  // MoveEvent is never called during drag & drop, so must call
  // SendMouseMoveEvent here.
  CefMouseEvent mouse_event;
  mouse_event.x = x;
  mouse_event.y = y;
  mouse_event.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
  self->ApplyPopupOffset(mouse_event.x, mouse_event.y);
  DeviceToLogical(mouse_event, device_scale_factor);
  if (self->browser_) {
    bool mouse_leave = self->drag_leave_;
    self->browser_->GetHost()->SendMouseMoveEvent(mouse_event, mouse_leave);
  }

  // Mouse event.
  CefMouseEvent ev;
  ev.x = x;
  ev.y = y;
  ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;

  CefBrowserHost::DragOperationsMask allowed_ops =
      GetDragOperationsMask(drag_context);

  // Send drag enter event if needed.
  if (self->drag_leave_ && self->browser_) {
    self->browser_->GetHost()->DragTargetDragEnter(self->drag_data_, ev,
                                                   allowed_ops);
  }

  // Send drag over event.
  if (self->browser_) {
    self->browser_->GetHost()->DragTargetDragOver(ev, allowed_ops);
  }

  // Update GTK drag status.
  if (widget == self->glarea_) {
    gdk_drag_status(drag_context, GDK_ACTION_COPY, time);
    if (self->drag_leave_) {
      self->drag_leave_ = false;
    }
    return TRUE;
  } else {
    LOG(WARNING) << "Invalid drag destination widget";
    gdk_drag_status(drag_context, (GdkDragAction)0, time);
    return FALSE;
  }
}

// static
void BrowserWindowOsrGtk::DragLeave(GtkWidget* widget,
                                    GdkDragContext* drag_context,
                                    guint time,
                                    BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  // There is no drag-enter event in GTK. The first drag-motion event
  // after drag-leave will be a drag-enter event.

  // There seems to be a bug during GTK drop, drag-leave event is generated
  // just before drag-drop. A solution is to call DragTargetDragEnter
  // and DragTargetDragOver in DragDrop when drag_leave_ is true.

  // Send drag leave event.
  if (self->browser_) {
    self->browser_->GetHost()->DragTargetDragLeave();
  }

  self->drag_leave_ = true;
}

// static
gboolean BrowserWindowOsrGtk::DragFailed(GtkWidget* widget,
                                         GdkDragContext* drag_context,
                                         GtkDragResult result,
                                         BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  // Send drag end coordinates and system drag ended event.
  if (self->browser_) {
    // Real coordinates not available.
    self->browser_->GetHost()->DragSourceEndedAt(-1, -1, self->drag_operation_);
    self->browser_->GetHost()->DragSourceSystemDragEnded();
  }

  self->DragReset();
  return TRUE;
}

// static
gboolean BrowserWindowOsrGtk::DragDrop(GtkWidget* widget,
                                       GdkDragContext* drag_context,
                                       gint x,
                                       gint y,
                                       guint time,
                                       BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();

  // Finish GTK drag.
  gtk_drag_finish(drag_context, TRUE, FALSE, time);

  // Mouse event.
  CefMouseEvent ev;
  ev.x = x;
  ev.y = y;
  ev.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;

  CefBrowserHost::DragOperationsMask allowed_ops =
      GetDragOperationsMask(drag_context);

  // Send drag enter/over events if needed (read comment in DragLeave).
  if (self->drag_leave_ && self->browser_) {
    self->browser_->GetHost()->DragTargetDragEnter(self->drag_data_, ev,
                                                   allowed_ops);
    self->browser_->GetHost()->DragTargetDragOver(ev, allowed_ops);
  }

  // Send drag drop event.
  if (self->browser_) {
    self->browser_->GetHost()->DragTargetDrop(ev);
  }

  // Send drag end coordinates.
  if (self->browser_) {
    self->browser_->GetHost()->DragSourceEndedAt(x, y, self->drag_operation_);
  }

  self->drag_drop_ = true;
  return TRUE;
}

// static
void BrowserWindowOsrGtk::DragDataReceived(GtkWidget* widget,
                                           GdkDragContext* drag_context,
                                           gint x,
                                           gint y,
                                           GtkSelectionData* data,
                                           guint info,
                                           guint time,
                                           BrowserWindowOsrGtk* self) {
  REQUIRE_MAIN_THREAD();
  // This callback is never called because DragDrop does not call
  // gtk_drag_get_data, as only dragging inside web view is supported.
}

}  // namespace client
