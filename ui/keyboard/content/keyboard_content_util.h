// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_CONTENT_KEYBOARD_COTNENT_UTIL_H_
#define UI_KEYBOARD_CONTENT_KEYBOARD_COTNENT_UTIL_H_

#include <stddef.h>

#include "ui/keyboard/keyboard_export.h"

struct GritResourceMap;

namespace keyboard {

// Get the list of keyboard resources. |size| is populated with the number of
// resources in the returned array.
KEYBOARD_EXPORT const GritResourceMap* GetKeyboardExtensionResources(
    size_t* size);

}  // namespace keyboard

#endif  // UI_KEYBOARD_CONTENT_KEYBOARD_COTNENT_UTIL_H_
