// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/FrameView.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/WebThemeEngine.h"
#include "public/web/WebScriptSource.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

namespace blink {

namespace {

class ScrollbarsTest : private ScopedRootLayerScrollingForTest, public SimTest {
 public:
  ScrollbarsTest() : ScopedRootLayerScrollingForTest(true) {}
};

TEST_F(ScrollbarsTest, DocumentStyleRecalcPreservesScrollbars) {
  v8::HandleScope handleScope(v8::Isolate::GetCurrent());
  webView().resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  loadURL("https://example.com/test.html");
  request.complete("<style> body { width: 1600px; height: 1200px; } </style>");
  PaintLayerScrollableArea* plsa = document().layoutView()->getScrollableArea();

  compositor().beginFrame();
  ASSERT_TRUE(plsa->verticalScrollbar() && plsa->horizontalScrollbar());

  // Forces recalc of LayoutView's computed style in Document::updateStyle,
  // without invalidating layout.
  mainFrame().executeScriptAndReturnValue(WebScriptSource(
      "document.querySelector('style').sheet.insertRule('body {}', 1);"));

  compositor().beginFrame();
  ASSERT_TRUE(plsa->verticalScrollbar() && plsa->horizontalScrollbar());
}

// Ensure that causing a change in scrollbar existence causes a nested layout
// to recalculate the existence of the opposite scrollbar. The bug here was
// caused by trying to avoid the layout when overlays are enabled but not
// checking whether the scrollbars should be custom - which do take up layout
// space. https://crbug.com/668387.
TEST_F(ScrollbarsTest, CustomScrollbarsCauseLayoutOnExistenceChange) {
  // The bug reproduces only with RLS off. When RLS ships we can keep the test
  // but remove this setting.
  ScopedRootLayerScrollingForTest turnOffRootLayerScrolling(false);

  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  DCHECK(ScrollbarTheme::theme().usesOverlayScrollbars());

  webView().resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  loadURL("https://example.com/test.html");
  request.complete(
      "<!DOCTYPE html>"
      "<style>"
      "  ::-webkit-scrollbar {"
      "      height: 16px;"
      "      width: 16px"
      "  }"
      "  ::-webkit-scrollbar-thumb {"
      "      background-color: rgba(0,0,0,.2);"
      "  }"
      "  html, body{"
      "    margin: 0;"
      "    height: 100%;"
      "  }"
      "  .box {"
      "    width: 100%;"
      "    height: 100%;"
      "  }"
      "  .transformed {"
      "    transform: translateY(100px);"
      "  }"
      "</style>"
      "<div id='box' class='box'></div>");

  ScrollableArea* layoutViewport =
      document().view()->layoutViewportScrollableArea();

  compositor().beginFrame();
  ASSERT_FALSE(layoutViewport->verticalScrollbar());
  ASSERT_FALSE(layoutViewport->horizontalScrollbar());

  // Adding translation will cause a vertical scrollbar to appear but not dirty
  // layout otherwise. Ensure the change of scrollbar causes a layout to
  // recalculate the page width with the vertical scrollbar added.
  mainFrame().executeScript(WebScriptSource(
      "document.getElementById('box').className = 'box transformed';"));
  compositor().beginFrame();

  ASSERT_TRUE(layoutViewport->verticalScrollbar());
  ASSERT_FALSE(layoutViewport->horizontalScrollbar());
}

typedef bool TestParamOverlayScrollbar;
class ScrollbarAppearanceTest
    : public SimTest,
      public ::testing::WithParamInterface<TestParamOverlayScrollbar> {
 public:
  // Use real scrollbars to ensure we're testing the real ScrollbarThemes.
  ScrollbarAppearanceTest() : m_mockScrollbars(false, GetParam()) {}

 private:
  FrameTestHelpers::UseMockScrollbarSettings m_mockScrollbars;
};

class StubWebThemeEngine : public WebThemeEngine {
 public:
  WebSize getSize(Part part) override {
    switch (part) {
      case PartScrollbarHorizontalThumb:
        return blink::WebSize(kMinimumHorizontalLength, 15);
      case PartScrollbarVerticalThumb:
        return blink::WebSize(15, kMinimumVerticalLength);
      default:
        return WebSize();
    }
  }
  static constexpr int kMinimumHorizontalLength = 51;
  static constexpr int kMinimumVerticalLength = 52;
};

constexpr int StubWebThemeEngine::kMinimumHorizontalLength;
constexpr int StubWebThemeEngine::kMinimumVerticalLength;

class ScrollbarTestingPlatformSupport : public TestingPlatformSupport {
 public:
  WebThemeEngine* themeEngine() override { return &m_stubThemeEngine; }

 private:
  StubWebThemeEngine m_stubThemeEngine;
};

// Test both overlay and non-overlay scrollbars.
INSTANTIATE_TEST_CASE_P(All, ScrollbarAppearanceTest, ::testing::Bool());

#if !OS(MACOSX)
// Ensure that the minimum length for a scrollbar thumb comes from the
// WebThemeEngine. Note, Mac scrollbars differ from all other platforms so this
// test doesn't apply there. https://crbug.com/682209.
TEST_P(ScrollbarAppearanceTest, ThemeEngineDefinesMinimumThumbLength) {
  ScopedTestingPlatformSupport<ScrollbarTestingPlatformSupport> platform;

  v8::HandleScope handleScope(v8::Isolate::GetCurrent());
  webView().resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  loadURL("https://example.com/test.html");
  request.complete(
      "<style> body { width: 1000000px; height: 1000000px; } </style>");
  ScrollableArea* scrollableArea =
      document().view()->layoutViewportScrollableArea();

  compositor().beginFrame();
  ASSERT_TRUE(scrollableArea->verticalScrollbar());
  ASSERT_TRUE(scrollableArea->horizontalScrollbar());

  ScrollbarTheme& theme = scrollableArea->verticalScrollbar()->theme();
  EXPECT_EQ(StubWebThemeEngine::kMinimumHorizontalLength,
            theme.thumbLength(*scrollableArea->horizontalScrollbar()));
  EXPECT_EQ(StubWebThemeEngine::kMinimumVerticalLength,
            theme.thumbLength(*scrollableArea->verticalScrollbar()));
}
#endif

}  // namespace

}  // namespace blink
