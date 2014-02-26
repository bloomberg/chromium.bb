// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_IME_KEYMAP_H_
#define UI_BASE_IME_CHROMEOS_IME_KEYMAP_H_

#include <string>
#include "base/basictypes.h"
#include "ui/base/ui_base_export.h"

namespace ui {

// Translates the key value from an X11 key value to key value string
// visible from javascript.
UI_BASE_EXPORT std::string FromXKeycodeToKeyValue(int keyval);

}  // namespace ui

#endif  // UI_BASE_IME_CHROMEOS_IME_KEYMAP_H_
