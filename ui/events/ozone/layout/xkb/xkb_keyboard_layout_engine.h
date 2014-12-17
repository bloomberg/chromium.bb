// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_LAYOUT_XKB_XKB_KEYBOARD_LAYOUT_ENGINE_H_
#define UI_EVENTS_OZONE_LAYOUT_XKB_XKB_KEYBOARD_LAYOUT_ENGINE_H_

#include <xkbcommon/xkbcommon.h>

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "ui/events/ozone/layout/events_ozone_layout_export.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/xkb/scoped_xkb.h"
#include "ui/events/ozone/layout/xkb/xkb_key_code_converter.h"

namespace ui {

class EVENTS_OZONE_LAYOUT_EXPORT XkbKeyboardLayoutEngine
    : public KeyboardLayoutEngine {
 public:
  XkbKeyboardLayoutEngine(const XkbKeyCodeConverter& converter);
  virtual ~XkbKeyboardLayoutEngine();

  // KeyboardLayoutEngine:
  virtual bool CanSetCurrentLayout() const override;
  virtual bool SetCurrentLayoutByName(const std::string& layout_name) override;

  virtual bool UsesISOLevel5Shift() const override;
  virtual bool UsesAltGr() const override;

  virtual bool Lookup(DomCode dom_code,
                      int flags,
                      DomKey* dom_key,
                      base::char16* character,
                      KeyboardCode* key_code,
                      uint32* platform_keycode) const override;

 protected:
  // Table for EventFlagsToXkbFlags().
  struct XkbFlagMapEntry {
    int ui_flag;
    xkb_mod_mask_t xkb_flag;
  };
  std::vector<XkbFlagMapEntry> xkb_flag_map_;

  // Determines the Windows-based KeyboardCode (VKEY) for a character key,
  // accounting for non-US layouts. May return VKEY_UNKNOWN, in which case the
  // caller should use |DomCodeToNonLocatedKeyboardCode()| as a last resort.
  KeyboardCode DifficultKeyboardCode(DomCode dom_code,
                                     int ui_flags,
                                     xkb_keycode_t xkb_keycode,
                                     xkb_mod_mask_t xkb_flags,
                                     xkb_keysym_t xkb_keysym,
                                     DomKey dom_key,
                                     base::char16 character) const;

  // Maps DomCode to xkb_keycode_t.
  const XkbKeyCodeConverter& key_code_converter_;

 private:
  // Sets a new XKB keymap. This updates xkb_state_ (which takes ownership
  // of the keymap), and updates xkb_flag_map_ for the new keymap.
  void SetKeymap(xkb_keymap* keymap);

  // Returns the XKB modifiers flags corresponding to the given EventFlags.
  xkb_mod_mask_t EventFlagsToXkbFlags(int ui_flags) const;

  // Determines the XKB KeySym and character associated with a key.
  // Returns true on success. This is virtual for testing.
  virtual bool XkbLookup(xkb_keycode_t xkb_keycode,
                         xkb_mod_mask_t xkb_flags,
                         xkb_keysym_t* xkb_keysym,
                         base::char16* character) const;

  // Helper for difficult VKEY lookup. If |ui_flags| matches |base_flags|,
  // returns |base_character|; otherwise returns the XKB character for
  // the keycode and mapped |ui_flags|.
  base::char16 XkbSubCharacter(xkb_keycode_t xkb_keycode,
                               xkb_mod_mask_t base_flags,
                               base::char16 base_character,
                               int ui_flags) const;

  // libxkbcommon uses explicit reference counting for its structures,
  // so we need to trigger its cleanup.
  scoped_ptr<xkb_context, XkbContextDeleter> xkb_context_;
  scoped_ptr<xkb_state, XkbStateDeleter> xkb_state_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_LAYOUT_XKB_XKB_KEYBOARD_LAYOUT_ENGINE_H_
