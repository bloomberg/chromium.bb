// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_metrics.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class AnchorElementMetricsTest : public SimTest {
 public:
  static constexpr int kViewportWidth = 400;
  static constexpr int kViewportHeight = 600;

 protected:
  AnchorElementMetricsTest() = default;

  void SetUp() override {
    SimTest::SetUp();
    WebView().Resize(WebSize(kViewportWidth, kViewportHeight));
  }

  bool IsIncrementedByOne(const String& a, const String& b) {
    return AnchorElementMetrics::IsStringIncrementedByOne(a, b);
  }
};

// Test for IsUrlIncrementedByOne().
TEST_F(AnchorElementMetricsTest, IsUrlIncrementedByOne) {
  EXPECT_TRUE(IsIncrementedByOne("example.com/p1", "example.com/p2"));
  EXPECT_TRUE(IsIncrementedByOne("example.com/?p=9", "example.com/?p=10"));
  EXPECT_TRUE(IsIncrementedByOne("example.com/?p=12", "example.com/?p=13"));
  EXPECT_TRUE(
      IsIncrementedByOne("example.com/p9/cat1", "example.com/p10/cat1"));

  EXPECT_FALSE(IsIncrementedByOne("example.com", ""));
  EXPECT_FALSE(IsIncrementedByOne("example.com/1", "google.com/2"));
  EXPECT_FALSE(IsIncrementedByOne("example.com/p1", "example.com/p1"));
  EXPECT_FALSE(IsIncrementedByOne("example.com/p2", "example.com/p1"));
  EXPECT_FALSE(
      IsIncrementedByOne("example.com/p9/cat1", "example.com/p10/cat2"));
}

// The main frame contains an anchor element, which contains an image element.
TEST_F(AnchorElementMetricsTest, AnchorFeatureImageLink) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
    <body style='margin: 0px'>
    <div style='height: %dpx;'></div>
    <a id='anchor' href="https://example.com/page2">
      <img height="300" width="200">
    </a>
    <div style='height: %d;'></div>
    </body>)HTML",
      kViewportHeight / 2, 10 * kViewportHeight));

  Element* anchor = GetDocument().getElementById("anchor");
  HTMLAnchorElement* anchor_element = ToHTMLAnchorElement(anchor);

  auto feature = AnchorElementMetrics::CreateFrom(anchor_element).value();
  EXPECT_FLOAT_EQ(0.25, feature.GetRatioArea());
  EXPECT_FLOAT_EQ(0.25, feature.GetRatioVisibleArea());
  EXPECT_FLOAT_EQ(0.5, feature.GetRatioDistanceTopToVisibleTop());
  EXPECT_FLOAT_EQ(0.75, feature.GetRatioDistanceCenterToVisibleTop());
  EXPECT_FLOAT_EQ(0.5, feature.GetRatioDistanceRootTop());
  EXPECT_FLOAT_EQ(10, feature.GetRatioDistanceRootBottom());
  EXPECT_FALSE(feature.GetIsInIframe());
  EXPECT_TRUE(feature.GetContainsImage());
  EXPECT_TRUE(feature.GetIsSameHost());
  EXPECT_FALSE(feature.GetIsUrlIncrementedByOne());
}

// The main frame contains an anchor element.
// Features of the element are extracted.
// Then the test scrolls down to check features again.
TEST_F(AnchorElementMetricsTest, AnchorFeatureExtract) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(
      R"HTML(
    <body style='margin: 0px'>
    <div style='height: %dpx;'></div>
    <a id='anchor' href="https://b.example.com">example</a>
    <div style='height: %d;'></div>
    </body>)HTML",
      2 * kViewportHeight, 10 * kViewportHeight));

  Element* anchor = GetDocument().getElementById("anchor");
  HTMLAnchorElement* anchor_element = ToHTMLAnchorElement(anchor);

  auto feature = AnchorElementMetrics::CreateFrom(anchor_element).value();
  EXPECT_GT(feature.GetRatioArea(), 0);
  EXPECT_FLOAT_EQ(feature.GetRatioDistanceRootTop(), 2);
  EXPECT_FLOAT_EQ(feature.GetRatioDistanceTopToVisibleTop(), 2);
  EXPECT_EQ(feature.GetIsInIframe(), false);

  // Element not in the viewport.
  EXPECT_GT(feature.GetRatioArea(), 0);
  EXPECT_FLOAT_EQ(0, feature.GetRatioVisibleArea());
  EXPECT_FLOAT_EQ(2, feature.GetRatioDistanceTopToVisibleTop());
  EXPECT_LT(2, feature.GetRatioDistanceCenterToVisibleTop());
  EXPECT_FLOAT_EQ(2, feature.GetRatioDistanceRootTop());
  EXPECT_FLOAT_EQ(10, feature.GetRatioDistanceRootBottom());
  EXPECT_FALSE(feature.GetIsInIframe());
  EXPECT_FALSE(feature.GetContainsImage());
  EXPECT_FALSE(feature.GetIsSameHost());
  EXPECT_FALSE(feature.GetIsUrlIncrementedByOne());

  // Scroll down to the anchor element.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight * 1.5), kProgrammaticScroll);

  auto feature2 = AnchorElementMetrics::CreateFrom(anchor_element).value();
  EXPECT_LT(0, feature2.GetRatioVisibleArea());
  EXPECT_FLOAT_EQ(0.5, feature2.GetRatioDistanceTopToVisibleTop());
  EXPECT_LT(0.5, feature2.GetRatioDistanceCenterToVisibleTop());
  EXPECT_FLOAT_EQ(2, feature2.GetRatioDistanceRootTop());
  EXPECT_FLOAT_EQ(10, feature2.GetRatioDistanceRootBottom());
}

// The main frame contains an iframe. The iframe contains an anchor element.
// Features of the element are extracted.
// Then the test scrolls down in the main frame to check features again.
// Then the test scrolls down in the iframe to check features again.
TEST_F(AnchorElementMetricsTest, AnchorFeatureInIframe) {
  SimRequest main_resource("https://example.com/page1", "text/html");
  SimRequest iframe_resource("https://example.com/iframe.html", "text/html");
  SimRequest image_resource("https://example.com/cat.png", "image/png");

  LoadURL("https://example.com/page1");

  main_resource.Complete(String::Format(
      R"HTML(
        <body style='margin: 0px'>
        <div style='height: %dpx;'></div>
        <iframe id='iframe' src='https://example.com/iframe.html'
            style='width: 300px; height: %dpx;
            border-style: none; padding: 0px; margin: 0px;'></iframe>
        <div style='height: %dpx;'></div>
        </body>)HTML",
      2 * kViewportHeight, kViewportHeight / 2, 10 * kViewportHeight));

  iframe_resource.Complete(String::Format(
      R"HTML(
    <body style='margin: 0px'>
    <div style='height: %dpx;'></div>
    <a id='anchor' href="https://example.com/page2">example</a>
    <div style='height: %dpx;'></div>
    </body>)HTML",
      kViewportHeight / 2, 5 * kViewportHeight));

  Element* iframe = GetDocument().getElementById("iframe");
  HTMLIFrameElement* iframe_element = ToHTMLIFrameElement(iframe);
  Frame* sub = iframe_element->ContentFrame();
  LocalFrame* subframe = ToLocalFrame(sub);

  Element* anchor = subframe->GetDocument()->getElementById("anchor");
  HTMLAnchorElement* anchor_element = ToHTMLAnchorElement(anchor);

  auto feature = AnchorElementMetrics::CreateFrom(anchor_element).value();
  EXPECT_LT(0, feature.GetRatioArea());
  EXPECT_FLOAT_EQ(0, feature.GetRatioVisibleArea());
  EXPECT_FLOAT_EQ(2.5, feature.GetRatioDistanceTopToVisibleTop());
  EXPECT_LT(2.5, feature.GetRatioDistanceCenterToVisibleTop());
  EXPECT_FLOAT_EQ(2.5, feature.GetRatioDistanceRootTop());
  EXPECT_TRUE(feature.GetIsInIframe());
  EXPECT_FALSE(feature.GetContainsImage());
  EXPECT_TRUE(feature.GetIsSameHost());
  EXPECT_TRUE(feature.GetIsUrlIncrementedByOne());

  // Scroll down the main frame.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight * 1.8), kProgrammaticScroll);

  auto feature2 = AnchorElementMetrics::CreateFrom(anchor_element).value();
  EXPECT_LT(0, feature2.GetRatioVisibleArea());
  EXPECT_FLOAT_EQ(0.7, feature2.GetRatioDistanceTopToVisibleTop());
  EXPECT_FLOAT_EQ(2.5, feature2.GetRatioDistanceRootTop());

  // Scroll down inside iframe. Now the anchor element is visible.
  subframe->View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, kViewportHeight * 0.2), kProgrammaticScroll);

  auto feature3 = AnchorElementMetrics::CreateFrom(anchor_element).value();
  EXPECT_LT(0, feature3.GetRatioVisibleArea());
  EXPECT_FLOAT_EQ(0.5, feature3.GetRatioDistanceTopToVisibleTop());
  EXPECT_FLOAT_EQ(2.5, feature3.GetRatioDistanceRootTop());
  // The distance is expected to be 10.2 - height of the anchor element.
  EXPECT_GT(10.2, feature3.GetRatioDistanceRootBottom());
}

}  // namespace blink
