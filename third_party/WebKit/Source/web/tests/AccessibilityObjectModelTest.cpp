// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/AccessibleNode.h"
#include "core/html/HTMLBodyElement.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

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

}  // namespace

}  // namespace blink
