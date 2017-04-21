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

}  // namespace

}  // namespace blink
