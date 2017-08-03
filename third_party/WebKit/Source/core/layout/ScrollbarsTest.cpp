// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "core/testing/sim/SimDisplayItemList.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/WebThemeEngine.h"
#include "public/web/WebScriptSource.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class ScrollbarsTest : private ScopedRootLayerScrollingForTest, public SimTest {
 public:
  ScrollbarsTest() : ScopedRootLayerScrollingForTest(true) {}
};

TEST_F(ScrollbarsTest, DocumentStyleRecalcPreservesScrollbars) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete("<style> body { width: 1600px; height: 1200px; } </style>");
  PaintLayerScrollableArea* plsa =
      GetDocument().GetLayoutView()->GetScrollableArea();

  Compositor().BeginFrame();
  ASSERT_TRUE(plsa->VerticalScrollbar() && plsa->HorizontalScrollbar());

  // Forces recalc of LayoutView's computed style in Document::updateStyle,
  // without invalidating layout.
  MainFrame().ExecuteScriptAndReturnValue(WebScriptSource(
      "document.querySelector('style').sheet.insertRule('body {}', 1);"));

  Compositor().BeginFrame();
  ASSERT_TRUE(plsa->VerticalScrollbar() && plsa->HorizontalScrollbar());
}

// Ensure that causing a change in scrollbar existence causes a nested layout
// to recalculate the existence of the opposite scrollbar. The bug here was
// caused by trying to avoid the layout when overlays are enabled but not
// checking whether the scrollbars should be custom - which do take up layout
// space. https://crbug.com/668387.
TEST_F(ScrollbarsTest, CustomScrollbarsCauseLayoutOnExistenceChange) {
  // The bug reproduces only with RLS off. When RLS ships we can keep the test
  // but remove this setting.
  ScopedRootLayerScrollingForTest turn_off_root_layer_scrolling(false);

  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  DCHECK(ScrollbarTheme::GetTheme().UsesOverlayScrollbars());

  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
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

  ScrollableArea* layout_viewport =
      GetDocument().View()->LayoutViewportScrollableArea();

  Compositor().BeginFrame();
  ASSERT_FALSE(layout_viewport->VerticalScrollbar());
  ASSERT_FALSE(layout_viewport->HorizontalScrollbar());

  // Adding translation will cause a vertical scrollbar to appear but not dirty
  // layout otherwise. Ensure the change of scrollbar causes a layout to
  // recalculate the page width with the vertical scrollbar added.
  MainFrame().ExecuteScript(WebScriptSource(
      "document.getElementById('box').className = 'box transformed';"));
  Compositor().BeginFrame();

  ASSERT_TRUE(layout_viewport->VerticalScrollbar());
  ASSERT_FALSE(layout_viewport->HorizontalScrollbar());
}

TEST_F(ScrollbarsTest, TransparentBackgroundUsesDarkOverlayColorTheme) {
  // The bug reproduces only with RLS off. When RLS ships we can keep the test
  // but remove this setting.
  ScopedRootLayerScrollingForTest turn_off_root_layer_scrolling(false);

  // This test is specifically checking the behavior when overlay scrollbars
  // are enabled.
  DCHECK(ScrollbarTheme::GetTheme().UsesOverlayScrollbars());

  WebView().Resize(WebSize(800, 600));
  WebView().SetBaseBackgroundColorOverride(SK_ColorTRANSPARENT);
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<!DOCTYPE html>"
      "<style>"
      "  body{"
      "    height: 300%;"
      "  }"
      "</style>");
  Compositor().BeginFrame();

  ScrollableArea* layout_viewport =
      GetDocument().View()->LayoutViewportScrollableArea();

  EXPECT_EQ(kScrollbarOverlayColorThemeDark,
            layout_viewport->GetScrollbarOverlayColorTheme());
}

typedef bool TestParamOverlayScrollbar;
class ScrollbarAppearanceTest
    : public SimTest,
      public ::testing::WithParamInterface<TestParamOverlayScrollbar> {
 public:
  // Use real scrollbars to ensure we're testing the real ScrollbarThemes.
  ScrollbarAppearanceTest() : mock_scrollbars_(false, GetParam()) {}

 private:
  FrameTestHelpers::UseMockScrollbarSettings mock_scrollbars_;
};

class StubWebThemeEngine : public WebThemeEngine {
 public:
  WebSize GetSize(Part part) override {
    switch (part) {
      case kPartScrollbarHorizontalThumb:
        return blink::WebSize(kMinimumHorizontalLength, 15);
      case kPartScrollbarVerticalThumb:
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
  WebThemeEngine* ThemeEngine() override { return &stub_theme_engine_; }

 private:
  StubWebThemeEngine stub_theme_engine_;
};

// Test both overlay and non-overlay scrollbars.
INSTANTIATE_TEST_CASE_P(All, ScrollbarAppearanceTest, ::testing::Bool());

#if !defined(OS_MACOSX)
// Ensure that the minimum length for a scrollbar thumb comes from the
// WebThemeEngine. Note, Mac scrollbars differ from all other platforms so this
// test doesn't apply there. https://crbug.com/682209.
TEST_P(ScrollbarAppearanceTest, ThemeEngineDefinesMinimumThumbLength) {
  ScopedTestingPlatformSupport<ScrollbarTestingPlatformSupport> platform;

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<style> body { width: 1000000px; height: 1000000px; } </style>");
  ScrollableArea* scrollable_area =
      GetDocument().View()->LayoutViewportScrollableArea();

  Compositor().BeginFrame();
  ASSERT_TRUE(scrollable_area->VerticalScrollbar());
  ASSERT_TRUE(scrollable_area->HorizontalScrollbar());

  ScrollbarTheme& theme = scrollable_area->VerticalScrollbar()->GetTheme();
  EXPECT_EQ(StubWebThemeEngine::kMinimumHorizontalLength,
            theme.ThumbLength(*scrollable_area->HorizontalScrollbar()));
  EXPECT_EQ(StubWebThemeEngine::kMinimumVerticalLength,
            theme.ThumbLength(*scrollable_area->VerticalScrollbar()));
}

// Ensure thumb position is correctly calculated even at ridiculously large
// scales.
TEST_P(ScrollbarAppearanceTest, HugeScrollingThumbPosition) {
  ScopedTestingPlatformSupport<ScrollbarTestingPlatformSupport> platform;

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(1000, 1000));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<style> body { margin: 0px; height: 10000000px; } </style>");
  ScrollableArea* scrollable_area =
      GetDocument().View()->LayoutViewportScrollableArea();

  Compositor().BeginFrame();

  scrollable_area->SetScrollOffset(ScrollOffset(0, 10000000),
                                   kProgrammaticScroll);

  int scroll_y = scrollable_area->GetScrollOffset().Height();
  ASSERT_EQ(9999000, scroll_y);

  Scrollbar* scrollbar = scrollable_area->VerticalScrollbar();
  ASSERT_TRUE(scrollbar);

  int maximumThumbPosition =
      WebView().Size().height - StubWebThemeEngine::kMinimumVerticalLength;

  EXPECT_EQ(maximumThumbPosition,
            scrollbar->GetTheme().ThumbPosition(*scrollbar));
}
#endif

}  // namespace

}  // namespace blink
