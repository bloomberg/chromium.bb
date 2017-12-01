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

// Returns true if this node is a document
AX_EXPORT bool IsDocument(ui::AXRole role);

// Returns true if this node is a cell or a table header.
AX_EXPORT bool IsCellOrTableHeaderRole(AXRole role);

// Returns true if this node is a table, a grid or a treegrid.
AX_EXPORT bool IsTableLikeRole(AXRole role);

// Returns true if this node is a container with selectable children.
AX_EXPORT bool IsContainerWithSelectableChildrenRole(ui::AXRole role);

// Returns true if this node is a row container.
AX_EXPORT bool IsRowContainer(ui::AXRole role);

// Returns true if this node is a control.
AX_EXPORT bool IsControl(ui::AXRole role);

// Returns true if this node is a menu or related role.
AX_EXPORT bool IsMenuRelated(ui::AXRole role);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_ROLE_PROPERTIES_H_
