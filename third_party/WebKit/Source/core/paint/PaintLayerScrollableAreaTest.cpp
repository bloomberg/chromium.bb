// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintLayerScrollableArea.h"

#include "core/frame/LocalFrameView.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

namespace blink {
namespace {

class ScrollableAreaMockChromeClient : public EmptyChromeClient {
 public:
  MOCK_METHOD3(MockSetToolTip, void(LocalFrame*, const String&, TextDirection));
  void SetToolTip(LocalFrame& frame,
                  const String& tooltip_text,
                  TextDirection dir) override {
    MockSetToolTip(&frame, tooltip_text, dir);
  }
};

}  // namespace {

class PaintLayerScrollableAreaTest : public RenderingTest {
 public:
  PaintLayerScrollableAreaTest()
      : RenderingTest(EmptyLocalFrameClient::Create()),
        chrome_client_(new ScrollableAreaMockChromeClient) {}

  ~PaintLayerScrollableAreaTest() {
    ::testing::Mock::VerifyAndClearExpectations(&GetChromeClient());
  }

  ScrollableAreaMockChromeClient& GetChromeClient() const override {
    return *chrome_client_;
  }

  BackgroundPaintLocation GetBackgroundPaintLocation(const char* element_id) {
    PaintLayer* paint_layer =
        ToLayoutBoxModelObject(GetLayoutObjectByElementId(element_id))->Layer();
    return paint_layer->GetBackgroundPaintLocation();
  }

 private:
  void SetUp() override {
    RenderingTest::SetUp();
    EnableCompositing();
  }

  Persistent<ScrollableAreaMockChromeClient> chrome_client_;
};

TEST_F(PaintLayerScrollableAreaTest,
       CanPaintBackgroundOntoScrollingContentsLayer) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);
  SetBodyInnerHTML(
      "<style>"
      ".scroller { overflow: scroll; will-change: transform; width: 300px; "
      "height: 300px;} .spacer { height: 1000px; }"
      "#scroller13::-webkit-scrollbar { width: 13px; height: 13px;}"
      "</style>"
      "<div id='scroller1' class='scroller' style='background: white local;'>"
      "    <div id='negative-composited-child' style='background-color: red; "
      "width: 1px; height: 1px; position: absolute; backface-visibility: "
      "hidden; z-index: -1'></div>"
      "    <div class='spacer'></div>"
      "</div>"
      "<div id='scroller2' class='scroller' style='background: white "
      "content-box; padding: 10px;'><div class='spacer'></div></div>"
      "<div id='scroller3' class='scroller' style='background: white local "
      "content-box; padding: 10px;'><div class='spacer'></div></div>"
      "<div id='scroller4' class='scroller' style='background: "
      "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg), white local;'><div "
      "class='spacer'></div></div>"
      "<div id='scroller5' class='scroller' style='background: "
      "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) local, white "
      "local;'><div class='spacer'></div></div>"
      "<div id='scroller6' class='scroller' style='background: "
      "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) local, white "
      "padding-box; padding: 10px;'><div class='spacer'></div></div>"
      "<div id='scroller7' class='scroller' style='background: "
      "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) local, white "
      "content-box; padding: 10px;'><div class='spacer'></div></div>"
      "<div id='scroller8' class='scroller' style='background: white "
      "border-box;'><div class='spacer'></div></div>"
      "<div id='scroller9' class='scroller' style='background: white "
      "border-box; border: 10px solid black;'><div class='spacer'></div></div>"
      "<div id='scroller10' class='scroller' style='background: white "
      "border-box; border: 10px solid rgba(0, 0, 0, 0.5);'><div "
      "class='spacer'></div></div>"
      "<div id='scroller11' class='scroller' style='background: white "
      "content-box;'><div class='spacer'></div></div>"
      "<div id='scroller12' class='scroller' style='background: white "
      "content-box; padding: 10px;'><div class='spacer'></div></div>"
      "<div id='scroller13' class='scroller' style='background: white "
      "border-box;'><div class='spacer'></div></div>"
      "<div id='scroller14' class='scroller' style='background: white; border: "
      "1px solid black; outline: 1px solid blue; outline-offset: -1px;'><div "
      "class='spacer'></div></div>"
      "<div id='scroller15' class='scroller' style='background: white; border: "
      "1px solid black; outline: 1px solid blue; outline-offset: -2px;'><div "
      "class='spacer'></div></div>"
      "<div id='scroller16' class='scroller' style='background: white; clip: "
      "rect(0px,10px,10px,0px);'><div class='spacer'></div></div>"
      "<div id='scroller17' class='scroller' style='background:"
      "rgba(255, 255, 255, 0.5) border-box; border: 5px solid "
      "rgba(0, 0, 0, 0.5);'><div class='spacer'></div></div>");

  // #scroller1 cannot paint background into scrolling contents layer because it
  // has a negative z-index child.
  EXPECT_EQ(kBackgroundPaintInGraphicsLayer,
            GetBackgroundPaintLocation("scroller1"));

  // #scroller2 cannot paint background into scrolling contents layer because it
  // has a content-box clip without local attachment.
  EXPECT_EQ(kBackgroundPaintInGraphicsLayer,
            GetBackgroundPaintLocation("scroller2"));

  // #scroller3 can paint background into scrolling contents layer.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller3"));

  // #scroller4 cannot paint background into scrolling contents layer because
  // the background image is not locally attached.
  EXPECT_EQ(kBackgroundPaintInGraphicsLayer,
            GetBackgroundPaintLocation("scroller4"));

  // #scroller5 can paint background into scrolling contents layer because both
  // the image and color are locally attached.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller5"));

  // #scroller6 can paint background into scrolling contents layer because the
  // image is locally attached and even though the color is not, it is filled to
  // the padding box so it will be drawn the same as a locally attached
  // background.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller6"));

  // #scroller7 cannot paint background into scrolling contents layer because
  // the color is filled to the content box and we have padding so it is not
  // equivalent to a locally attached background.
  EXPECT_EQ(kBackgroundPaintInGraphicsLayer,
            GetBackgroundPaintLocation("scroller7"));

  // #scroller8 can paint background into scrolling contents layer because its
  // border-box is equivalent to its padding box since it has no border.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller8"));

  // #scroller9 can paint background into scrolling contents layer because its
  // border is opaque so it completely covers the background outside of the
  // padding-box.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller9"));

  // #scroller10 paints the background into both layers because its border is
  // partially transparent so the background must be drawn to the
  // border-box edges.
  EXPECT_EQ(
      kBackgroundPaintInGraphicsLayer | kBackgroundPaintInScrollingContents,
      GetBackgroundPaintLocation("scroller10"));

  // #scroller11 can paint background into scrolling contents layer because its
  // content-box is equivalent to its padding box since it has no padding.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller11"));

  // #scroller12 cannot paint background into scrolling contents layer because
  // it has padding so its content-box is not equivalent to its padding-box.
  EXPECT_EQ(kBackgroundPaintInGraphicsLayer,
            GetBackgroundPaintLocation("scroller12"));

  // #scroller13 paints the background into both layers because it has a custom
  // scrollbar which the background may need to draw under.
  EXPECT_EQ(
      kBackgroundPaintInGraphicsLayer | kBackgroundPaintInScrollingContents,
      GetBackgroundPaintLocation("scroller13"));

  // #scroller14 can paint background into scrolling contents layer because the
  // outline is drawn outside the padding box.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller14"));

  // #scroller15 can paint background into scrolling contents layer because
  // the outline is drawn into the decoration layer which will not be covered
  // up.
  EXPECT_EQ(kBackgroundPaintInScrollingContents,
            GetBackgroundPaintLocation("scroller15"));

  // #scroller16 cannot paint background into scrolling contents layer because
  // the scroller has a clip which would not be respected by the scrolling
  // contents layer.
  EXPECT_EQ(kBackgroundPaintInGraphicsLayer,
            GetBackgroundPaintLocation("scroller16"));

  // #scroller17 can only be painted once as it is translucent, and it must
  // be painted in the graphics layer to be under the translucent border.
  EXPECT_EQ(kBackgroundPaintInGraphicsLayer,
            GetBackgroundPaintLocation("scroller17"));
}

TEST_F(PaintLayerScrollableAreaTest, OpaqueContainedLayersPromoted) {
  RuntimeEnabledFeatures::SetCompositeOpaqueScrollersEnabled(true);

  SetBodyInnerHTML(
      "<style>"
      "#scroller { overflow: scroll; height: 200px; width: 200px; "
      "contain: paint; background: white local content-box; "
      "border: 10px solid rgba(0, 255, 0, 0.5); }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::CompositeOpaqueScrollersEnabled());
  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());
}

// Tests that we don't promote scrolling content which would not be contained.
// Promoting the scroller would also require promoting the positioned div
// which would lose subpixel anti-aliasing due to its transparent background.
TEST_F(PaintLayerScrollableAreaTest, NonContainedLayersNotPromoted) {
  RuntimeEnabledFeatures::SetCompositeOpaqueScrollersEnabled(true);

  SetBodyInnerHTML(
      "<style>"
      "#scroller { overflow: scroll; height: 200px; width: 200px; "
      "background: white local content-box; "
      "border: 10px solid rgba(0, 255, 0, 0.5); }"
      "#scrolled { height: 300px; }"
      "#positioned { position: relative; }"
      "</style>"
      "<div id=\"scroller\">"
      "  <div id=\"positioned\">Not contained by scroller.</div>"
      "  <div id=\"scrolled\"></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::CompositeOpaqueScrollersEnabled());
  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking());
}

TEST_F(PaintLayerScrollableAreaTest, TransparentLayersNotPromoted) {
  RuntimeEnabledFeatures::SetCompositeOpaqueScrollersEnabled(true);

  SetBodyInnerHTML(
      "<style>"
      "#scroller { overflow: scroll; height: 200px; width: 200px; background: "
      "rgba(0, 255, 0, 0.5) local content-box; border: 10px solid rgba(0, 255, "
      "0, 0.5); contain: paint; }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::CompositeOpaqueScrollersEnabled());
  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking());
}

TEST_F(PaintLayerScrollableAreaTest, OpaqueLayersDepromotedOnStyleChange) {
  RuntimeEnabledFeatures::SetCompositeOpaqueScrollersEnabled(true);

  SetBodyInnerHTML(
      "<style>"
      "#scroller { overflow: scroll; height: 200px; width: 200px; background: "
      "white local content-box; contain: paint; }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::CompositeOpaqueScrollersEnabled());
  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());

  // Change the background to transparent
  scroller->setAttribute(
      HTMLNames::styleAttr,
      "background: rgba(255,255,255,0.5) local content-box;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking());
}

TEST_F(PaintLayerScrollableAreaTest, OpaqueLayersPromotedOnStyleChange) {
  RuntimeEnabledFeatures::SetCompositeOpaqueScrollersEnabled(true);

  SetBodyInnerHTML(
      "<style>"
      "#scroller { overflow: scroll; height: 200px; width: 200px; background: "
      "rgba(255,255,255,0.5) local content-box; contain: paint; }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::CompositeOpaqueScrollersEnabled());
  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());

  // Change the background to transparent
  scroller->setAttribute(HTMLNames::styleAttr,
                         "background: white local content-box;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());
}

// Tests that a transform on the scroller or an ancestor will prevent promotion
// TODO(flackr): Allow integer transforms as long as all of the ancestor
// transforms are also integer.
TEST_F(PaintLayerScrollableAreaTest, OnlyNonTransformedOpaqueLayersPromoted) {
  ScopedCompositeOpaqueScrollersForTest composite_opaque_scrollers(true);

  SetBodyInnerHTML(
      "<style>"
      "#scroller { overflow: scroll; height: 200px; width: 200px; background: "
      "white local content-box; contain: paint; }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"parent\">"
      "  <div id=\"scroller\"><div id=\"scrolled\"></div></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::CompositeOpaqueScrollersEnabled());
  Element* parent = GetDocument().getElementById("parent");
  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());

  // Change the parent to have a transform.
  parent->setAttribute(HTMLNames::styleAttr, "transform: translate(1px, 0);");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking());

  // Change the parent to have no transform again.
  parent->removeAttribute(HTMLNames::styleAttr);
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());

  // Apply a transform to the scroller directly.
  scroller->setAttribute(HTMLNames::styleAttr, "transform: translate(1px, 0);");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking());
}

// Test that opacity applied to the scroller or an ancestor will cause the
// scrolling contents layer to not be promoted.
TEST_F(PaintLayerScrollableAreaTest, OnlyOpaqueLayersPromoted) {
  ScopedCompositeOpaqueScrollersForTest composite_opaque_scrollers(true);

  SetBodyInnerHTML(
      "<style>"
      "#scroller { overflow: scroll; height: 200px; width: 200px; background: "
      "white local content-box; contain: paint; }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"parent\">"
      "  <div id=\"scroller\"><div id=\"scrolled\"></div></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::CompositeOpaqueScrollersEnabled());
  Element* parent = GetDocument().getElementById("parent");
  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());

  // Change the parent to be partially translucent.
  parent->setAttribute(HTMLNames::styleAttr, "opacity: 0.5;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking());

  // Change the parent to be opaque again.
  parent->setAttribute(HTMLNames::styleAttr, "opacity: 1;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking());
  ASSERT_TRUE(paint_layer->GraphicsLayerBacking());
  EXPECT_TRUE(paint_layer->GraphicsLayerBacking()->ContentsOpaque());

  // Make the scroller translucent.
  scroller->setAttribute(HTMLNames::styleAttr, "opacity: 0.5");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());
  EXPECT_FALSE(paint_layer->GraphicsLayerBacking());
}

// Ensure OverlayScrollbarColorTheme get updated when page load
TEST_F(PaintLayerScrollableAreaTest, OverlayScrollbarColorThemeUpdated) {
  SetBodyInnerHTML(
      "<style>"
      "div { overflow: scroll; }"
      "#white { background-color: white; }"
      "#black { background-color: black; }"
      "</style>"
      "<div id=\"none\">a</div>"
      "<div id=\"white\">b</div>"
      "<div id=\"black\">c</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* none = GetDocument().getElementById("none");
  Element* white = GetDocument().getElementById("white");
  Element* black = GetDocument().getElementById("black");

  PaintLayer* none_layer =
      ToLayoutBoxModelObject(none->GetLayoutObject())->Layer();
  PaintLayer* white_layer =
      ToLayoutBoxModelObject(white->GetLayoutObject())->Layer();
  PaintLayer* black_layer =
      ToLayoutBoxModelObject(black->GetLayoutObject())->Layer();

  ASSERT_TRUE(none_layer);
  ASSERT_TRUE(white_layer);
  ASSERT_TRUE(black_layer);

  ASSERT_EQ(ScrollbarOverlayColorTheme::kScrollbarOverlayColorThemeDark,
            none_layer->GetScrollableArea()->GetScrollbarOverlayColorTheme());
  ASSERT_EQ(ScrollbarOverlayColorTheme::kScrollbarOverlayColorThemeDark,
            white_layer->GetScrollableArea()->GetScrollbarOverlayColorTheme());
  ASSERT_EQ(ScrollbarOverlayColorTheme::kScrollbarOverlayColorThemeLight,
            black_layer->GetScrollableArea()->GetScrollbarOverlayColorTheme());
}

// Test that css clip applied to the scroller will cause the
// scrolling contents layer to not be promoted.
TEST_F(PaintLayerScrollableAreaTest,
       OnlyAutoClippedScrollingContentsLayerPromoted) {
  SetBodyInnerHTML(
      "<style>"
      ".clip { clip: rect(0px,60px,50px,0px); }"
      "#scroller { position: absolute; overflow: auto;"
      "height: 100px; width: 100px; background: grey;"
      "will-change:transform; }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());

  // Add clip to scroller.
  scroller->setAttribute(HTMLNames::classAttr, "clip");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_FALSE(paint_layer->NeedsCompositedScrolling());

  // Change the scroller to be auto clipped again.
  scroller->removeAttribute("class");
  GetDocument().View()->UpdateAllLifecyclePhases();
  paint_layer = ToLayoutBoxModelObject(scroller->GetLayoutObject())->Layer();
  ASSERT_TRUE(paint_layer);
  EXPECT_TRUE(paint_layer->NeedsCompositedScrolling());
}

TEST_F(PaintLayerScrollableAreaTest, HideTooltipWhenScrollPositionChanges) {
  SetBodyInnerHTML(
      "<style>"
      "#scroller { width: 100px; height: 100px; overflow: scroll; }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* scroller = GetDocument().getElementById("scroller");
  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);

  EXPECT_CALL(GetChromeClient(),
              MockSetToolTip(GetDocument().GetFrame(), String(), _))
      .Times(1);
  scrollable_area->SetScrollOffset(ScrollOffset(1, 1), kUserScroll);

  // Programmatic scrolling should not dismiss the tooltip, so setToolTip
  // should not be called for this invocation.
  EXPECT_CALL(GetChromeClient(),
              MockSetToolTip(GetDocument().GetFrame(), String(), _))
      .Times(0);
  scrollable_area->SetScrollOffset(ScrollOffset(2, 2), kProgrammaticScroll);
}

TEST_F(PaintLayerScrollableAreaTest, IncludeOverlayScrollbarsInVisibleWidth) {
  RuntimeEnabledFeatures::SetOverlayScrollbarsEnabled(false);
  SetBodyInnerHTML(
      "<style>"
      "#scroller { overflow: overlay; height: 100px; width: 100px; }"
      "#scrolled { width: 100px; height: 200px; }"
      "</style>"
      "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* scroller = GetDocument().getElementById("scroller");
  ASSERT_TRUE(scroller);
  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(scroller->GetLayoutObject())->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  scrollable_area->SetScrollOffset(ScrollOffset(100, 0), kClampingScroll);
  EXPECT_EQ(scrollable_area->GetScrollOffset().Width(), 0);
}

TEST_F(PaintLayerScrollableAreaTest, ShowAutoScrollbarsForVisibleContent) {
  RuntimeEnabledFeatures::SetOverlayScrollbarsEnabled(false);
  SetBodyInnerHTML(
      "<style>"
      "#outerDiv {"
      "  width: 15px;"
      "  height: 100px;"
      "  overflow-y: auto;"
      "  overflow-x: hidden;"
      "}"
      "#innerDiv {"
      "  height:300px;"
      "  width: 1px;"
      "}"
      "</style>"
      "<div id='outerDiv'>"
      "  <div id='innerDiv'></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* outer_div = GetDocument().getElementById("outerDiv");
  ASSERT_TRUE(outer_div);
  outer_div->GetLayoutObject()->SetNeedsLayout("test");
  GetDocument().View()->UpdateAllLifecyclePhases();
  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(outer_div->GetLayoutObject())->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_TRUE(scrollable_area->HasVerticalScrollbar());
}

TEST_F(PaintLayerScrollableAreaTest, FloatOverflowInRtlContainer) {
  RuntimeEnabledFeatures::SetOverlayScrollbarsEnabled(false);
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "#container {"
      "  width: 200px;"
      "  overflow-x: auto;"
      "  overflow-y: scroll;"
      "  direction: rtl;"
      "}"
      "</style>"
      "<div id='container'>"
      "  <div style='float:left'>"
      "lorem ipsum"
      "  </div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* container = GetDocument().getElementById("container");
  ASSERT_TRUE(container);
  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(container->GetLayoutObject())->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_FALSE(scrollable_area->HasHorizontalScrollbar());
}
}
