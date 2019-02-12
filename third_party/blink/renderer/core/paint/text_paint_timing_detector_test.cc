// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class TextPaintTimingDetectorTest
    : public RenderingTest,
      private ScopedFirstContentfulPaintPlusPlusForTest {
 public:
  TextPaintTimingDetectorTest()
      : ScopedFirstContentfulPaintPlusPlusForTest(true) {}
  void SetUp() override { RenderingTest::SetUp(); }

 protected:
  LocalFrameView& GetFrameView() { return *GetFrame().View(); }
  PaintTimingDetector& GetPaintTimingDetector() {
    return GetFrameView().GetPaintTimingDetector();
  }

  unsigned CountRecords() {
    return GetPaintTimingDetector()
        .GetTextPaintTimingDetector()
        .recorded_text_node_ids_.size();
  }

  void InvokeCallback() {
    TextPaintTimingDetector& detector =
        GetPaintTimingDetector().GetTextPaintTimingDetector();
    detector.ReportSwapTime(WebLayerTreeView::SwapResult::kDidSwap,
                            CurrentTimeTicks());
  }

  TimeTicks LargestPaintStoredResult() {
    return GetPaintTimingDetector()
        .GetTextPaintTimingDetector()
        .largest_text_paint_;
  }

  TimeTicks LastPaintStoredResult() {
    return GetPaintTimingDetector()
        .GetTextPaintTimingDetector()
        .last_text_paint_;
  }

  void UpdateAllLifecyclePhasesAndSimulateSwapTime() {
    GetFrameView().UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kTest);
    TextPaintTimingDetector& detector =
        GetPaintTimingDetector().GetTextPaintTimingDetector();
    if (detector.texts_to_record_swap_time_.size() > 0) {
      detector.ReportSwapTime(WebLayerTreeView::SwapResult::kDidSwap,
                              CurrentTimeTicks());
    }
  }

  void SimulateAnalyze() {
    GetPaintTimingDetector().GetTextPaintTimingDetector().Analyze();
  }

  Element* AppendFontElementToBody(String content) {
    Element* element = GetDocument().CreateRawElement(html_names::kFontTag);
    element->setAttribute(html_names::kSizeAttr, AtomicString("5"));
    Text* text = GetDocument().createTextNode(content);
    element->AppendChild(text);
    GetDocument().body()->AppendChild(element);
    return element;
  }

  Element* AppendDivElementToBody(String content, String style = "") {
    Element* div = GetDocument().CreateRawElement(html_names::kDivTag);
    div->setAttribute(html_names::kStyleAttr, AtomicString(style));
    Text* text = GetDocument().createTextNode(content);
    div->AppendChild(text);
    GetDocument().body()->AppendChild(div);
    return div;
  }

  DOMNodeId NodeIdOfText(Element* element) {
    DCHECK_EQ(element->CountChildren(), 1u);
    DCHECK(element->firstChild()->IsTextNode());
    DCHECK(!element->firstChild()->hasChildren());
    return DOMNodeIds::IdForNode(element->firstChild());
  }

  TextRecord* TextRecordOfLargestTextPaint() {
    return GetPaintTimingDetector()
        .GetTextPaintTimingDetector()
        .FindLargestPaintCandidate();
  }

  TextRecord* TextRecordOfLastTextPaint() {
    return GetPaintTimingDetector()
        .GetTextPaintTimingDetector()
        .FindLastPaintCandidate();
  }

  void SetFontSize(Element* font_element, uint8_t font_size) {
    DCHECK_EQ(font_element->nodeName(), "FONT");
    font_element->setAttribute(html_names::kSizeAttr,
                               AtomicString(WTF::String::Number(font_size)));
  }

  void SetElementStyle(Element* element, String style) {
    element->setAttribute(html_names::kStyleAttr, AtomicString(style));
  }

  void RemoveElement(Element* element) {
    element->GetLayoutObject()->Parent()->GetNode()->removeChild(element);
  }
};

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_NoText) {
  SetBodyInnerHTML(R"HTML(
    <body></body>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_OneText) {
  SetBodyInnerHTML(R"HTML(
    <body></body>
  )HTML");
  Element* only_text = AppendDivElementToBody("The only text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id, NodeIdOfText(only_text));
}

TEST_F(TextPaintTimingDetectorTest, NodeRemovedBeforeAssigningSwapTime) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <div id="remove">The only text</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("remove"));
  InvokeCallback();
  EXPECT_EQ(CountRecords(), 0u);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_LargestText) {
  SetBodyInnerHTML(R"HTML(
    <body></body>
  )HTML");
  AppendDivElementToBody("medium text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  Element* large_text = AppendDivElementToBody("a long-long-long text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  AppendDivElementToBody("small");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id, NodeIdOfText(large_text));
}

TEST_F(TextPaintTimingDetectorTest, UpdateResultWhenCandidateChanged) {
  TimeTicks time1 = CurrentTimeTicks();
  SetBodyInnerHTML(R"HTML(
    <div>small text</div>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  SimulateAnalyze();
  TimeTicks time2 = CurrentTimeTicks();
  TimeTicks first_largest = LargestPaintStoredResult();
  TimeTicks first_last = LastPaintStoredResult();
  EXPECT_GE(first_largest, time1);
  EXPECT_GE(time2, first_largest);
  EXPECT_GE(first_last, time1);
  EXPECT_GE(time2, first_last);

  Text* larger_text = GetDocument().createTextNode("a long-long-long text");
  GetDocument().body()->AppendChild(larger_text);
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  SimulateAnalyze();
  TimeTicks time3 = CurrentTimeTicks();
  TimeTicks second_largest = LargestPaintStoredResult();
  TimeTicks second_last = LastPaintStoredResult();
  EXPECT_GE(second_largest, time2);
  EXPECT_GE(time3, second_largest);
  EXPECT_GE(second_last, time2);
  EXPECT_GE(time3, second_last);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_ReportFirstPaintTime) {
  TimeTicks time1 = CurrentTimeTicks();
  SetBodyInnerHTML(R"HTML(
    <body></body>
  )HTML");
  Element* size_changing_text = AppendFontElementToBody("size-changing text");
  Element* long_text =
      AppendFontElementToBody("a long-long-long-long moving text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  TimeTicks time2 = CurrentTimeTicks();
  SetFontSize(size_changing_text, 50);
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  SetFontSize(long_text, 100);
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  TextRecord* record = GetPaintTimingDetector()
                           .GetTextPaintTimingDetector()
                           .FindLargestPaintCandidate();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id, NodeIdOfText(long_text));
  TimeTicks firing_time = record->first_paint_time;
  EXPECT_GE(firing_time, time1);
  EXPECT_GE(time2, firing_time);
}

TEST_F(TextPaintTimingDetectorTest,
       LargestTextPaint_IgnoreTextOutsideViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div.out {
        position: fixed;
        top: -100px;
      }
    </style>
    <div class='out'>text outside of viewport</div>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_FALSE(GetPaintTimingDetector()
                   .GetTextPaintTimingDetector()
                   .FindLargestPaintCandidate());
  EXPECT_FALSE(GetPaintTimingDetector()
                   .GetTextPaintTimingDetector()
                   .FindLastPaintCandidate());
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_IgnoreRemovedText) {
  SetBodyInnerHTML(R"HTML(
    <body></body>
  )HTML");
  Element* large_text = AppendDivElementToBody(
      "(large text)(large text)(large text)(large text)(large text)(large "
      "text)");
  Element* small_text = AppendDivElementToBody("small text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id, NodeIdOfText(large_text));

  RemoveElement(large_text);
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id, NodeIdOfText(small_text));
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_ReportLastNullCandidate) {
  SetBodyInnerHTML(R"HTML(
    <body></body>
  )HTML");
  Element* text = AppendDivElementToBody("text to remove");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  SimulateAnalyze();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id, NodeIdOfText(text));
  EXPECT_NE(LargestPaintStoredResult(), base::TimeTicks());

  RemoveElement(text);
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  SimulateAnalyze();
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
  EXPECT_EQ(LargestPaintStoredResult(), base::TimeTicks());
}

TEST_F(TextPaintTimingDetectorTest,
       LargestTextPaint_CompareVisualSizeNotActualSize) {
  SetBodyInnerHTML(R"HTML(
    <body></body>
  )HTML");
  AppendDivElementToBody("a long text", "position:fixed;left:-10px");
  Element* short_text = AppendDivElementToBody("short");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id, NodeIdOfText(short_text));
}

// Depite that the l
TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_CompareSizesAtFirstPaint) {
  SetBodyInnerHTML(R"HTML(
    <body></body>
  )HTML");
  Element* shortening_long_text = AppendDivElementToBody("123456789");
  AppendDivElementToBody("12345678");  // 1 letter shorter than the above.
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  // The visual size becomes smaller when less portion intersecting with
  // viewport.
  SetElementStyle(shortening_long_text, "position:fixed;left:-10px");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            NodeIdOfText(shortening_long_text));
}

TEST_F(TextPaintTimingDetectorTest, LastTextPaint_NoText) {
  SetBodyInnerHTML(R"HTML(
    <div></div>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  TextRecord* record = GetPaintTimingDetector()
                           .GetTextPaintTimingDetector()
                           .FindLastPaintCandidate();
  EXPECT_FALSE(record);
}

TEST_F(TextPaintTimingDetectorTest, LastTextPaint_OneText) {
  SetBodyInnerHTML(R"HTML(
    <body></body>
  )HTML");
  Element* text = AppendDivElementToBody("The only text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id, NodeIdOfText(text));
}

TEST_F(TextPaintTimingDetectorTest, LastTextPaint_LastText) {
  SetBodyInnerHTML(R"HTML(
    <div>1st text</div>
  )HTML");
  AppendDivElementToBody("s");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  AppendDivElementToBody("loooooooong");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  Element* third_text = AppendDivElementToBody("medium");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  EXPECT_EQ(TextRecordOfLastTextPaint()->node_id, NodeIdOfText(third_text));
}

TEST_F(TextPaintTimingDetectorTest, LastTextPaint_ReportFirstPaintTime) {
  SetBodyInnerHTML(R"HTML(
    <body></body>
  )HTML");
  AppendDivElementToBody("a loooooooooooooooooooong text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  TimeTicks time1 = CurrentTimeTicks();
  Element* latest_text = AppendFontElementToBody("latest text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  TimeTicks time2 = CurrentTimeTicks();
  SetFontSize(latest_text, 50);
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  SetFontSize(latest_text, 100);
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  TextRecord* record = TextRecordOfLastTextPaint();
  EXPECT_EQ(record->node_id, NodeIdOfText(latest_text));
  TimeTicks firing_time = record->first_paint_time;
  EXPECT_GE(firing_time, time1);
  EXPECT_GE(time2, firing_time);
}

TEST_F(TextPaintTimingDetectorTest, LastTextPaint_IgnoreRemovedText) {
  SetBodyInnerHTML(R"HTML(
    <body></body>
  )HTML");
  Element* first_text = AppendDivElementToBody("1st text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  Element* second_text = AppendDivElementToBody("2nd text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  RemoveElement(second_text);
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLastTextPaint()->node_id, NodeIdOfText(first_text));
}

TEST_F(TextPaintTimingDetectorTest, LastTextPaint_StopRecordingOverNodeLimit) {
  SetBodyInnerHTML(R"HTML(
    <body></body>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  for (int i = 1; i <= 4999; i++)
    AppendDivElementToBody(WTF::String::Number(i), "position:fixed;left:0px");

  UpdateAllLifecyclePhasesAndSimulateSwapTime();

  Element* text_5000 = AppendDivElementToBody(WTF::String::Number(5000));
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLastTextPaint()->node_id, NodeIdOfText(text_5000));

  AppendDivElementToBody(WTF::String::Number(5001));
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  EXPECT_EQ(TextRecordOfLastTextPaint()->node_id, NodeIdOfText(text_5000));
}

TEST_F(TextPaintTimingDetectorTest, LastTextPaint_ReportLastNullCandidate) {
  SetBodyInnerHTML(R"HTML(
    <body></body>
  )HTML");
  Element* text = AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  SimulateAnalyze();
  EXPECT_EQ(TextRecordOfLastTextPaint()->node_id, NodeIdOfText(text));
  EXPECT_NE(LastPaintStoredResult(), base::TimeTicks());

  RemoveElement(text);
  UpdateAllLifecyclePhasesAndSimulateSwapTime();
  SimulateAnalyze();
  EXPECT_FALSE(TextRecordOfLastTextPaint());
  EXPECT_EQ(LastPaintStoredResult(), base::TimeTicks());
}

}  // namespace blink
