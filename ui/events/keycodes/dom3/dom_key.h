// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_DOM3_DOM_KEY_H_
#define UI_EVENTS_KEYCODES_DOM3_DOM_KEY_H_

namespace ui {

#define DOM_KEY_MAP(key, id) id
#define DOM_KEY_MAP_DECLARATION enum class DomKey
#include "ui/events/keycodes/dom3/dom_key_data.h"
#undef DOM_KEY_MAP
#undef DOM_KEY_MAP_DECLARATION

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_DOM3_DOM_KEY_H_
