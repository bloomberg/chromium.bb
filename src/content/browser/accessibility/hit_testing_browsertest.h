// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_HIT_TESTING_BROWSERTEST_H_
#define CONTENT_BROWSER_ACCESSIBILITY_HIT_TESTING_BROWSERTEST_H_

#include "content/browser/accessibility/accessibility_content_browsertest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// First parameter of the tuple = device scale factor
// Second parameter = whether use-zoom-for-dsf is enabled
using AccessibilityZoomTestParam = std::tuple<double, bool>;

class AccessibilityHitTestingBrowserTest
    : public AccessibilityContentBrowserTest,
      public ::testing::WithParamInterface<AccessibilityZoomTestParam> {
 public:
  AccessibilityHitTestingBrowserTest();
  ~AccessibilityHitTestingBrowserTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  struct TestPassToString {
    std::string operator()(
        const ::testing::TestParamInfo<AccessibilityZoomTestParam>& info) const;
  };

 protected:
  BrowserAccessibilityManager* GetRootBrowserAccessibilityManager();
  float GetDeviceScaleFactor();
  float GetPageScaleFactor();
  gfx::Rect GetViewBoundsInScreenCoordinates();
  gfx::Point CSSToFramePoint(gfx::Point css_point);
  gfx::Point CSSToPhysicalPixelPoint(gfx::Point css_point);
  BrowserAccessibility* HitTestAndWaitForResultWithEvent(
      const gfx::Point& point,
      ax::mojom::Event event_to_fire);
  BrowserAccessibility* HitTestAndWaitForResult(const gfx::Point& point);
  BrowserAccessibility* CallCachingAsyncHitTest(const gfx::Point& page_point);
  BrowserAccessibility* CallNearestLeafNode(const gfx::Point& page_point);
  void SynchronizeThreads();
  base::string16 FormatHitTestAccessibilityTree();
  std::string GetScopedTrace(gfx::Point css_point);
  void SimulatePinchZoom(float desired_page_scale);

  float page_scale_ = 1.0f;
  gfx::Vector2d scroll_offset_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_HIT_TESTING_BROWSERTEST_H_
