// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"

#include "base/test/test_mock_time_task_runner.h"
#include "base/test/trace_event_analyzer.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/paint_timing_test_helper.h"
#include "third_party/blink/renderer/core/svg/svg_text_content_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class TextPaintTimingDetectorTest : public testing::Test {
 public:
  TextPaintTimingDetectorTest()
      : test_task_runner_(
            base::MakeRefCounted<base::TestMockTimeTaskRunner>()) {}

  void SetUp() override {
    web_view_helper_.Initialize();

    // Enable compositing on the page before running the document lifecycle.
    web_view_helper_.GetWebView()
        ->GetPage()
        ->GetSettings()
        .SetAcceleratedCompositingEnabled(true);

    WebLocalFrameImpl& frame_impl = *web_view_helper_.LocalMainFrame();
    frame_impl.ViewImpl()->MainFrameViewWidget()->Resize(gfx::Size(640, 480));

    frame_test_helpers::LoadFrame(
        web_view_helper_.GetWebView()->MainFrameImpl(), "about:blank");
    GetDocument().View()->SetParentVisible(true);
    GetDocument().View()->SetSelfVisible(true);
    // Advance clock so it isn't 0 as rendering code asserts in that case.
    AdvanceClock(base::TimeDelta::FromMicroseconds(1));
  }

 protected:
  LocalFrameView& GetFrameView() { return *GetFrame()->View(); }
  PaintTimingDetector& GetPaintTimingDetector() {
    return GetFrameView().GetPaintTimingDetector();
  }
  Document& GetDocument() { return *GetFrame()->GetDocument(); }

  IntRect GetViewportRect(LocalFrameView& view) {
    ScrollableArea* scrollable_area = view.GetScrollableArea();
    DCHECK(scrollable_area);
    return scrollable_area->VisibleContentRect();
  }

  LocalFrameView& GetChildFrameView() {
    return *To<LocalFrame>(GetFrame()->Tree().FirstChild())->View();
  }
  Document* GetChildDocument() {
    return To<LocalFrame>(GetFrame()->Tree().FirstChild())->GetDocument();
  }

  TextPaintTimingDetector* GetTextPaintTimingDetector() {
    return GetPaintTimingDetector().GetTextPaintTimingDetector();
  }

  TextPaintTimingDetector* GetChildFrameTextPaintTimingDetector() {
    return GetChildFrameView()
        .GetPaintTimingDetector()
        .GetTextPaintTimingDetector();
  }

  LargestTextPaintManager* GetLargestTextPaintManager() {
    return GetTextPaintTimingDetector()->records_manager_.ltp_manager_;
  }

  wtf_size_t CountVisibleTexts() {
    DCHECK(GetTextPaintTimingDetector());
    return GetTextPaintTimingDetector()
        ->records_manager_.visible_objects_.size();
  }

  wtf_size_t CountRankingSetSize() {
    DCHECK(GetTextPaintTimingDetector());
    return static_cast<wtf_size_t>(
        GetLargestTextPaintManager()->size_ordered_set_.size());
  }

  wtf_size_t CountInvisibleTexts() {
    return GetTextPaintTimingDetector()
        ->records_manager_.invisible_objects_.size();
  }

  wtf_size_t TextQueuedForPaintTimeSize() {
    return GetTextPaintTimingDetector()
        ->records_manager_.texts_queued_for_paint_time_.size();
  }

  wtf_size_t ContainerTotalSize() {
    return CountVisibleTexts() + CountRankingSetSize() + CountInvisibleTexts() +
           TextQueuedForPaintTimeSize();
  }

  void SimulateInputEvent() {
    GetPaintTimingDetector().NotifyInputEvent(WebInputEvent::Type::kMouseDown);
  }

  void SimulateScroll() {
    GetPaintTimingDetector().NotifyScroll(mojom::blink::ScrollType::kUser);
  }

  void SimulateKeyUp() {
    GetPaintTimingDetector().NotifyInputEvent(WebInputEvent::Type::kKeyUp);
  }

  void InvokeCallback() {
    DCHECK_GT(mock_callback_manager_->CountCallbacks(), 0u);
    InvokePresentationTimeCallback(mock_callback_manager_);
  }

  void ChildFramePresentationTimeCallBack() {
    DCHECK_GT(child_frame_mock_callback_manager_->CountCallbacks(), 0u);
    InvokePresentationTimeCallback(child_frame_mock_callback_manager_);
  }

  void InvokePresentationTimeCallback(
      MockPaintTimingCallbackManager* callback_manager) {
    callback_manager->InvokePresentationTimeCallback(
        test_task_runner_->NowTicks());
    // Outside the tests, this is invoked by
    // |PaintTimingCallbackManagerImpl::ReportPaintTime|.
    GetLargestTextPaintManager()->UpdateCandidate();
  }

  base::TimeTicks LargestPaintTime() {
    return GetPaintTimingDetector().largest_text_paint_time_;
  }

  uint64_t LargestPaintSize() {
    return GetPaintTimingDetector().largest_text_paint_size_;
  }

  base::TimeTicks ExperimentalLargestPaintTime() {
    return GetPaintTimingDetector().experimental_largest_text_paint_time_;
  }

  uint64_t ExperimentalLargestPaintSize() {
    return GetPaintTimingDetector().experimental_largest_text_paint_size_;
  }

  void SetBodyInnerHTML(const std::string& content) {
    frame_test_helpers::LoadHTMLString(
        web_view_helper_.GetWebView()->MainFrameImpl(), content,
        KURL("http://test.com"));
    mock_callback_manager_ =
        MakeGarbageCollected<MockPaintTimingCallbackManager>();
    GetTextPaintTimingDetector()->ResetCallbackManager(mock_callback_manager_);
    UpdateAllLifecyclePhases();
  }

  void SetChildBodyInnerHTML(const String& content) {
    GetChildDocument()->SetBaseURLOverride(KURL("http://test.com"));
    GetChildDocument()->body()->setInnerHTML(content, ASSERT_NO_EXCEPTION);
    child_frame_mock_callback_manager_ =
        MakeGarbageCollected<MockPaintTimingCallbackManager>();
    GetChildFrameTextPaintTimingDetector()->ResetCallbackManager(
        child_frame_mock_callback_manager_);
    UpdateAllLifecyclePhases();
  }

  void UpdateAllLifecyclePhases() {
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  }

  static constexpr base::TimeDelta kQuantumOfTime =
      base::TimeDelta::FromMilliseconds(10);

  // This only triggers ReportPresentationTime in main frame.
  void UpdateAllLifecyclePhasesAndSimulatePresentationTime() {
    UpdateAllLifecyclePhases();
    // Advance the clock for a bit so different presentation callbacks get
    // different times.
    AdvanceClock(kQuantumOfTime);
    while (mock_callback_manager_->CountCallbacks() > 0)
      InvokeCallback();
  }

  size_t CountPendingPresentationTime(LocalFrameView& frame_view) {
    TextPaintTimingDetector* detector =
        frame_view.GetPaintTimingDetector().GetTextPaintTimingDetector();
    return detector->records_manager_.texts_queued_for_paint_time_.size();
  }

  Element* AppendFontBlockToBody(String content) {
    Element* font = GetDocument().CreateRawElement(html_names::kFontTag);
    font->setAttribute(html_names::kSizeAttr, AtomicString("5"));
    Text* text = GetDocument().createTextNode(content);
    font->AppendChild(text);
    Element* div = GetDocument().CreateRawElement(html_names::kDivTag);
    div->AppendChild(font);
    GetDocument().body()->AppendChild(div);
    return font;
  }

  Element* AppendDivElementToBody(String content, String style = "") {
    Element* div = GetDocument().CreateRawElement(html_names::kDivTag);
    div->setAttribute(html_names::kStyleAttr, AtomicString(style));
    Text* text = GetDocument().createTextNode(content);
    div->AppendChild(text);
    GetDocument().body()->AppendChild(div);
    return div;
  }

  base::WeakPtr<TextRecord> TextRecordOfLargestTextPaint() {
    return GetLargestTextPaintManager()->FindLargestPaintCandidate();
  }

  base::WeakPtr<TextRecord> ChildFrameTextRecordOfLargestTextPaint() {
    return GetChildFrameView()
        .GetPaintTimingDetector()
        .GetTextPaintTimingDetector()
        ->records_manager_.ltp_manager_->FindLargestPaintCandidate();
  }

  void SetFontSize(Element* font_element, uint16_t font_size) {
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

  base::TimeTicks NowTicks() const { return test_task_runner_->NowTicks(); }

  void AdvanceClock(base::TimeDelta delta) {
    test_task_runner_->FastForwardBy(delta);
  }

  void LoadAhem() { web_view_helper_.LoadAhem(); }

 private:
  LocalFrame* GetFrame() {
    return web_view_helper_.GetWebView()->MainFrameImpl()->GetFrame();
  }

  frame_test_helpers::WebViewHelper web_view_helper_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  Persistent<MockPaintTimingCallbackManager> mock_callback_manager_;
  Persistent<MockPaintTimingCallbackManager> child_frame_mock_callback_manager_;
};

// Helper class to run the same test code with and without LayoutNG
class ParameterizedTextPaintTimingDetectorTest
    : public ::testing::WithParamInterface<bool>,
      private ScopedLayoutNGForTest,
      public TextPaintTimingDetectorTest {
 public:
  ParameterizedTextPaintTimingDetectorTest()
      : ScopedLayoutNGForTest(GetParam()) {}

 protected:
  bool LayoutNGEnabled() const {
    return RuntimeEnabledFeatures::LayoutNGEnabled();
  }
};

constexpr base::TimeDelta TextPaintTimingDetectorTest::kQuantumOfTime;

INSTANTIATE_TEST_SUITE_P(All,
                         ParameterizedTextPaintTimingDetectorTest,
                         testing::Bool());

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_NoText) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_OneText) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* only_text = AppendDivElementToBody("The only text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(only_text));
}

TEST_F(TextPaintTimingDetectorTest, InsertionOrderIsSecondaryRankingKey) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* first = AppendDivElementToBody("text");
  AppendDivElementToBody("text");
  AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(first));
  EXPECT_EQ(LargestPaintSize(), ExperimentalLargestPaintSize());
  EXPECT_EQ(LargestPaintTime(), ExperimentalLargestPaintTime());
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_TraceEvent_Candidate) {
  using trace_analyzer::Query;
  trace_analyzer::Start("*");
  {
    SetBodyInnerHTML(R"HTML(
      )HTML");
    AppendDivElementToBody("The only text");
    UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  }
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("LargestTextPaint::Candidate");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("loading", events[0]->category);

  EXPECT_TRUE(events[0]->HasArg("frame"));

  EXPECT_TRUE(events[0]->HasArg("data"));
  base::Value arg;
  EXPECT_TRUE(events[0]->GetArgAsValue("data", &arg));
  base::DictionaryValue* arg_dict;
  EXPECT_TRUE(arg.GetAsDictionary(&arg_dict));
  DOMNodeId node_id;
  EXPECT_TRUE(arg_dict->GetInteger("DOMNodeId", &node_id));
  EXPECT_GT(node_id, 0);
  int size;
  EXPECT_TRUE(arg_dict->GetInteger("size", &size));
  EXPECT_GT(size, 0);
  DOMNodeId candidate_index;
  EXPECT_TRUE(arg_dict->GetInteger("candidateIndex", &candidate_index));
  EXPECT_EQ(candidate_index, 1);
  bool is_main_frame;
  EXPECT_TRUE(arg_dict->GetBoolean("isMainFrame", &is_main_frame));
  EXPECT_EQ(true, is_main_frame);
  bool is_oopif;
  EXPECT_TRUE(arg_dict->GetBoolean("isOOPIF", &is_oopif));
  EXPECT_EQ(false, is_oopif);
  int x;
  EXPECT_TRUE(arg_dict->GetInteger("frame_x", &x));
  EXPECT_GT(x, 0);
  int y;
  EXPECT_TRUE(arg_dict->GetInteger("frame_y", &y));
  EXPECT_GT(y, 0);
  int width;
  EXPECT_TRUE(arg_dict->GetInteger("frame_width", &width));
  EXPECT_GT(width, 0);
  int height;
  EXPECT_TRUE(arg_dict->GetInteger("frame_height", &height));
  EXPECT_GT(height, 0);
  EXPECT_TRUE(arg_dict->GetInteger("root_x", &x));
  EXPECT_GT(x, 0);
  EXPECT_TRUE(arg_dict->GetInteger("root_y", &y));
  EXPECT_GT(y, 0);
  EXPECT_TRUE(arg_dict->GetInteger("root_width", &width));
  EXPECT_GT(width, 0);
  EXPECT_TRUE(arg_dict->GetInteger("root_height", &height));
  EXPECT_GT(height, 0);
}

TEST_F(TextPaintTimingDetectorTest,
       LargestTextPaint_TraceEvent_Candidate_Frame) {
  using trace_analyzer::Query;
  trace_analyzer::Start("*");
  {
    GetDocument().SetBaseURLOverride(KURL("http://test.com"));
    SetBodyInnerHTML(R"HTML(
      <style>body { margin: 15px; } iframe { display: block; position: relative; margin-top: 50px; } </style>
      <iframe> </iframe>
    )HTML");
    SetChildBodyInnerHTML(R"HTML(
    <style>body { margin: 10px;} #target { width: 200px; height: 200px; }
    </style>
    <div>Some content</div>
  )HTML");
    UpdateAllLifecyclePhasesAndSimulatePresentationTime();
    ChildFramePresentationTimeCallBack();
  }
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("LargestTextPaint::Candidate");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("loading", events[0]->category);

  EXPECT_TRUE(events[0]->HasArg("frame"));

  EXPECT_TRUE(events[0]->HasArg("data"));
  base::Value arg;
  EXPECT_TRUE(events[0]->GetArgAsValue("data", &arg));
  base::DictionaryValue* arg_dict;
  EXPECT_TRUE(arg.GetAsDictionary(&arg_dict));
  DOMNodeId node_id;
  EXPECT_TRUE(arg_dict->GetInteger("DOMNodeId", &node_id));
  EXPECT_GT(node_id, 0);
  int size;
  EXPECT_TRUE(arg_dict->GetInteger("size", &size));
  EXPECT_GT(size, 0);
  DOMNodeId candidate_index;
  EXPECT_TRUE(arg_dict->GetInteger("candidateIndex", &candidate_index));
  EXPECT_EQ(candidate_index, 1);
  bool is_main_frame;
  EXPECT_TRUE(arg_dict->GetBoolean("isMainFrame", &is_main_frame));
  EXPECT_EQ(false, is_main_frame);
  bool is_oopif;
  EXPECT_TRUE(arg_dict->GetBoolean("isOOPIF", &is_oopif));
  EXPECT_EQ(false, is_oopif);
  int x;
  EXPECT_TRUE(arg_dict->GetInteger("frame_x", &x));
  EXPECT_EQ(x, 10);
  int y;
  EXPECT_TRUE(arg_dict->GetInteger("frame_y", &y));
  // There's sometimes a 1 pixel offset for the y dimensions.
  EXPECT_GE(y, 9);
  EXPECT_LE(y, 10);
  int width;
  EXPECT_TRUE(arg_dict->GetInteger("frame_width", &width));
  EXPECT_GT(width, 0);
  int height;
  EXPECT_TRUE(arg_dict->GetInteger("frame_height", &height));
  EXPECT_GT(height, 0);
  EXPECT_TRUE(arg_dict->GetInteger("root_x", &x));
  EXPECT_GT(x, 25);
  EXPECT_TRUE(arg_dict->GetInteger("root_y", &y));
  EXPECT_GT(y, 50);
  EXPECT_TRUE(arg_dict->GetInteger("root_width", &width));
  EXPECT_GT(width, 0);
  EXPECT_TRUE(arg_dict->GetInteger("root_height", &height));
  EXPECT_GT(height, 0);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_TraceEvent_NoCandidate) {
  using trace_analyzer::Query;
  trace_analyzer::Start("*");
  {
    SetBodyInnerHTML(R"HTML(
      )HTML");
    Element* element = AppendDivElementToBody("text");
    UpdateAllLifecyclePhasesAndSimulatePresentationTime();
    RemoveElement(element);
    UpdateAllLifecyclePhases();
    EXPECT_GT(LargestPaintSize(), 0u);
    EXPECT_NE(LargestPaintTime(), base::TimeTicks());
    EXPECT_EQ(ExperimentalLargestPaintSize(), 0u);
    EXPECT_EQ(ExperimentalLargestPaintTime(), base::TimeTicks());
  }
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("LargestTextPaint::NoCandidate");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("loading", events[0]->category);

  EXPECT_TRUE(events[0]->HasArg("frame"));

  EXPECT_TRUE(events[0]->HasArg("data"));
  base::Value arg;
  EXPECT_TRUE(events[0]->GetArgAsValue("data", &arg));
  base::DictionaryValue* arg_dict;
  EXPECT_TRUE(arg.GetAsDictionary(&arg_dict));
  DOMNodeId candidate_index;
  EXPECT_TRUE(arg_dict->GetInteger("candidateIndex", &candidate_index));
  EXPECT_EQ(candidate_index, 2);
  bool is_main_frame;
  EXPECT_TRUE(arg_dict->GetBoolean("isMainFrame", &is_main_frame));
  EXPECT_EQ(true, is_main_frame);
  bool is_oopif;
  EXPECT_TRUE(arg_dict->GetBoolean("isOOPIF", &is_oopif));
  EXPECT_EQ(false, is_oopif);
}

TEST_F(TextPaintTimingDetectorTest, AggregationBySelfPaintingInlineElement) {
  SetBodyInnerHTML(R"HTML(
    <div style="background: yellow">
      tiny
      <span id="target"
        style="position: relative; background: blue; top: 100px; left: 100px">
        this is the largest text in the world.</span>
    </div>
  )HTML");
  Element* span = GetDocument().getElementById("target");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(span));
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_OpacityZero) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      opacity: 0;
    }
    </style>
  )HTML");
  AppendDivElementToBody("The only text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(CountVisibleTexts(), 0u);
}

TEST_F(TextPaintTimingDetectorTest,
       NodeRemovedBeforeAssigningPresentationTime) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <div id="remove">The only text</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhases();
  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("remove"));
  InvokeCallback();
  EXPECT_EQ(CountVisibleTexts(), 0u);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_LargestText) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("medium text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();

  Element* large_text = AppendDivElementToBody("a long-long-long text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();

  AppendDivElementToBody("small");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();

  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(large_text));
  EXPECT_EQ(LargestPaintSize(), ExperimentalLargestPaintSize());
  EXPECT_EQ(LargestPaintTime(), ExperimentalLargestPaintTime());
}

TEST_F(TextPaintTimingDetectorTest, UpdateResultWhenCandidateChanged) {
  base::TimeTicks time1 = NowTicks();
  SetBodyInnerHTML(R"HTML(
    <div>small text</div>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  base::TimeTicks time2 = NowTicks();
  base::TimeTicks first_largest = LargestPaintTime();
  EXPECT_GE(first_largest, time1);
  EXPECT_GE(time2, first_largest);
  EXPECT_EQ(first_largest, ExperimentalLargestPaintTime());

  AppendDivElementToBody("a long-long-long text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  base::TimeTicks time3 = NowTicks();
  base::TimeTicks second_largest = LargestPaintTime();
  EXPECT_GE(second_largest, time2);
  EXPECT_GE(time3, second_largest);
  EXPECT_EQ(second_largest, ExperimentalLargestPaintTime());
}

// There is a risk that a text that is just recorded is selected to be the
// metric candidate. The algorithm should skip the text record if its paint time
// hasn't been recorded yet.
TEST_F(TextPaintTimingDetectorTest, PendingTextIsLargest) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  GetFrameView().UpdateAllLifecyclePhasesForTest();
  // We do not call presentation-time callback here in order to not set the
  // paint time.
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
}

// The same node may be visited by recordText for twice before the paint time
// is set. In some previous design, this caused the node to be recorded twice.
TEST_F(TextPaintTimingDetectorTest, VisitSameNodeTwiceBeforePaintTimeIsSet) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* text = AppendDivElementToBody("text");
  GetFrameView().UpdateAllLifecyclePhasesForTest();
  // Change a property of the text to trigger repaint.
  text->setAttribute(html_names::kStyleAttr, AtomicString("color:red;"));
  GetFrameView().UpdateAllLifecyclePhasesForTest();
  InvokeCallback();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(text));
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_ReportFirstPaintTime) {
  base::TimeTicks start_time = NowTicks();
  AdvanceClock(base::TimeDelta::FromSecondsD(1));
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* text = AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  AdvanceClock(base::TimeDelta::FromSecondsD(1));
  text->setAttribute(html_names::kStyleAttr,
                     AtomicString("position:fixed;left:30px"));
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  AdvanceClock(base::TimeDelta::FromSecondsD(1));
  base::WeakPtr<TextRecord> record = TextRecordOfLargestTextPaint();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->paint_time,
            start_time + base::TimeDelta::FromSecondsD(1) + kQuantumOfTime);
  EXPECT_EQ(LargestPaintSize(), ExperimentalLargestPaintSize());
  EXPECT_EQ(LargestPaintTime(), ExperimentalLargestPaintTime());
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
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_RemovedText) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* large_text = AppendDivElementToBody(
      "(large text)(large text)(large text)(large text)(large text)(large "
      "text)");
  Element* small_text = AppendDivElementToBody("small text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(large_text));
  uint64_t size_before_remove = LargestPaintSize();
  base::TimeTicks time_before_remove = LargestPaintTime();
  EXPECT_EQ(ExperimentalLargestPaintSize(), size_before_remove);
  EXPECT_EQ(ExperimentalLargestPaintTime(), time_before_remove);
  EXPECT_GT(size_before_remove, 0u);
  EXPECT_GT(time_before_remove, base::TimeTicks());

  RemoveElement(large_text);
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(small_text));
  // LCP values should remain unchanged.
  EXPECT_EQ(LargestPaintSize(), size_before_remove);
  EXPECT_EQ(LargestPaintTime(), time_before_remove);
  // Experimental values should correspond to the smaller text.
  EXPECT_LT(ExperimentalLargestPaintSize(), size_before_remove);
}

TEST_F(TextPaintTimingDetectorTest,
       RemoveRecordFromAllContainerAfterTextRemoval) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* text = AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(ContainerTotalSize(), 2u);

  RemoveElement(text);
  EXPECT_EQ(ContainerTotalSize(), 0u);
}

TEST_F(TextPaintTimingDetectorTest,
       RemoveRecordFromAllContainerAfterRepeatedAttachAndDetach) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* text1 = AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(ContainerTotalSize(), 2u);

  Element* text2 = AppendDivElementToBody("text2");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(ContainerTotalSize(), 4u);

  RemoveElement(text1);
  EXPECT_EQ(ContainerTotalSize(), 2u);

  GetDocument().body()->AppendChild(text1);
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(ContainerTotalSize(), 4u);

  RemoveElement(text1);
  EXPECT_EQ(ContainerTotalSize(), 2u);

  RemoveElement(text2);
  EXPECT_EQ(ContainerTotalSize(), 0u);
}

TEST_F(TextPaintTimingDetectorTest,
       DestroyLargestTextPaintMangerAfterUserInput) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_TRUE(GetLargestTextPaintManager());

  SimulateInputEvent();
  EXPECT_FALSE(GetLargestTextPaintManager());
}

TEST_F(TextPaintTimingDetectorTest, KeepLargestTextPaintMangerAfterUserInput) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_TRUE(GetLargestTextPaintManager());

  SimulateKeyUp();
  EXPECT_TRUE(GetLargestTextPaintManager());
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_ReportLastNullCandidate) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* text = AppendDivElementToBody("text to remove");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(text));
  base::TimeTicks largest_paint_time = LargestPaintTime();
  EXPECT_NE(largest_paint_time, base::TimeTicks());
  EXPECT_EQ(largest_paint_time, ExperimentalLargestPaintTime());
  uint64_t largest_paint_size = LargestPaintSize();
  EXPECT_NE(largest_paint_size, 0u);
  EXPECT_EQ(largest_paint_size, ExperimentalLargestPaintSize());

  RemoveElement(text);
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
  // LCP values should remain unchanged.
  EXPECT_EQ(largest_paint_time, LargestPaintTime());
  EXPECT_EQ(largest_paint_size, LargestPaintSize());
  // Experimental values should be reset.
  EXPECT_EQ(base::TimeTicks(), ExperimentalLargestPaintTime());
  EXPECT_EQ(0u, ExperimentalLargestPaintSize());
}

TEST_F(TextPaintTimingDetectorTest,
       LargestTextPaint_CompareVisualSizeNotActualSize) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("a long text", "position:fixed;left:-10px");
  Element* short_text = AppendDivElementToBody("short");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(short_text));
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_CompareSizesAtFirstPaint) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* shortening_long_text = AppendDivElementToBody("123456789");
  AppendDivElementToBody("12345678");  // 1 letter shorter than the above.
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  // The visual size becomes smaller when less portion intersecting with
  // viewport.
  SetElementStyle(shortening_long_text, "position:fixed;left:-10px");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(shortening_long_text));
}

TEST_F(TextPaintTimingDetectorTest, TreatEllipsisAsText) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <div style="font:10px Ahem;white-space:nowrap;width:50px;overflow:hidden;text-overflow:ellipsis;">
    00000000000000000000000000000000000000000000000000000000000000000000000000
    00000000000000000000000000000000000000000000000000000000000000000000000000
    </div>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();

  EXPECT_EQ(CountVisibleTexts(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, CaptureFileUploadController) {
  SetBodyInnerHTML("<input type='file'>");
  Element* element = GetDocument().QuerySelector("input");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();

  EXPECT_EQ(CountVisibleTexts(), 1u);
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(element));
}

TEST_P(ParameterizedTextPaintTimingDetectorTest, CapturingListMarkers) {
  SetBodyInnerHTML(R"HTML(
    <ul>
      <li>List item</li>
    </ul>
    <ol>
      <li>Another list item</li>
    </ol>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();

  EXPECT_EQ(CountVisibleTexts(), LayoutNGEnabled() ? 3u : 2u);
}

TEST_F(TextPaintTimingDetectorTest, CaptureSVGText) {
  SetBodyInnerHTML(R"HTML(
    <svg height="40" width="300">
      <text x="0" y="15">A SVG text.</text>
    </svg>
  )HTML");

  auto* elem = To<SVGTextContentElement>(GetDocument().QuerySelector("text"));
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();

  EXPECT_EQ(CountVisibleTexts(), 1u);
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(elem));
}

// This is for comparison with the ClippedByViewport test.
TEST_F(TextPaintTimingDetectorTest, NormalTextUnclipped) {
  SetBodyInnerHTML(R"HTML(
    <div id='d'>text</div>
  )HTML");
  EXPECT_EQ(CountPendingPresentationTime(GetFrameView()), 1u);
  EXPECT_EQ(CountVisibleTexts(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, ClippedByViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #d { margin-top: 1234567px }
    </style>
    <div id='d'>text</div>
  )HTML");
  // Make sure the margin-top is larger than the viewport height.
  DCHECK_LT(GetViewportRect(GetFrameView()).Height(), 1234567);
  EXPECT_EQ(CountPendingPresentationTime(GetFrameView()), 0u);
  EXPECT_EQ(CountVisibleTexts(), 0u);
}

TEST_F(TextPaintTimingDetectorTest, ClippedByParentVisibleRect) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #outer1 {
        overflow: hidden;
        height: 1px;
        width: 1px;
      }
      #outer2 {
        overflow: hidden;
        height: 2px;
        width: 2px;
      }
    </style>
    <div id='outer1'></div>
    <div id='outer2'></div>
  )HTML");
  Element* div1 = GetDocument().CreateRawElement(html_names::kDivTag);
  Text* text1 = GetDocument().createTextNode(
      "########################################################################"
      "######################################################################"
      "#");
  div1->AppendChild(text1);
  GetDocument().body()->getElementById("outer1")->AppendChild(div1);

  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(div1));
  EXPECT_EQ(TextRecordOfLargestTextPaint()->first_size, 1u);

  Element* div2 = GetDocument().CreateRawElement(html_names::kDivTag);
  Text* text2 = GetDocument().createTextNode(
      "########################################################################"
      "######################################################################"
      "#");
  div2->AppendChild(text2);
  GetDocument().body()->getElementById("outer2")->AppendChild(div2);

  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_id,
            DOMNodeIds::ExistingIdForNode(div2));
  // This size is larger than the size of the first object . But the exact size
  // depends on different platforms. We only need to ensure this size is larger
  // than the first size.
  EXPECT_GT(TextRecordOfLargestTextPaint()->first_size, 1u);
}

TEST_F(TextPaintTimingDetectorTest, Iframe) {
  SetBodyInnerHTML(R"HTML(
    <iframe width=100px height=100px></iframe>
  )HTML");
  SetChildBodyInnerHTML("A");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(CountPendingPresentationTime(GetChildFrameView()), 1u);
  ChildFramePresentationTimeCallBack();
  base::WeakPtr<TextRecord> text = ChildFrameTextRecordOfLargestTextPaint();
  EXPECT_TRUE(text);
}

TEST_F(TextPaintTimingDetectorTest, Iframe_ClippedByViewport) {
  SetBodyInnerHTML(R"HTML(
    <iframe width=100px height=100px></iframe>
  )HTML");
  SetChildBodyInnerHTML(R"HTML(
    <style>
      #d { margin-top: 200px }
    </style>
    <div id='d'>text</div>
  )HTML");
  DCHECK_EQ(GetViewportRect(GetChildFrameView()).Height(), 100);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(CountPendingPresentationTime(GetChildFrameView()), 0u);
}

TEST_F(TextPaintTimingDetectorTest, SameSizeShouldNotBeIgnored) {
  SetBodyInnerHTML(R"HTML(
    <div>text</div>
    <div>text</div>
    <div>text</div>
    <div>text</div>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(CountRankingSetSize(), 4u);
}

TEST_F(TextPaintTimingDetectorTest, VisibleTextAfterUserInput) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(CountVisibleTexts(), 1u);
  EXPECT_TRUE(GetLargestTextPaintManager());

  SimulateInputEvent();
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(CountVisibleTexts(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, VisibleTextAfterUserScroll) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(CountVisibleTexts(), 1u);
  EXPECT_TRUE(GetLargestTextPaintManager());

  SimulateScroll();
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(CountVisibleTexts(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, OpacityZeroHTML) {
  SetBodyInnerHTML(R"HTML(
    <style>
      :root {
        opacity: 0;
        will-change: opacity;
      }
    </style>
    <div>Text</div>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(CountVisibleTexts(), 0u);

  // Change the opacity of documentElement, now the img should be a candidate.
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                "opacity: 1");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(CountVisibleTexts(), 1u);
  EXPECT_TRUE(TextRecordOfLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest, OpacityZeroHTML2) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        opacity: 0;
        will-change: opacity;
      }
    </style>
    <div id="target">Text</div>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(CountVisibleTexts(), 0u);

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                "opacity: 0");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(CountVisibleTexts(), 0u);

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                "opacity: 1");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(CountVisibleTexts(), 0u);
}

}  // namespace blink
