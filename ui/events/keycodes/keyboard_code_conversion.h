// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_H_
#define UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_H_

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "ui/events/events_base_export.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {

enum class DomCode;
enum class DomKey;

// Helper functions to get the meaning of a Windows key code in a
// platform independent way. It supports control characters as well.
// It assumes a US keyboard layout is used, so it may only be used when there
// is no native event or no better way to get the character.
//
// For example, if a virtual keyboard implementation can only generate key
// events with key_code and flags information, then there is no way for us to
// determine the actual character that should be generate by the key. Because
// a key_code only represents a physical key on the keyboard, it has nothing
// to do with the actual character printed on that key. In such case, the only
// thing we can do is to assume that we are using a US keyboard and get the
// character according to US keyboard layout definition. Preferably, such
// events should be created using a full KeyEvent constructor, explicitly
// specifying the character and DOM 3 values as well as the legacy VKEY.
//
// TODO(kpschoedel): replace remaining uses of the ...FromKeyCode() functions
// and remove them, to avoid future creation of underspecified key events.
// crbug.com/444045
EVENTS_BASE_EXPORT base::char16 GetCharacterFromKeyCode(KeyboardCode key_code,
                                                        int flags);
EVENTS_BASE_EXPORT bool GetMeaningFromKeyCode(KeyboardCode key_code,
                                              int flags,
                                              DomKey* dom_key,
                                              base::char16* character);

// Helper function to map a physical key state (dom_code and flags)
// to a meaning (dom_key and character, together corresponding to the
// DOM keyboard event |key| value), along with a corresponding Windows-based
// key_code.
//
// This follows a US keyboard layout, so it should only be used when there
// is no other better way to obtain the meaning (e.g. actual keyboard layout).
// Returns true and sets the output parameters if the (dom_code, flags) pair
// has an interpretation in the US English layout; otherwise the output
// parameters are untouched.
EVENTS_BASE_EXPORT bool DomCodeToUsLayoutMeaning(DomCode dom_code,
                                                 int flags,
                                                 DomKey* dom_key,
                                                 base::char16* character,
                                                 KeyboardCode* key_code)
    WARN_UNUSED_RESULT;

// Obtains the control character corresponding to a physical key;
// that is, the meaning of the physical key state (dom_code, and flags
// containing EF_CONTROL_DOWN) under the base US English layout.
// Returns true and sets the output parameters if the (dom_code, flags) pair
// is interpreted as a control character; otherwise the output parameters
// are untouched.
EVENTS_BASE_EXPORT bool DomCodeToControlCharacter(DomCode dom_code,
                                                  int flags,
                                                  DomKey* dom_key,
                                                  base::char16* character,
                                                  KeyboardCode* key_code)
    WARN_UNUSED_RESULT;

// Returns the DomKey value associated with an ASCII/Unicode character.
// All printable characters and most other character codes use
// DomKey::CHARACTER, but a few ASCII C0 codes have their own DomKey.
EVENTS_BASE_EXPORT DomKey CharacterToDomKey(uint32 character);

// Returns a Windows-based VKEY for a non-printable DOM Level 3 |key|.
// The returned VKEY is non-located (e.g. VKEY_SHIFT).
EVENTS_BASE_EXPORT KeyboardCode
NonPrintableDomKeyToKeyboardCode(DomKey dom_key);

// Determine the non-located VKEY corresponding to a located VKEY.
// Most modifier keys have two kinds of KeyboardCode: located (e.g.
// VKEY_LSHIFT and VKEY_RSHIFT), that indentify one of two specific
// physical keys, and non-located (e.g. VKEY_SHIFT) that identify
// only the operation.
EVENTS_BASE_EXPORT KeyboardCode
LocatedToNonLocatedKeyboardCode(KeyboardCode key_code);

// Determine the located VKEY corresponding to a non-located VKEY.
EVENTS_BASE_EXPORT KeyboardCode
NonLocatedToLocatedKeyboardCode(KeyboardCode key_code, DomCode dom_code);

// Returns a DOM Level 3 |code| from a Windows-based VKEY value.
// This assumes a US layout and should only be used when |code| cannot be
// determined from a physical scan code, for example when a key event was
// generated synthetically by JavaScript with only a VKEY value supplied.
EVENTS_BASE_EXPORT DomCode UsLayoutKeyboardCodeToDomCode(KeyboardCode key_code);

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_KEYBOARD_CODE_CONVERSION_H_
