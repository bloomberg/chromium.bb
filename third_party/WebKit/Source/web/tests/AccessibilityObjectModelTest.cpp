// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/AccessibleNode.h"
#include "core/html/HTMLBodyElement.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/accessibility/AXTable.h"
#include "modules/accessibility/AXTableCell.h"
#include "modules/accessibility/AXTableRow.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"

namespace blink {

namespace {

class AccessibilityObjectModelTest
    : public SimTest,
      public ScopedAccessibilityObjectModelForTest {
 public:
  AccessibilityObjectModelTest()
      : ScopedAccessibilityObjectModelForTest(true) {}

 protected:
  AXObjectCacheImpl* AXObjectCache() {
    GetDocument().GetSettings()->SetAccessibilityEnabled(true);
    return static_cast<AXObjectCacheImpl*>(GetDocument().AxObjectCache());
  }
};

TEST_F(AccessibilityObjectModelTest, DOMElementsHaveAnAccessibleNode) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<button id=button>Click me</button>");

  auto* button = GetDocument().getElementById("button");
  EXPECT_NE(nullptr, button->accessibleNode());
  EXPECT_TRUE(button->accessibleNode()->role().IsNull());
  EXPECT_TRUE(button->accessibleNode()->label().IsNull());
}

TEST_F(AccessibilityObjectModelTest, SetAccessibleNodeRole) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<button id=button>Click me</button>");

  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);

  auto* button = GetDocument().getElementById("button");
  ASSERT_NE(nullptr, button);

  auto* axButton = cache->GetOrCreate(button);
  EXPECT_EQ(kButtonRole, axButton->RoleValue());

  button->accessibleNode()->setRole("slider");
  EXPECT_EQ("slider", button->accessibleNode()->role());

  EXPECT_EQ(kSliderRole, axButton->RoleValue());
}

TEST_F(AccessibilityObjectModelTest, AOMDoesNotReflectARIA) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<input id=textbox>");

  // Set ARIA attributes.
  auto* textbox = GetDocument().getElementById("textbox");
  ASSERT_NE(nullptr, textbox);
  textbox->setAttribute("role", "combobox");
  textbox->setAttribute("aria-label", "Combo");
  textbox->setAttribute("aria-disabled", "true");

  // Assert that the ARIA attributes affect the AX object.
  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  auto* axTextBox = cache->GetOrCreate(textbox);
  EXPECT_EQ(kComboBoxRole, axTextBox->RoleValue());
  AXNameFrom name_from;
  AXObjectImpl::AXObjectVector name_objects;
  EXPECT_EQ("Combo", axTextBox->GetName(name_from, &name_objects));
  EXPECT_FALSE(axTextBox->IsEnabled());

  // The AOM properties should still all be null.
  EXPECT_EQ(nullptr, textbox->accessibleNode()->role());
  EXPECT_EQ(nullptr, textbox->accessibleNode()->label());
  bool is_null = false;
  EXPECT_FALSE(textbox->accessibleNode()->disabled(is_null));
  EXPECT_TRUE(is_null);
}

TEST_F(AccessibilityObjectModelTest, AOMPropertiesCanBeCleared) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<input type=button id=button>");

  // Set ARIA attributes.
  auto* button = GetDocument().getElementById("button");
  ASSERT_NE(nullptr, button);
  button->setAttribute("role", "checkbox");
  button->setAttribute("aria-label", "Check");
  button->setAttribute("aria-disabled", "true");

  // Assert that the AX object was affected by ARIA attributes.
  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  auto* axButton = cache->GetOrCreate(button);
  EXPECT_EQ(kCheckBoxRole, axButton->RoleValue());
  AXNameFrom name_from;
  AXObjectImpl::AXObjectVector name_objects;
  EXPECT_EQ("Check", axButton->GetName(name_from, &name_objects));
  EXPECT_FALSE(axButton->IsEnabled());

  // Now set the AOM properties to override.
  button->accessibleNode()->setRole("radio");
  button->accessibleNode()->setLabel("Radio");
  button->accessibleNode()->setDisabled(false, false);

  // Assert that the AX object was affected by AOM properties.
  EXPECT_EQ(kRadioButtonRole, axButton->RoleValue());
  EXPECT_EQ("Radio", axButton->GetName(name_from, &name_objects));
  EXPECT_TRUE(axButton->IsEnabled());

  // Null the AOM properties.
  button->accessibleNode()->setRole(g_null_atom);
  button->accessibleNode()->setLabel(g_null_atom);
  button->accessibleNode()->setDisabled(false, true);

  // The AX Object should now revert to ARIA.
  EXPECT_EQ(kCheckBoxRole, axButton->RoleValue());
  EXPECT_EQ("Check", axButton->GetName(name_from, &name_objects));
  EXPECT_FALSE(axButton->IsEnabled());
}

TEST_F(AccessibilityObjectModelTest, RangeProperties) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<div role=slider id=slider>");

  auto* slider = GetDocument().getElementById("slider");
  ASSERT_NE(nullptr, slider);
  slider->accessibleNode()->setValueMin(-0.5, false);
  slider->accessibleNode()->setValueMax(0.5, false);
  slider->accessibleNode()->setValueNow(0.1, false);

  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  auto* ax_slider = cache->GetOrCreate(slider);
  EXPECT_EQ(-0.5f, ax_slider->MinValueForRange());
  EXPECT_EQ(0.5f, ax_slider->MaxValueForRange());
  EXPECT_EQ(0.1f, ax_slider->ValueForRange());
}

TEST_F(AccessibilityObjectModelTest, Level) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<div role=heading id=heading>");

  auto* heading = GetDocument().getElementById("heading");
  ASSERT_NE(nullptr, heading);
  heading->accessibleNode()->setLevel(5, false);

  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  auto* ax_heading = cache->GetOrCreate(heading);
  EXPECT_EQ(5, ax_heading->HeadingLevel());
}

TEST_F(AccessibilityObjectModelTest, ListItem) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<div role=list><div role=listitem id=listitem></div></div>");

  auto* listitem = GetDocument().getElementById("listitem");
  ASSERT_NE(nullptr, listitem);
  listitem->accessibleNode()->setPosInSet(9, false);
  listitem->accessibleNode()->setSetSize(10, false);

  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);
  auto* ax_listitem = cache->GetOrCreate(listitem);
  EXPECT_EQ(9, ax_listitem->PosInSet());
  EXPECT_EQ(10, ax_listitem->SetSize());
}

TEST_F(AccessibilityObjectModelTest, Grid) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<div role=grid id=grid>"
      "  <div role=row id=row>"
      "    <div role=gridcell id=cell></div>"
      "    <div role=gridcell id=cell2></div>"
      "  </div>"
      "</div>");

  auto* grid = GetDocument().getElementById("grid");
  ASSERT_NE(nullptr, grid);
  grid->accessibleNode()->setColCount(16, false);
  grid->accessibleNode()->setRowCount(9, false);

  auto* row = GetDocument().getElementById("row");
  ASSERT_NE(nullptr, row);
  row->accessibleNode()->setColIndex(8, false);
  row->accessibleNode()->setRowIndex(5, false);

  auto* cell = GetDocument().getElementById("cell");

  auto* cell2 = GetDocument().getElementById("cell2");
  ASSERT_NE(nullptr, cell2);
  cell2->accessibleNode()->setColIndex(10, false);
  cell2->accessibleNode()->setRowIndex(7, false);

  auto* cache = AXObjectCache();
  ASSERT_NE(nullptr, cache);

  auto* ax_grid = static_cast<AXTable*>(cache->GetOrCreate(grid));
  EXPECT_EQ(16, ax_grid->AriaColumnCount());
  EXPECT_EQ(9, ax_grid->AriaRowCount());

  auto* ax_cell = static_cast<AXTableCell*>(cache->GetOrCreate(cell));
  EXPECT_TRUE(ax_cell->IsTableCell());
  EXPECT_EQ(8U, ax_cell->AriaColumnIndex());
  EXPECT_EQ(5U, ax_cell->AriaRowIndex());

  auto* ax_cell2 = static_cast<AXTableCell*>(cache->GetOrCreate(cell2));
  EXPECT_TRUE(ax_cell2->IsTableCell());
  EXPECT_EQ(10U, ax_cell2->AriaColumnIndex());
  EXPECT_EQ(7U, ax_cell2->AriaRowIndex());
}

}  // namespace

}  // namespace blink
