// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/TextAutosizer.h"
#include "core/loader/EmptyClients.h"
#include "platform/PlatformFrameView.h"
#include "platform/geometry/IntRect.h"
#include "public/platform/WebFloatRect.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
class TextAutosizerClient : public EmptyChromeClient {
 public:
  static TextAutosizerClient* Create() { return new TextAutosizerClient; }
  float WindowToViewportScalar(const float value) const override {
    return value * device_scale_factor_;
  }
  IntRect ViewportToScreen(const IntRect& rect,
                           const PlatformFrameView* view) const override {
    IntRect scaled_rect(rect);
    scaled_rect.Scale(1 / device_scale_factor_);
    return scaled_rect;
  }
  void set_device_scale_factor(float device_scale_factor) {
    device_scale_factor_ = device_scale_factor;
  }

 private:
  float device_scale_factor_;
};

class TextAutosizerTest : public RenderingTest {
 public:
  ChromeClient& GetChromeClient() const override {
    return GetTextAutosizerClient();
  }
  TextAutosizerClient& GetTextAutosizerClient() const {
    DEFINE_STATIC_LOCAL(TextAutosizerClient, client,
                        (TextAutosizerClient::Create()));
    return client;
  }
  void set_device_scale_factor(float device_scale_factor) {
    GetTextAutosizerClient().set_device_scale_factor(device_scale_factor);

    // This fake ChromeClient cannot update device scale factor (DSF). We apply
    // DSF to the zoom factor manually.
    GetDocument().GetFrame()->SetPageZoomFactor(device_scale_factor);
  }

 private:
  void SetUp() override {
    GetTextAutosizerClient().set_device_scale_factor(1.f);
    RenderingTest::SetUp();
    GetDocument().GetSettings()->SetTextAutosizingEnabled(true);
    GetDocument().GetSettings()->SetTextAutosizingWindowSizeOverride(
        IntSize(320, 480));
  }
};

TEST_F(TextAutosizerTest, SimpleParagraph) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='autosized'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  Element* autosized = GetDocument().getElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->GetLayoutObject()->Style()->SpecifiedFontSize());
  // (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  EXPECT_FLOAT_EQ(40.f,
                  autosized->GetLayoutObject()->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, TextSizeAdjustDisablesAutosizing) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='textSizeAdjustAuto' style='text-size-adjust: auto;'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
    <div id='textSizeAdjustNone' style='text-size-adjust: none;'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
    <div id='textSizeAdjust100' style='text-size-adjust: 100%;'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  LayoutObject* text_size_adjust_auto =
      GetDocument().getElementById("textSizeAdjustAuto")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, text_size_adjust_auto->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(40.f, text_size_adjust_auto->Style()->ComputedFontSize());
  LayoutObject* text_size_adjust_none =
      GetDocument().getElementById("textSizeAdjustNone")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, text_size_adjust_none->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(16.f, text_size_adjust_none->Style()->ComputedFontSize());
  LayoutObject* text_size_adjust100 =
      GetDocument().getElementById("textSizeAdjust100")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, text_size_adjust100->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(16.f, text_size_adjust100->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, ParagraphWithChangingTextSizeAdjustment) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
      .none { text-size-adjust: none; }
      .small { text-size-adjust: 50%; }
      .large { text-size-adjust: 150%; }
    </style>
    <div id='autosized'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  Element* autosized_div = GetDocument().getElementById("autosized");
  EXPECT_FLOAT_EQ(
      16.f, autosized_div->GetLayoutObject()->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(
      40.f, autosized_div->GetLayoutObject()->Style()->ComputedFontSize());

  autosized_div->setAttribute(HTMLNames::classAttr, "none");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FLOAT_EQ(
      16.f, autosized_div->GetLayoutObject()->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(
      16.f, autosized_div->GetLayoutObject()->Style()->ComputedFontSize());

  autosized_div->setAttribute(HTMLNames::classAttr, "small");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FLOAT_EQ(
      16.f, autosized_div->GetLayoutObject()->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(
      8.f, autosized_div->GetLayoutObject()->Style()->ComputedFontSize());

  autosized_div->setAttribute(HTMLNames::classAttr, "large");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FLOAT_EQ(
      16.f, autosized_div->GetLayoutObject()->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(
      24.f, autosized_div->GetLayoutObject()->Style()->ComputedFontSize());

  autosized_div->removeAttribute(HTMLNames::classAttr);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FLOAT_EQ(
      16.f, autosized_div->GetLayoutObject()->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(
      40.f, autosized_div->GetLayoutObject()->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, ZeroTextSizeAdjustment) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='textSizeAdjustZero' style='text-size-adjust: 0%;'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  LayoutObject* text_size_adjust_zero =
      GetDocument().getElementById("textSizeAdjustZero")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, text_size_adjust_zero->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(0.f, text_size_adjust_zero->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, NegativeTextSizeAdjustment) {
  SetBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "</style>"
      // Negative values should be treated as auto.
      "<div id='textSizeAdjustNegative' style='text-size-adjust: -10%;'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>");
  LayoutObject* text_size_adjust_negative =
      GetDocument().getElementById("textSizeAdjustNegative")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f,
                  text_size_adjust_negative->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(40.f, text_size_adjust_negative->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, TextSizeAdjustmentPixelUnits) {
  SetBodyInnerHTML(
      "<style>"
      "  html { font-size: 16px; }"
      "  body { width: 800px; margin: 0; overflow-y: hidden; }"
      "</style>"
      // Non-percentage values should be treated as auto.
      "<div id='textSizeAdjustPixels' style='text-size-adjust: 0.1px;'>"
      "  Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "  eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "  ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "  aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "  reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "  pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "  culpa qui officia deserunt mollit anim id est laborum."
      "</div>");
  LayoutObject* text_size_adjust_pixels =
      GetDocument().getElementById("textSizeAdjustPixels")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, text_size_adjust_pixels->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(40.f, text_size_adjust_pixels->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, NestedTextSizeAdjust) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='textSizeAdjustA' style='text-size-adjust: 47%;'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
      <div id='textSizeAdjustB' style='text-size-adjust: 53%;'>
        Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
        eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
        ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
        aliquip ex ea commodo consequat. Duis aute irure dolor in
        reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
        pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
        culpa qui officia deserunt mollit anim id est laborum.
      </div>
    </div>
  )HTML");
  LayoutObject* text_size_adjust_a =
      GetDocument().getElementById("textSizeAdjustA")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, text_size_adjust_a->Style()->SpecifiedFontSize());
  // 16px * 47% = 7.52
  EXPECT_FLOAT_EQ(7.52f, text_size_adjust_a->Style()->ComputedFontSize());
  LayoutObject* text_size_adjust_b =
      GetDocument().getElementById("textSizeAdjustB")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, text_size_adjust_b->Style()->SpecifiedFontSize());
  // 16px * 53% = 8.48
  EXPECT_FLOAT_EQ(8.48f, text_size_adjust_b->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, PrefixedTextSizeAdjustIsAlias) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='textSizeAdjust' style='-webkit-text-size-adjust: 50%;'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  LayoutObject* text_size_adjust =
      GetDocument().getElementById("textSizeAdjust")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, text_size_adjust->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(8.f, text_size_adjust->Style()->ComputedFontSize());
  EXPECT_FLOAT_EQ(.5f,
                  text_size_adjust->Style()->GetTextSizeAdjust().Multiplier());
}

TEST_F(TextAutosizerTest, AccessibilityFontScaleFactor) {
  GetDocument().GetSettings()->SetAccessibilityFontScaleFactor(1.5);
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='autosized'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  Element* autosized = GetDocument().getElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->GetLayoutObject()->Style()->SpecifiedFontSize());
  // 1.5 * (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 60px.
  EXPECT_FLOAT_EQ(60.f,
                  autosized->GetLayoutObject()->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, AccessibilityFontScaleFactorWithTextSizeAdjustNone) {
  GetDocument().GetSettings()->SetAccessibilityFontScaleFactor(1.5);
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
      #autosized { width: 400px; text-size-adjust: 100%; }
      #notAutosized { width: 100px; text-size-adjust: 100%; }
    </style>
    <div id='autosized'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
    <div id='notAutosized'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  Element* autosized = GetDocument().getElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->GetLayoutObject()->Style()->SpecifiedFontSize());
  // 1.5 * (specified font-size = 16px) = 24px.
  EXPECT_FLOAT_EQ(24.f,
                  autosized->GetLayoutObject()->Style()->ComputedFontSize());

  // Because this does not autosize (due to the width), no accessibility font
  // scale factor should be applied.
  Element* not_autosized = GetDocument().getElementById("notAutosized");
  EXPECT_FLOAT_EQ(
      16.f, not_autosized->GetLayoutObject()->Style()->SpecifiedFontSize());
  // specified font-size = 16px.
  EXPECT_FLOAT_EQ(
      16.f, not_autosized->GetLayoutObject()->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, ChangingAccessibilityFontScaleFactor) {
  GetDocument().GetSettings()->SetAccessibilityFontScaleFactor(1);
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='autosized'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  Element* autosized = GetDocument().getElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->GetLayoutObject()->Style()->SpecifiedFontSize());
  // 1.0 * (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  EXPECT_FLOAT_EQ(40.f,
                  autosized->GetLayoutObject()->Style()->ComputedFontSize());

  GetDocument().GetSettings()->SetAccessibilityFontScaleFactor(2);
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_FLOAT_EQ(16.f,
                  autosized->GetLayoutObject()->Style()->SpecifiedFontSize());
  // 2.0 * (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 80px.
  EXPECT_FLOAT_EQ(80.f,
                  autosized->GetLayoutObject()->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, TextSizeAdjustDoesNotDisableAccessibility) {
  GetDocument().GetSettings()->SetAccessibilityFontScaleFactor(1.5);
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='textSizeAdjustNone' style='text-size-adjust: none;'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
    <div id='textSizeAdjustDouble' style='text-size-adjust: 200%;'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");
  Element* text_size_adjust_none =
      GetDocument().getElementById("textSizeAdjustNone");
  EXPECT_FLOAT_EQ(
      16.f,
      text_size_adjust_none->GetLayoutObject()->Style()->SpecifiedFontSize());
  // 1.5 * (specified font-size = 16px) = 24px.
  EXPECT_FLOAT_EQ(
      24.f,
      text_size_adjust_none->GetLayoutObject()->Style()->ComputedFontSize());

  Element* text_size_adjust_double =
      GetDocument().getElementById("textSizeAdjustDouble");
  EXPECT_FLOAT_EQ(
      16.f,
      text_size_adjust_double->GetLayoutObject()->Style()->SpecifiedFontSize());
  // 1.5 * (specified font-size = 16px) * (text size adjustment = 2) = 48px.
  EXPECT_FLOAT_EQ(
      48.f,
      text_size_adjust_double->GetLayoutObject()->Style()->ComputedFontSize());

  // Changing the accessibility font scale factor should change the adjusted
  // size.
  GetDocument().GetSettings()->SetAccessibilityFontScaleFactor(2);
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_FLOAT_EQ(
      16.f,
      text_size_adjust_none->GetLayoutObject()->Style()->SpecifiedFontSize());
  // 2.0 * (specified font-size = 16px) = 32px.
  EXPECT_FLOAT_EQ(
      32.f,
      text_size_adjust_none->GetLayoutObject()->Style()->ComputedFontSize());

  EXPECT_FLOAT_EQ(
      16.f,
      text_size_adjust_double->GetLayoutObject()->Style()->SpecifiedFontSize());
  // 2.0 * (specified font-size = 16px) * (text size adjustment = 2) = 64px.
  EXPECT_FLOAT_EQ(
      64.f,
      text_size_adjust_double->GetLayoutObject()->Style()->ComputedFontSize());
}

// https://crbug.com/646237
TEST_F(TextAutosizerTest, DISABLED_TextSizeAdjustWithoutNeedingAutosizing) {
  GetDocument().GetSettings()->SetTextAutosizingWindowSizeOverride(
      IntSize(800, 600));
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='textSizeAdjust' style='text-size-adjust: 150%;'>
      Text
    </div>
  )HTML");

  LayoutObject* text_size_adjust =
      GetDocument().getElementById("textSizeAdjust")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, text_size_adjust->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(24.f, text_size_adjust->Style()->ComputedFontSize());
  EXPECT_FLOAT_EQ(1.5f,
                  text_size_adjust->Style()->GetTextSizeAdjust().Multiplier());
}

TEST_F(TextAutosizerTest, DeviceScaleAdjustmentWithViewport) {
  SetBodyInnerHTML(R"HTML(
    <meta name='viewport' content='width=800'>
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='autosized'>
      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do
      eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim
      ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut
      aliquip ex ea commodo consequat. Duis aute irure dolor in
      reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla
      pariatur. Excepteur sint occaecat cupidatat non proident, sunt in
      culpa qui officia deserunt mollit anim id est laborum.
    </div>
  )HTML");

  GetDocument().GetSettings()->SetViewportMetaEnabled(true);
  GetDocument().GetSettings()->SetDeviceScaleAdjustment(1.5f);
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* autosized = GetDocument().getElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->GetLayoutObject()->Style()->SpecifiedFontSize());
  // (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  // The device scale adjustment of 1.5 is ignored.
  EXPECT_FLOAT_EQ(40.f,
                  autosized->GetLayoutObject()->Style()->ComputedFontSize());

  GetDocument().GetSettings()->SetViewportMetaEnabled(false);
  GetDocument().View()->UpdateAllLifecyclePhases();

  autosized = GetDocument().getElementById("autosized");
  EXPECT_FLOAT_EQ(16.f,
                  autosized->GetLayoutObject()->Style()->SpecifiedFontSize());
  // (device scale adjustment = 1.5) * (specified font-size = 16px) *
  // (viewport width = 800px) / (window width = 320px) = 60px.
  EXPECT_FLOAT_EQ(60.f,
                  autosized->GetLayoutObject()->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, ChangingSuperClusterFirstText) {
  SetBodyInnerHTML(R"HTML(
    <meta name='viewport' content='width=800'>
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
      .supercluster { width:560px; }
    </style>
    <div class='supercluster'>
      <div id='longText'>short blah blah</div>
    </div>
    <div class='supercluster'>
      <div id='shortText'>short blah blah</div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* long_text_element = GetDocument().getElementById("longText");
  long_text_element->SetInnerHTMLFromString(
      "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed "
      "do eiusmod tempor"
      "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
      "veniam, quis nostrud"
      "    exercitation ullamco laboris nisi ut aliquip ex ea commodo "
      "consequat. Duis aute irure"
      "    dolor in reprehenderit in voluptate velit esse cillum dolore eu "
      "fugiat nulla pariatur."
      "    Excepteur sint occaecat cupidatat non proident, sunt in culpa "
      "qui officia deserunt"
      "    mollit anim id est laborum.",
      ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  LayoutObject* long_text =
      GetDocument().getElementById("longText")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, long_text->Style()->SpecifiedFontSize());
  //(specified font-size = 16px) * (block width = 560px) /
  // (window width = 320px) = 28px.
  EXPECT_FLOAT_EQ(28.f, long_text->Style()->ComputedFontSize());
  LayoutObject* short_text =
      GetDocument().getElementById("shortText")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, short_text->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(28.f, short_text->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, ChangingSuperClusterSecondText) {
  SetBodyInnerHTML(R"HTML(
    <meta name='viewport' content='width=800'>
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
      .supercluster { width:560px; }
    </style>
    <div class='supercluster'>
      <div id='shortText'>short blah blah</div>
    </div>
    <div class='supercluster'>
      <div id='longText'>short blah blah</div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* long_text_element = GetDocument().getElementById("longText");
  long_text_element->SetInnerHTMLFromString(
      "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed "
      "do eiusmod tempor"
      "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
      "veniam, quis nostrud"
      "    exercitation ullamco laboris nisi ut aliquip ex ea commodo "
      "consequat. Duis aute irure"
      "    dolor in reprehenderit in voluptate velit esse cillum dolore eu "
      "fugiat nulla pariatur."
      "    Excepteur sint occaecat cupidatat non proident, sunt in culpa "
      "qui officia deserunt"
      "    mollit anim id est laborum.",
      ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  LayoutObject* long_text =
      GetDocument().getElementById("longText")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, long_text->Style()->SpecifiedFontSize());
  //(specified font-size = 16px) * (block width = 560px) /
  // (window width = 320px) = 28px.
  EXPECT_FLOAT_EQ(28.f, long_text->Style()->ComputedFontSize());
  LayoutObject* short_text =
      GetDocument().getElementById("shortText")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, short_text->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(28.f, short_text->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, AddingSuperCluster) {
  SetBodyInnerHTML(R"HTML(
    <meta name='viewport' content='width=800'>
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
      .supercluster { width:560px; }
    </style>
    <div>
      <div class='supercluster' id='shortText'>
          short blah blah
      </div>
    </div>
    <div id='container'></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* container = GetDocument().getElementById("container");
  container->SetInnerHTMLFromString(
      "<div class='supercluster' id='longText'>"
      "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed "
      "do eiusmod tempor"
      "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
      "veniam, quis nostrud"
      "    exercitation ullamco laboris nisi ut aliquip ex ea commodo "
      "consequat. Duis aute irure"
      "    dolor in reprehenderit in voluptate velit esse cillum dolore eu "
      "fugiat nulla pariatur."
      "    Excepteur sint occaecat cupidatat non proident, sunt in culpa "
      "qui officia deserunt"
      "    mollit anim id est laborum."
      "</div>",
      ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  LayoutObject* long_text =
      GetDocument().getElementById("longText")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, long_text->Style()->SpecifiedFontSize());
  //(specified font-size = 16px) * (block width = 560px) /
  // (window width = 320px) = 28px.
  EXPECT_FLOAT_EQ(28.f, long_text->Style()->ComputedFontSize());
  LayoutObject* short_text =
      GetDocument().getElementById("shortText")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, short_text->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(28.f, short_text->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, ChangingInheritedClusterTextInsideSuperCluster) {
  SetBodyInnerHTML(R"HTML(
    <meta name='viewport' content='width=800'>
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
      .supercluster { width:560px; }
      .cluster{width:560px;}
    </style>
    <div class='supercluster'>
      <div class='cluster' id='longText'>short blah blah</div>
    </div>
    <div class='supercluster'>
      <div class='cluster' id='shortText'>short blah blah</div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* long_text_element = GetDocument().getElementById("longText");
  long_text_element->SetInnerHTMLFromString(
      "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed "
      "do eiusmod tempor"
      "    incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
      "veniam, quis nostrud"
      "    exercitation ullamco laboris nisi ut aliquip ex ea commodo "
      "consequat. Duis aute irure"
      "    dolor in reprehenderit in voluptate velit esse cillum dolore eu "
      "fugiat nulla pariatur."
      "    Excepteur sint occaecat cupidatat non proident, sunt in culpa "
      "qui officia deserunt"
      "    mollit anim id est laborum.",
      ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  LayoutObject* long_text =
      GetDocument().getElementById("longText")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, long_text->Style()->SpecifiedFontSize());
  //(specified font-size = 16px) * (block width = 560px) /
  // (window width = 320px) = 28px.
  EXPECT_FLOAT_EQ(28.f, long_text->Style()->ComputedFontSize());
  LayoutObject* short_text =
      GetDocument().getElementById("shortText")->GetLayoutObject();
  EXPECT_FLOAT_EQ(16.f, short_text->Style()->SpecifiedFontSize());
  EXPECT_FLOAT_EQ(28.f, short_text->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, AutosizeInnerContentOfRuby) {
  SetBodyInnerHTML(R"HTML(
    <meta name='viewport' content='width=800'>
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
    </style>
    <div id='autosized'>
      東京特許許可局許可局長　今日
      <ruby>
        <rb id='rubyInline'>急遽</rb>
        <rp>(</rp>
        <rt>きゅうきょ</rt>
        <rp>)</rp>
      </ruby>
      許可却下、<br><br>
      <span>
          Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec
          sed diam facilisis, elementum elit at, elementum sem. Aliquam
          consectetur leo at nisi fermentum, vitae maximus libero
    sodales. Sed
          laoreet congue ipsum, at tincidunt ante tempor sed. Cras eget
    erat
          mattis urna vestibulum porta. Sed tempus vitae dui et suscipit.
          Curabitur laoreet accumsan pharetra. Nunc facilisis, elit sit
    amet
          sollicitudin condimentum, ipsum velit ultricies mi, eget
    dapibus nunc
          nulla nec sapien. Fusce dictum imperdiet aliquet.
      </span>
      <ruby style='display:block'>
        <rb id='rubyBlock'>拼音</rb>
        <rt>pin yin</rt>
      </ruby>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* ruby_inline = GetDocument().getElementById("rubyInline");
  EXPECT_FLOAT_EQ(16.f,
                  ruby_inline->GetLayoutObject()->Style()->SpecifiedFontSize());
  // (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  EXPECT_FLOAT_EQ(40.f,
                  ruby_inline->GetLayoutObject()->Style()->ComputedFontSize());

  Element* ruby_block = GetDocument().getElementById("rubyBlock");
  EXPECT_FLOAT_EQ(16.f,
                  ruby_block->GetLayoutObject()->Style()->SpecifiedFontSize());
  // (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  EXPECT_FLOAT_EQ(40.f,
                  ruby_block->GetLayoutObject()->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, ResizeAndGlyphOverflowChanged) {
  GetDocument().GetSettings()->SetTextAutosizingWindowSizeOverride(
      IntSize(360, 640));
  Element* html = GetDocument().body()->parentElement();
  html->SetInnerHTMLFromString(
      "<head>"
      "  <meta name='viewport' content='width=800'>"
      "  <style>"
      "    html { font-size:16px; font-family:'Times New Roman';}"
      "  </style>"
      "</head>"
      "<body>"
      "  <span id='autosized' style='font-size:10px'>"
      "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "    eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "    ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "    aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "    reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "    pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "    culpa qui officia deserunt mollit anim id est laborum."
      "  </span>"
      "  <span style='font-size:8px'>n</span>"
      "  <span style='font-size:9px'>n</span>"
      "  <span style='font-size:10px'>n</span>"
      "  <span style='font-size:11px'>n</span>"
      "  <span style='font-size:12px'>n</span>"
      "  <span style='font-size:13px'>n</span>"
      "  <span style='font-size:14px'>n</span>"
      "  <span style='font-size:15px'>n</span>"
      "</body>",
      ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  GetDocument().GetSettings()->SetTextAutosizingWindowSizeOverride(
      IntSize(640, 360));
  GetDocument().View()->UpdateAllLifecyclePhases();

  GetDocument().GetSettings()->SetTextAutosizingWindowSizeOverride(
      IntSize(360, 640));
  GetDocument().View()->UpdateAllLifecyclePhases();
}

TEST_F(TextAutosizerTest, narrowContentInsideNestedWideBlock) {
  Element* html = GetDocument().body()->parentElement();
  html->SetInnerHTMLFromString(
      "<head>"
      "  <meta name='viewport' content='width=800'>"
      "  <style>"
      "    html { font-size:16px;}"
      "  </style>"
      "</head>"
      "<body>"
      "  <div style='width:800px'>"
      "    <div style='width:800px'>"
      "      <div style='width:200px' id='content'>"
      "        Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed "
      "        do eiusmod tempor incididunt ut labore et dolore magna aliqua."
      "        Ut enim ad minim veniam, quis nostrud exercitation ullamco "
      "        laboris nisi ut aliquip ex ea commodo consequat. Duis aute "
      "        irure dolor in reprehenderit in voluptate velit esse cillum "
      "        dolore eu fugiat nulla pariatur. Excepteur sint occaecat "
      "        cupidatat non proident, sunt in culpa qui officia deserunt "
      "        mollit anim id est laborum."
      "      </div>"
      "    </div>"
      "    Content belong to first wide block."
      "  </div>"
      "</body>",
      ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* content = GetDocument().getElementById("content");
  //(content width = 200px) / (window width = 320px) < 1.0f, multiplier = 1.0,
  // font-size = 16px;
  EXPECT_FLOAT_EQ(16.f,
                  content->GetLayoutObject()->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, LayoutViewWidthProvider) {
  Element* html = GetDocument().body()->parentElement();
  html->SetInnerHTMLFromString(
      "<head>"
      "  <meta name='viewport' content='width=800'>"
      "  <style>"
      "    html { font-size:16px;}"
      "    #content {margin-left: 140px;}"
      "  </style>"
      "</head>"
      "<body>"
      "  <div id='content'>"
      "    Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do"
      "    eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim"
      "    ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut"
      "    aliquip ex ea commodo consequat. Duis aute irure dolor in"
      "    reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla"
      "    pariatur. Excepteur sint occaecat cupidatat non proident, sunt in"
      "    culpa qui officia deserunt mollit anim id est laborum."
      "  </div>"
      "  <div id='panel'></div>"
      "</body>",
      ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* content = GetDocument().getElementById("content");
  // (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  EXPECT_FLOAT_EQ(40.f,
                  content->GetLayoutObject()->Style()->ComputedFontSize());

  GetDocument().getElementById("panel")->SetInnerHTMLFromString("insert text");
  content->SetInnerHTMLFromString(content->InnerHTMLAsString());
  GetDocument().View()->UpdateAllLifecyclePhases();

  // (specified font-size = 16px) * (viewport width = 800px) /
  // (window width = 320px) = 40px.
  EXPECT_FLOAT_EQ(40.f,
                  content->GetLayoutObject()->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, MultiColumns) {
  Element* html = GetDocument().body()->parentElement();
  html->SetInnerHTMLFromString(
      "<head>"
      "  <meta name='viewport' content='width=800'>"
      "  <style>"
      "    html { font-size:16px;}"
      "    #mc {columns: 3;}"
      "  </style>"
      "</head>"
      "<body>"
      "  <div id='mc'>"
      "    <div id='target'>"
      "      Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed "
      "      do eiusmod tempor incididunt ut labore et dolore magna aliqua."
      "      Ut enim ad minim veniam, quis nostrud exercitation ullamco "
      "      laboris nisi ut aliquip ex ea commodo consequat. Duis aute "
      "      irure dolor in reprehenderit in voluptate velit esse cillum "
      "      dolore eu fugiat nulla pariatur. Excepteur sint occaecat "
      "      cupidatat non proident, sunt in culpa qui officia deserunt "
      "    </div>"
      "  </div>"
      "  <div> hello </div>"
      "</body>",
      ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* target = GetDocument().getElementById("target");
  // (specified font-size = 16px) * ( thread flow layout width = 800px / 3) /
  // (window width = 320px) < 16px.
  EXPECT_FLOAT_EQ(16.f, target->GetLayoutObject()->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, ScaledbyDSF) {
  const float device_scale = 3;
  set_device_scale_factor(device_scale);
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 16px; }
      body { width: 800px; margin: 0; overflow-y: hidden; }
      .target { width: 560px; }
    </style>
    <body>
      <div id='target'>
        Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed
        do eiusmod tempor incididunt ut labore et dolore magna aliqua.
        Ut enim ad minim veniam, quis nostrud exercitation ullamco
        laboris nisi ut aliquip ex ea commodo consequat. Duis aute
        irure dolor in reprehenderit in voluptate velit esse cillum
        dolore eu fugiat nulla pariatur. Excepteur sint occaecat
        cupidatat non proident, sunt in culpa qui officia deserunt
      </div>
    </body>
  )HTML");
  Element* target = GetDocument().getElementById("target");
  // (specified font-size = 16px) * (thread flow layout width = 800px) /
  // (window width = 320px) * (device scale factor) = 40px * device_scale.
  EXPECT_FLOAT_EQ(40.0f * device_scale,
                  target->GetLayoutObject()->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, ClusterHasNotEnoughTextToAutosizeForZoomDSF) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 8px; }
    </style>
    <body>
      <div id='target'>
        Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed
        do eiusmod tempor incididunt ut labore et dolore magna aliqua.
        Ut enim ad minim veniam, quis nostrud exercitation ullamco
        laboris nisi ut aliquip ex ea commodo consequat.
      </div>
    </body>
  )HTML");
  Element* target = GetDocument().getElementById("target");
  // ClusterHasEnoughTextToAutosize() returns false because
  // minimum_text_length_to_autosize < length. Thus, ClusterMultiplier()
  // returns 1 (not multiplied by the accessibility font scale factor).
  // computed font-size = specified font-size = 8px.
  EXPECT_FLOAT_EQ(8.0f, target->GetLayoutObject()->Style()->ComputedFontSize());
}

// TODO(jaebaek): Unit tests ClusterHasNotEnoughTextToAutosizeForZoomDSF and
// ClusterHasEnoughTextToAutosizeForZoomDSF must be updated.
// The return value of TextAutosizer::ClusterHasEnoughTextToAutosize() must not
// be the same regardless of DSF. In real world
// TextAutosizer::ClusterHasEnoughTextToAutosize(),
// minimum_text_length_to_autosize is in physical pixel scale. However, in
// these unit tests, it is in DIP scale, which makes
// ClusterHasEnoughTextToAutosizeForZoomDSF not fail. We need a trick to update
// the minimum_text_length_to_autosize in these unit test and check the return
// value change of TextAutosizer::ClusterHasEnoughTextToAutosize() depending on
// the length of text even when DSF is not 1 (e.g., letting DummyPageHolder
// update the view size according to the change of DSF).
TEST_F(TextAutosizerTest, ClusterHasEnoughTextToAutosizeForZoomDSF) {
  const float device_scale = 3;
  set_device_scale_factor(device_scale);
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 8px; }
    </style>
    <body>
      <div id='target'>
        Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed
        do eiusmod tempor incididunt ut labore et dolore magna aliqua.
        Ut enim ad minim veniam, quis nostrud exercitation ullamco
        laboris nisi ut aliquip ex ea commodo consequat.
      </div>
    </body>
  )HTML");
  Element* target = GetDocument().getElementById("target");
  // (specified font-size = 8px) * (thread flow layout width = 800px) /
  // (window width = 320px) * (device scale factor) = 20px * device_scale.
  // ClusterHasEnoughTextToAutosize() returns true and both accessibility font
  // scale factor and device scale factor are multiplied.
  EXPECT_FLOAT_EQ(20.0f * device_scale,
                  target->GetLayoutObject()->Style()->ComputedFontSize());
}

TEST_F(TextAutosizerTest, AfterPrint) {
  const float device_scale = 3;
  FloatSize print_size(160, 240);
  set_device_scale_factor(device_scale);
  SetBodyInnerHTML(R"HTML(
    <style>
      html { font-size: 8px; }
    </style>
    <body>
      <div id='target'>
        Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed
        do eiusmod tempor incididunt ut labore et dolore magna aliqua.
        Ut enim ad minim veniam, quis nostrud exercitation ullamco
        laboris nisi ut aliquip ex ea commodo consequat.
      </div>
    </body>
  )HTML");
  Element* target = GetDocument().getElementById("target");
  EXPECT_FLOAT_EQ(20.0f * device_scale,
                  target->GetLayoutObject()->Style()->ComputedFontSize());
  GetDocument().GetFrame()->StartPrinting(print_size, print_size, 1.0);
  EXPECT_FLOAT_EQ(8.0f, target->GetLayoutObject()->Style()->ComputedFontSize());
  GetDocument().GetFrame()->EndPrinting();
  EXPECT_FLOAT_EQ(20.0f * device_scale,
                  target->GetLayoutObject()->Style()->ComputedFontSize());
}
}  // namespace blink
