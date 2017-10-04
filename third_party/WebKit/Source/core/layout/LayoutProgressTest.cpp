// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutProgress.h"

#include "core/html/HTMLElement.h"
#include "core/html_names.h"
#include "core/layout/LayoutTestHelper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutProgressTest : public RenderingTest {
 public:
  static bool IsAnimationTimerActive(const LayoutProgress* o) {
    return o->IsAnimationTimerActive();
  }
  static bool IsAnimatiing(const LayoutProgress* o) { return o->IsAnimating(); }
};

TEST_F(LayoutProgressTest, AnimationScheduling) {
  RenderingTest::SetBodyInnerHTML(
      "<progress id=\"progressElement\" value=0.3 max=1.0></progress>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* progress_element =
      GetDocument().getElementById(AtomicString("progressElement"));
  LayoutProgress* layout_progress =
      ToLayoutProgress(progress_element->GetLayoutObject());

  // Verify that we do not schedule a timer for a determinant progress element
  EXPECT_FALSE(LayoutProgressTest::IsAnimationTimerActive(layout_progress));
  EXPECT_FALSE(LayoutProgressTest::IsAnimatiing(layout_progress));

  progress_element->removeAttribute("value");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Verify that we schedule a timer for an indeterminant progress element
  EXPECT_TRUE(LayoutProgressTest::IsAnimationTimerActive(layout_progress));
  EXPECT_TRUE(LayoutProgressTest::IsAnimatiing(layout_progress));

  progress_element->setAttribute(HTMLNames::valueAttr, "0.7");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Verify that we cancel the timer for a determinant progress element
  EXPECT_FALSE(LayoutProgressTest::IsAnimationTimerActive(layout_progress));
  EXPECT_FALSE(LayoutProgressTest::IsAnimatiing(layout_progress));
}

}  // namespace blink
