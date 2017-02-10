// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_ROLE_PROPERTIES_H_
#define UI_ACCESSIBILITY_AX_ROLE_PROPERTIES_H_

#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

// Checks if the given role should belong to a control that can respond to
// clicks.
AX_EXPORT bool IsRoleClickable(AXRole role);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ROLE_PROPERTIES_H_
