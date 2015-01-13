// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_LAYOUT_LAYOUT_UTIL_H_
#define UI_EVENTS_OZONE_LAYOUT_LAYOUT_UTIL_H_

// TODO(kpschoedel): consider moving this out of Ozone.

#include "base/strings/string16.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/ozone/layout/events_ozone_layout_export.h"

namespace ui {

enum class DomCode;
enum class DomKey;

// Returns a Windows-based VKEY for a non-printable DOM Level 3 |key|.
// The returned VKEY is non-located (e.g. VKEY_SHIFT).
EVENTS_OZONE_LAYOUT_EXPORT KeyboardCode
NonPrintableDomKeyToKeyboardCode(DomKey dom_key);

// Returns the Windows-based VKEY value corresponding to a DOM Level 3 |code|.
// The returned VKEY is located (e.g. VKEY_LSHIFT).
EVENTS_OZONE_LAYOUT_EXPORT KeyboardCode DomCodeToKeyboardCode(DomCode dom_code);

// Returns the Windows-based VKEY value corresponding to a DOM Level 3 |code|.
// The returned VKEY is non-located (e.g. VKEY_SHIFT).
EVENTS_OZONE_LAYOUT_EXPORT KeyboardCode
DomCodeToNonLocatedKeyboardCode(DomCode dom_code);

// Returns true control character corresponding to a physical key.
// In some contexts this is used instead of the key layout.
EVENTS_OZONE_LAYOUT_EXPORT bool LookupControlCharacter(DomCode dom_code,
                                                       int flags,
                                                       DomKey* dom_key,
                                                       base::char16* character,
                                                       KeyboardCode* key_code);

// Returns the ui::EventFlags value associated with a modifier key,
// or 0 (EF_NONE) if the key is not a modifier.
EVENTS_OZONE_LAYOUT_EXPORT int ModifierDomKeyToEventFlag(DomKey key);

}  // namespace ui

#endif  // UI_EVENTS_OZONE_LAYOUT_LAYOUT_UTIL_H_
