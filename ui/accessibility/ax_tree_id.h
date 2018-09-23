// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_ID_H_
#define UI_ACCESSIBILITY_AX_TREE_ID_H_

#include <string>

#include "ui/accessibility/ax_export.h"

namespace ui {

// Note: Originally AXTreeID was an integer. Temporarily we're making it a
// string with the plan to eventually make it a base::UnguessableToken.
using AXTreeID = std::string;

// The value to use when an AXTreeID is unknown.
AX_EXPORT extern const AXTreeID& AXTreeIDUnknown();

// The AXTreeID of the desktop.
AX_EXPORT extern const AXTreeID& DesktopAXTreeID();

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_ID_H_
