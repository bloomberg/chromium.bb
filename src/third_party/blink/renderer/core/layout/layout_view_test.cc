// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_view.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class LayoutViewTest : public testing::WithParamInterface<bool>,
                       private ScopedLayoutNGForTest,
                       public RenderingTest {
 public:
  LayoutViewTest()
      : ScopedLayoutNGForTest(LayoutNG()),
        RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 protected:
  bool LayoutNG() { return GetParam(); }
  bool LayoutNGOrAndroidOrWindows() {
    // TODO(crbug.com/966731): Why is the platform difference?
#if defined(OS_WIN) || defined(OS_ANDROID)
    return true;
#else
    return LayoutNG();
#endif
  }
};

INSTANTIATE_TEST_SUITE_P(All, LayoutViewTest, testing::Bool());

TEST_P(LayoutViewTest, UpdateCountersLayout) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div.incX { counter-increment: x }
      div.incY { counter-increment: y }
      div::before { content: counter(y) }
    </style>
    <div id=inc></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  Element* inc = GetDocument().getElementById("inc");

  inc->setAttribute("class", "incX");
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_FALSE(GetDocument().View()->NeedsLayout());

  UpdateAllLifecyclePhasesForTest();
  inc->setAttribute("class", "incY");
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(GetDocument().View()->NeedsLayout());
}

TEST_P(LayoutViewTest, DisplayNoneFrame) {
  SetBodyInnerHTML(R"HTML(
    <iframe id="iframe" style="display:none"></iframe>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  HTMLIFrameElement* iframe =
      ToHTMLIFrameElement(GetDocument().getElementById("iframe"));
  Document* frame_doc = iframe->contentDocument();
  ASSERT_TRUE(frame_doc);
  LayoutObject* view = frame_doc->GetLayoutView();
  ASSERT_TRUE(view);
  EXPECT_FALSE(view->CanHaveChildren());

  frame_doc->body()->SetInnerHTMLFromString(R"HTML(
    <div id="div"></div>
  )HTML");

  frame_doc->Lifecycle().AdvanceTo(DocumentLifecycle::kInStyleRecalc);
  frame_doc->GetStyleEngine().RecalcStyle({});

  Element* div = frame_doc->getElementById("div");
  EXPECT_FALSE(div->GetComputedStyle());
}

TEST_P(LayoutViewTest, HitTestHorizontal) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div id="div" style="position: relative; font: 10px/10px Ahem;
        top: 100px; left: 50px; width: 200px; height: 80px">
      <span id="span1">ABCDE</span><span id="span2"
          style="position: relative; top: 30px">XYZ</span>
    </div>
  )HTML");

  // (50, 100)         (250, 100)
  //   |------------------|
  //   |ABCDE             |
  //   |                  |
  //   |                  |
  //   |     XYZ          |
  //   |                  |
  //   |                  |
  //   |------------------|
  // (50, 180)         (250, 180)
  auto* div = GetDocument().getElementById("div");
  auto* text1 = GetDocument().getElementById("span1")->firstChild();
  auto* text2 = GetDocument().getElementById("span2")->firstChild();

  HitTestResult result;
  // In body, but not in any descendants.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(1, 1)), result);
  EXPECT_EQ(GetDocument().body(), result.InnerNode());
  EXPECT_EQ(LayoutPoint(1, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Top-left corner of div and span1.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(51, 101)), result);
  EXPECT_EQ(text1, result.InnerNode());
  EXPECT_EQ(LayoutPoint(1, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Top-right corner (outside) of div.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(251, 101)), result);
  EXPECT_EQ(GetDocument().documentElement(), result.InnerNode());
  EXPECT_EQ(LayoutPoint(251, 101), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 3), TextAffinity::kUpstream),
            result.GetPosition());

  // Top-right corner (inside) of div.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(249, 101)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(LayoutPoint(199, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 3), TextAffinity::kUpstream),
            result.GetPosition());

  // Top-right corner (inside) of span1.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(99, 101)), result);
  EXPECT_EQ(text1, result.InnerNode());
  EXPECT_EQ(LayoutPoint(49, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 5), TextAffinity::kUpstream),
            result.GetPosition());

  // Top-right corner (outside) of span1.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(101, 101)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(LayoutPoint(51, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Bottom-left corner (outside) of div.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(51, 181)), result);
  EXPECT_EQ(GetDocument().documentElement(), result.InnerNode());
  EXPECT_EQ(LayoutPoint(51, 181), result.LocalPoint());
  // TODO(crbug.com/966731): Verify if the difference reflects any issue.
  EXPECT_EQ(
      LayoutNGOrAndroidOrWindows()
          ? PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Bottom-left corner (inside) of div.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(51, 179)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(LayoutPoint(1, 79), result.LocalPoint());
  // TODO(crbug.com/966731): Verify if the difference reflects any issue.
  EXPECT_EQ(
      LayoutNGOrAndroidOrWindows()
          ? PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Bottom-left corner (outside) of span1.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(51, 111)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(LayoutPoint(1, 11), result.LocalPoint());
  // TODO(crbug.com/966731): Verify if the difference reflects any issue.
  EXPECT_EQ(
      LayoutNGOrAndroidOrWindows()
          ? PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Top-left corner of span2.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(101, 131)), result);
  EXPECT_EQ(text2, result.InnerNode());
  EXPECT_EQ(LayoutPoint(51, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 0), TextAffinity::kDownstream),
            result.GetPosition());
}

TEST_P(LayoutViewTest, HitTestVerticalLR) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div id="div" style="position: relative; font: 10px/10px Ahem;
        top: 100px; left: 50px; width: 200px; height: 80px;
        writing-mode: vertical-lr">
      <span id="span1">ABCDE</span><span id="span2"
          style="position: relative; left: 30px">XYZ</span>
    </div>
  )HTML");

  // (50, 100)         (250, 100)
  //   |------------------|
  //   |A                 |
  //   |B                 |
  //   |C                 |
  //   |D                 |
  //   |E                 |
  //   |   X              |
  //   |   Y              |
  //   |   Z              |
  //   |------------------|
  // (50, 180)         (250, 180)
  auto* div = GetDocument().getElementById("div");
  auto* text1 = GetDocument().getElementById("span1")->firstChild();
  auto* text2 = GetDocument().getElementById("span2")->firstChild();

  HitTestResult result;
  // In body, but not in any descendants.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(1, 1)), result);
  EXPECT_EQ(GetDocument().body(), result.InnerNode());
  EXPECT_EQ(LayoutPoint(1, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Top-left corner of div and span1.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(51, 101)), result);
  EXPECT_EQ(text1, result.InnerNode());
  EXPECT_EQ(LayoutPoint(1, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Top-right corner (outside) of div.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(251, 101)), result);
  EXPECT_EQ(GetDocument().documentElement(), result.InnerNode());
  EXPECT_EQ(LayoutPoint(251, 101), result.LocalPoint());
  // TODO(crbug.com/966731): Verify if the difference reflects any issue.
  EXPECT_EQ(
      LayoutNGOrAndroidOrWindows()
          ? PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Top-right corner (inside) of div.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(249, 101)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(LayoutPoint(199, 1), result.LocalPoint());
  // TODO(crbug.com/966731): Verify if the difference reflects any issue.
  EXPECT_EQ(
      LayoutNGOrAndroidOrWindows()
          ? PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Top-right corner (inside) of span1.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(59, 101)), result);
  EXPECT_EQ(text1, result.InnerNode());
  EXPECT_EQ(LayoutPoint(9, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Top-right corner (outside) of span1.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(61, 101)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(LayoutPoint(11, 1), result.LocalPoint());
  // TODO(crbug.com/966731): Verify if the difference reflects any issue.
  EXPECT_EQ(
      LayoutNGOrAndroidOrWindows()
          ? PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Bottom-left corner (outside) of span1.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(51, 181)), result);
  EXPECT_EQ(GetDocument().documentElement(), result.InnerNode());
  EXPECT_EQ(LayoutPoint(51, 181), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 3), TextAffinity::kUpstream),
            result.GetPosition());

  // Bottom-left corner (inside) of span1.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(51, 179)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(LayoutPoint(1, 79), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 3), TextAffinity::kUpstream),
            result.GetPosition());

  // Top-left corner of span2.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(81, 151)), result);
  EXPECT_EQ(text2, result.InnerNode());
  EXPECT_EQ(LayoutPoint(1, 51), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 0), TextAffinity::kDownstream),
            result.GetPosition());
}

TEST_P(LayoutViewTest, HitTestVerticalRL) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div id="div" style="position: relative; font: 10px/10px Ahem;
        top: 100px; left: 50px; width: 200px; height: 80px;
        writing-mode: vertical-rl">
      <span id="span1">ABCDE</span><span id="span2"
          style="position: relative; left: -30px">XYZ</span>
    </div>
  )HTML");

  // (50, 100)         (250, 100)
  //   |------------------|
  //   |                 A|
  //   |                 B|
  //   |                 C|
  //   |                 D|
  //   |                 E|
  //   |              X   |
  //   |              Y   |
  //   |              Z   |
  //   |------------------|
  // (50, 180)         (250, 180)
  auto* div = GetDocument().getElementById("div");
  auto* text1 = GetDocument().getElementById("span1")->firstChild();
  auto* text2 = GetDocument().getElementById("span2")->firstChild();

  HitTestResult result;
  // In body, but not in any descendants.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(1, 1)), result);
  EXPECT_EQ(GetDocument().body(), result.InnerNode());
  EXPECT_EQ(LayoutPoint(1, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Top-left corner of div.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(51, 101)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(LayoutPoint(199, 1), result.LocalPoint());
  // TODO(crbug.com/966731): Verify if the difference reflects any issue.
  EXPECT_EQ(
      LayoutNGOrAndroidOrWindows()
          ? PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Top-right corner (outside) of div.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(251, 101)), result);
  EXPECT_EQ(GetDocument().documentElement(), result.InnerNode());
  EXPECT_EQ(LayoutPoint(251, 101), result.LocalPoint());
  // TODO(crbug.com/966731): Verify if the difference reflects any issue.
  EXPECT_EQ(
      LayoutNGOrAndroidOrWindows()
          ? PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Top-right corner (inside) of div and span1.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(249, 101)), result);
  EXPECT_EQ(text1, result.InnerNode());
  EXPECT_EQ(LayoutPoint(1, 1), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Bottom-right corner (inside) of span1.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(249, 149)), result);
  EXPECT_EQ(text1, result.InnerNode());
  EXPECT_EQ(LayoutPoint(1, 49), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text1, 5), TextAffinity::kUpstream),
            result.GetPosition());

  // Bottom-right corner (outside) of span1 but inside of div.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(249, 151)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(LayoutPoint(1, 51), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Bottom-left corner (outside) of div.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(51, 181)), result);
  EXPECT_EQ(GetDocument().documentElement(), result.InnerNode());
  EXPECT_EQ(LayoutPoint(51, 181), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 3), TextAffinity::kUpstream),
            result.GetPosition());

  // Bottom-left corner (inside) of div.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(51, 179)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(LayoutPoint(199, 79), result.LocalPoint());
  // TODO(crbug.com/966731): Verify if the difference reflects any issue.
  EXPECT_EQ(
      LayoutNGOrAndroidOrWindows()
          ? PositionWithAffinity(Position(text2, 3), TextAffinity::kUpstream)
          : PositionWithAffinity(Position(text2, 3), TextAffinity::kDownstream),
      result.GetPosition());

  // Bottom-left corner (outside) of span1.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(241, 151)), result);
  EXPECT_EQ(div, result.InnerNode());
  EXPECT_EQ(LayoutPoint(9, 51), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 0), TextAffinity::kDownstream),
            result.GetPosition());

  // Top-right corner (inside) of span2.
  GetLayoutView().HitTest(HitTestLocation(LayoutPoint(219, 151)), result);
  EXPECT_EQ(text2, result.InnerNode());
  EXPECT_EQ(LayoutPoint(1, 51), result.LocalPoint());
  EXPECT_EQ(PositionWithAffinity(Position(text2, 0), TextAffinity::kDownstream),
            result.GetPosition());
}

}  // namespace blink
