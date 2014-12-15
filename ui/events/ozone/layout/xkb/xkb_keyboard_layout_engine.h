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
                      KeyboardCode* key_code) const override;

 private:
  // Sets a new XKB keymap, updating object fields.
  void SetKeymap(xkb_keymap* keymap);

  // Returns the XKB modifiers flags corresponding to the given EventFlags.
  xkb_mod_mask_t EventFlagsToXkbFlags(int ui_flags) const;

  // Determines the XKB KeySym and character associated with a key.
  // Returns true on success.
  bool XkbLookup(xkb_keycode_t xkb_keycode,
                 xkb_mod_mask_t xkb_flags,
                 xkb_keysym_t* xkb_keysym,
                 base::char16* character) const;

  // Maps DomCode to xkb_keycode_t.
  const XkbKeyCodeConverter& key_code_converter_;

  // libxkbcommon uses explicit reference counting for its structures,
  // so we need to trigger its cleanup.
  scoped_ptr<xkb_context, XkbContextDeleter> xkb_context_;
  scoped_ptr<xkb_state, XkbStateDeleter> xkb_state_;

  // Table for EventFlagsToXkbFlags().
  struct XkbFlagMapEntry {
    int ui_flag;
    xkb_mod_mask_t xkb_flag;
  };
  std::vector<XkbFlagMapEntry> xkb_flag_map_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_LAYOUT_XKB_XKB_KEYBOARD_LAYOUT_ENGINE_H_
