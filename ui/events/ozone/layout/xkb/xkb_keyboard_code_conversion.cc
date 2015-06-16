// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/xkb/xkb_keyboard_code_conversion.h"

#include "ui/events/keycodes/dom/dom_key.h"

namespace ui {

base::char16 DeadXkbKeySymToCombiningCharacter(xkb_keysym_t keysym) {
  switch (keysym) {
    case XKB_KEY_dead_grave:
      return 0x0300;  // combining grave accent
    case XKB_KEY_dead_acute:
      return 0x0301;  // combining acute accent
    case XKB_KEY_dead_circumflex:
      return 0x0302;  // combining circumflex accent
    case XKB_KEY_dead_tilde:
      return 0x0303;  // combining tilde
    case XKB_KEY_dead_macron:
      return 0x0304;  // combining macron
    case XKB_KEY_dead_breve:
      return 0x0306;  // combining breve
    case XKB_KEY_dead_abovedot:
      return 0x0307;  // combining dot above
    case XKB_KEY_dead_diaeresis:
      return 0x0308;  // combining diaeresis
    case XKB_KEY_dead_abovering:
      return 0x030A;  // combining ring above
    case XKB_KEY_dead_doubleacute:
      return 0x030B;  // combining double acute accent
    case XKB_KEY_dead_caron:
      return 0x030C;  // combining caron
    case XKB_KEY_dead_cedilla:
      return 0x0327;  // combining cedilla
    case XKB_KEY_dead_ogonek:
      return 0x0328;  // combining ogonek
    case XKB_KEY_dead_iota:
      return 0x0345;  // combining greek ypogegrammeni
    case XKB_KEY_dead_voiced_sound:
      return 0x3099;  // combining katakana-hiragana voiced sound mark
    case XKB_KEY_dead_semivoiced_sound:
      return 0x309A;  // combining katakana-hiragana semi-voiced sound mark
    case XKB_KEY_dead_belowdot:
      return 0x0323;  // combining dot below
    case XKB_KEY_dead_hook:
      return 0x0309;  // combining hook above
    case XKB_KEY_dead_horn:
      return 0x031B;  // combining horn
    case XKB_KEY_dead_stroke:
      return 0x0338;  // combining long solidus overlay
    case XKB_KEY_dead_abovecomma:
      return 0x0313;  // combining comma above
    case XKB_KEY_dead_abovereversedcomma:
      return 0x0314;  // combining reversed comma above
    case XKB_KEY_dead_doublegrave:
      return 0x030F;  // combining double grave accent
    case XKB_KEY_dead_belowring:
      return 0x0325;  // combining ring below
    case XKB_KEY_dead_belowmacron:
      return 0x0331;  // combining macron below
    case XKB_KEY_dead_belowcircumflex:
      return 0x032D;  // combining circumflex accent below
    case XKB_KEY_dead_belowtilde:
      return 0x0330;  // combining tilde below
    case XKB_KEY_dead_belowbreve:
      return 0x032E;  // combining breve below
    case XKB_KEY_dead_belowdiaeresis:
      return 0x0324;  // combining diaeresis below
    case XKB_KEY_dead_invertedbreve:
      return 0x0311;  // combining inverted breve
    case XKB_KEY_dead_belowcomma:
      return 0x0326;  // combining comma below
    case XKB_KEY_dead_currency:
      return 0x00A4;  // currency sign
    case XKB_KEY_dead_greek:
      return 0x037E;  // greek question mark
    default:
      return 0;
  }
}

}  // namespace ui
