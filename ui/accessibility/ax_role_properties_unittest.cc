// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_role_properties.h"

namespace ui {

TEST(AXRolePropertiesTest, TestSupportsExpandCollapse) {
  AXNodeData data;

  // Test for ax node data attributes/state that support expand collapse.
  data.role = ax::mojom::Role::kGenericContainer;
  data.SetHasPopup(ax::mojom::HasPopup::kTrue);
  EXPECT_TRUE(SupportsExpandCollapse(data));

  data.SetHasPopup(ax::mojom::HasPopup::kFalse);
  EXPECT_FALSE(SupportsExpandCollapse(data));

  data.AddState(ax::mojom::State::kExpanded);
  EXPECT_TRUE(SupportsExpandCollapse(data));
  data.RemoveState(ax::mojom::State::kExpanded);

  data.AddState(ax::mojom::State::kCollapsed);
  EXPECT_TRUE(SupportsExpandCollapse(data));
  data.RemoveState(ax::mojom::State::kCollapsed);

  // Test for roles that support expand collapse.
  data.role = ax::mojom::Role::kComboBoxGrouping;
  EXPECT_TRUE(SupportsExpandCollapse(data));

  data.role = ax::mojom::Role::kComboBoxMenuButton;
  EXPECT_TRUE(SupportsExpandCollapse(data));

  data.role = ax::mojom::Role::kDisclosureTriangle;
  EXPECT_TRUE(SupportsExpandCollapse(data));

  data.role = ax::mojom::Role::kTextFieldWithComboBox;
  EXPECT_TRUE(SupportsExpandCollapse(data));

  data.role = ax::mojom::Role::kTreeItem;
  EXPECT_TRUE(SupportsExpandCollapse(data));
}

}  // namespace ui
