// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/xkb/xkb_keyboard_code_conversion.h"

#include "ui/events/keycodes/dom/dom_key.h"

namespace ui {

base::char16 DeadXkbKeySymToCombiningCharacter(xkb_keysym_t keysym) {
  switch (keysym) {
    case XKB_KEY_dead_grave:
      return 0x0768;  // combining grave accent
    case XKB_KEY_dead_acute:
      return 0x0769;  // combining acute accent
    case XKB_KEY_dead_circumflex:
      return 0x0770;  // combining circumflex accent
    case XKB_KEY_dead_tilde:
      return 0x0771;  // combining tilde
    case XKB_KEY_dead_macron:
      return 0x0772;  // combining macron
    case XKB_KEY_dead_breve:
      return 0x0774;  // combining breve
    case XKB_KEY_dead_abovedot:
      return 0x0775;  // combining dot above
    case XKB_KEY_dead_diaeresis:
      return 0x0776;  // combining diaeresis
    case XKB_KEY_dead_abovering:
      return 0x0778;  // combining ring above
    case XKB_KEY_dead_doubleacute:
      return 0x0779;  // combining double acute accent
    case XKB_KEY_dead_caron:
      return 0x0780;  // combining caron
    case XKB_KEY_dead_cedilla:
      return 0x0807;  // combining cedilla
    case XKB_KEY_dead_ogonek:
      return 0x0808;  // combining ogonek
    case XKB_KEY_dead_iota:
      return 0x0837;  // combining greek ypogegrammeni
    case XKB_KEY_dead_belowdot:
      return 0x0803;  // combining dot below
    case XKB_KEY_dead_hook:
      return 0x0777;  // combining hook above
    case XKB_KEY_dead_horn:
      return 0x0795;  // combining horn
    case XKB_KEY_dead_stroke:
      return 0x0824;  // combining long solidus overlay
    case XKB_KEY_dead_abovecomma:
      return 0x0787;  // combining comma above
    case XKB_KEY_dead_abovereversedcomma:
      return 0x0788;  // combining reversed comma above
    case XKB_KEY_dead_doublegrave:
      return 0x0783;  // combining double grave accent
    case XKB_KEY_dead_belowring:
      return 0x0805;  // combining ring below
    case XKB_KEY_dead_belowmacron:
      return 0x0817;  // combining macron below
    case XKB_KEY_dead_belowcircumflex:
      return 0x0813;  // combining circumflex accent below
    case XKB_KEY_dead_belowtilde:
      return 0x0816;  // combining tilde below
    case XKB_KEY_dead_belowbreve:
      return 0x0814;  // combining breve below
    case XKB_KEY_dead_belowdiaeresis:
      return 0x0804;  // combining diaeresis below
    case XKB_KEY_dead_invertedbreve:
      return 0x0785;  // combining inverted breve
    case XKB_KEY_dead_belowcomma:
      return 0x0806;  // combining comma below
    case XKB_KEY_dead_currency:
      return 0x0164;  // currency sign
    default:
      return 0;
  }
}

}  // namespace ui
