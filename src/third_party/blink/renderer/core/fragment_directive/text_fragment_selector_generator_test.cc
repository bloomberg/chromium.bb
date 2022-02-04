// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/text_fragment_selector_generator.h"

#include <gtest/gtest.h>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "components/ukm/test_ukm_recorder.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/link_to_text/link_to_text.mojom-blink.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_handler.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/testing/scoped_fake_ukm_recorder.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

using LinkGenerationError = shared_highlighting::LinkGenerationError;

namespace blink {

namespace {
const char kSuccessUkmMetric[] = "Success";
const char kErrorUkmMetric[] = "Error";
}  // namespace

class TextFragmentSelectorGeneratorTest : public SimTest {
 public:
  void SetUp() override {
    SimTest::SetUp();
    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  }

  void VerifySelector(Position selected_start,
                      Position selected_end,
                      String expected_selector) {
    String generated_selector = GenerateSelector(selected_start, selected_end);
    EXPECT_EQ(expected_selector, generated_selector);

    // Should not have logged errors in a success case.
    histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated.Error",
                                       0);
    histogram_tester_.ExpectTotalCount(
        "SharedHighlights.LinkGenerated.Error.Requested", 0);

    histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated",
                                       generate_call_count_);
    auto entries = ukm_recorder()->GetEntriesByName(
        ukm::builders::SharedHighlights_LinkGenerated::kEntryName);
    ASSERT_EQ(1u, entries.size());
    const ukm::mojom::UkmEntry* entry = entries[0];
    EXPECT_EQ(GetDocument().UkmSourceID(), entry->source_id);
    ukm_recorder()->ExpectEntryMetric(entry, kSuccessUkmMetric, true);
    EXPECT_FALSE(ukm_recorder()->GetEntryMetric(entry, kErrorUkmMetric));
  }

  void VerifySelectorFails(Position selected_start,
                           Position selected_end,
                           LinkGenerationError error) {
    String generated_selector = GenerateSelector(selected_start, selected_end);
    EXPECT_EQ("", generated_selector);

    histogram_tester_.ExpectTotalCount("SharedHighlights.LinkGenerated",
                                       generate_call_count_);
    histogram_tester_.ExpectBucketCount("SharedHighlights.LinkGenerated.Error",
                                        error, 1);
    auto entries = ukm_recorder()->GetEntriesByName(
        ukm::builders::SharedHighlights_LinkGenerated::kEntryName);
    ASSERT_EQ(1u, entries.size());
    const ukm::mojom::UkmEntry* entry = entries[0];
    EXPECT_EQ(GetDocument().UkmSourceID(), entry->source_id);
    ukm_recorder()->ExpectEntryMetric(entry, kSuccessUkmMetric, false);
    ukm_recorder()->ExpectEntryMetric(entry, kErrorUkmMetric,
                                      static_cast<int64_t>(error));
  }

  String GenerateSelector(Position selected_start, Position selected_end) {
    generate_call_count_++;

    bool callback_called = false;
    String selector;
    auto lambda = [](bool& callback_called, String& selector,
                     const TextFragmentSelector& generated_selector,
                     shared_highlighting::LinkGenerationError error) {
      selector = generated_selector.ToString();
      callback_called = true;
    };
    auto callback =
        WTF::Bind(lambda, std::ref(callback_called), std::ref(selector));
    GetTextFragmentSelectorGenerator()->Generate(
        *MakeGarbageCollected<RangeInFlatTree>(
            ToPositionInFlatTree(selected_start),
            ToPositionInFlatTree(selected_end)),
        std::move(callback));
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(callback_called);
    return selector;
  }

  TextFragmentSelectorGenerator* GetTextFragmentSelectorGenerator() {
    return GetDocument()
        .GetFrame()
        ->GetTextFragmentHandler()
        ->GetTextFragmentSelectorGenerator();
  }

 protected:
  ukm::TestUkmRecorder* ukm_recorder() {
    return scoped_ukm_recorder_.recorder();
  }

  base::HistogramTester histogram_tester_;
  ScopedFakeUkmRecorder scoped_ukm_recorder_;
  int generate_call_count_ = 0;
};

// Basic exact selector case.
TEST_F(TextFragmentSelectorGeneratorTest, EmptySelection) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 5);
  const auto& selected_end = Position(first_paragraph, 6);
  ASSERT_EQ(" ", PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelectorFails(selected_start, selected_end,
                      LinkGenerationError::kEmptySelection);
}

// Basic exact selector case.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextSelector) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <p id='second'>Second paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 28);
  ASSERT_EQ("First paragraph text that is",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "First%20paragraph%20text%20that%20is");
}

// Exact selector test where selection contains nested <i> node.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextWithNestedTextNodes) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is <i>longer than 20</i> chars</p>
    <p id='second'>Second paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first");
  const auto& selected_start = Position(first_paragraph->firstChild(), 0);
  const auto& selected_end =
      Position(first_paragraph->firstChild()->nextSibling()->firstChild(), 6);
  ASSERT_EQ("First paragraph text that is longer",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "First%20paragraph%20text%20that%20is%20longer");
}

// Exact selector test where selection contains multiple spaces.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextWithExtraSpace) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <p id='second'>Second paragraph
      text</p>
  )HTML");
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& selected_start = Position(second_paragraph, 0);
  const auto& selected_end = Position(second_paragraph, 27);
  ASSERT_EQ("Second paragraph text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end, "Second%20paragraph%20text");
}

// Exact selector where selection is too short, in which case context is
// required.
TEST_F(TextFragmentSelectorGeneratorTest,
       ExactTextSelector_TooShortNeedsContext) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph prefix to unique snippet of text.</p>
    <p id='second'>Second paragraph</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 26);
  const auto& selected_end = Position(first_paragraph, 40);
  ASSERT_EQ("unique snippet",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "paragraph%20prefix%20to-,unique%20snippet,-of%20text.");
}

// Exact selector with context test. Case when only one word for prefix and
// suffix is enough to disambiguate the selection.
TEST_F(TextFragmentSelectorGeneratorTest,
       ExactTextSelector_WithOneWordContext) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <p id='second'>Second paragraph text that is short</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 6);
  const auto& selected_end = Position(first_paragraph, 28);
  ASSERT_EQ("paragraph text that is",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "First-,paragraph%20text%20that%20is,-longer%20than%2020");
}

// Exact selector with context test. Case when multiple words for prefix and
// suffix is necessary to disambiguate the selection.
TEST_F(TextFragmentSelectorGeneratorTest,
       ExactTextSelector_MultipleWordContext) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First prefix to not unique snippet of text followed by suffix</p>
    <p id='second'>Second prefix to not unique snippet of text followed by suffix</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 16);
  const auto& selected_end = Position(first_paragraph, 42);
  ASSERT_EQ("not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "First%20prefix%20to-,not%20unique%20snippet%20of%"
                 "20text,-followed%20by%20suffix");
}

// Exact selector with context test. Case when multiple words for prefix and
// suffix is necessary to disambiguate the selection and prefix and suffix
// contain extra space.
TEST_F(TextFragmentSelectorGeneratorTest,
       ExactTextSelector_MultipleWordContext_ExtraSpace) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First prefix      to not unique snippet of text followed       by suffix</p>
    <p id='second'>Second prefix to not unique snippet of text followed by suffix</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 21);
  const auto& selected_end = Position(first_paragraph, 47);
  ASSERT_EQ("not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "First%20prefix%20to-,not%20unique%20snippet%20of%"
                 "20text,-followed%20by%20suffix");
}

// Exact selector with context test. Case when available prefix for all the
// occurrences of selected text is the same. In this case suffix should be
// extended until unique selector is found.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextSelector_SamePrefix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>Prefix to not unique snippet of text followed by different suffix</p>
    <p id='second'>Prefix to not unique snippet of text followed by suffix</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 10);
  const auto& selected_end = Position(first_paragraph, 36);
  ASSERT_EQ("not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "Prefix%20to-,not%20unique%20snippet%20of%20text,-"
                 "followed%20by%20different");
}

// Exact selector with context test. Case when available suffix for all the
// occurrences of selected text is the same. In this case prefix should be
// extended until unique selector is found.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextSelector_SameSuffix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph prefix to not unique snippet of text followed by suffix</p>
    <p id='second'>Second paragraph prefix to not unique snippet of text followed by suffix</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 26);
  const auto& selected_end = Position(first_paragraph, 52);
  ASSERT_EQ("not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "First%20paragraph%20prefix%20to-,not%20unique%"
                 "20snippet%20of%20text,-followed%20by%20suffix");
}

// Exact selector with context test. Case when available prefix and suffix for
// all the occurrences of selected text are the same. In this case generation
// should be unsuccessful.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextSelector_SamePrefixSuffix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>Same paragraph prefix to not unique snippet of text followed by suffix</p>
    <p id='second'>Same paragraph prefix to not unique snippet of text followed by suffix</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 25);
  const auto& selected_end = Position(first_paragraph, 51);
  ASSERT_EQ("not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelectorFails(selected_start, selected_end,
                      LinkGenerationError::kContextExhausted);
}

// Exact selector with context test. Case when available prefix and suffix for
// all the occurrences of selected text are the same for the first 10 words. In
// this case generation should be unsuccessful.
TEST_F(TextFragmentSelectorGeneratorTest,
       ExactTextSelector_SimilarLongPreffixSuffix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph prefix one two three four five six seven
     eight nine ten to not unique snippet of text followed by suffix</p>
    <p id='second'>Second paragraph prefix one two three four five six seven
     eight nine ten to not unique snippet of text followed by suffix</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 80);
  const auto& selected_end = Position(first_paragraph, 106);
  ASSERT_EQ("not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelectorFails(selected_start, selected_end,
                      LinkGenerationError::kContextLimitReached);
}

// Exact selector with context test. Case when no prefix is available.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextSelector_NoPrefix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>Not unique snippet of text followed by first suffix</p>
    <p id='second'>Not unique snippet of text followed by second suffix</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 26);
  ASSERT_EQ("Not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "Not%20unique%20snippet%20of%20text,-followed%20by%20first");
}

// Exact selector with context test. Case when no suffix is available.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextSelector_NoSuffix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First prefix to not unique snippet of text</p>
    <p id='second'>Second prefix to not unique snippet of text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& selected_start = Position(first_paragraph, 17);
  const auto& selected_end = Position(first_paragraph, 43);
  ASSERT_EQ("not unique snippet of text",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "Second%20prefix%20to-,not%20unique%20snippet%20of%"
                 "20text");
}

// Exact selector with context test. Case when available prefix is the
// preceding block.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextSelector_PrevNodePrefix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph with not unique snippet</p>
    <p id='second'>not unique snippet of text</p>
  )HTML");
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& selected_start = Position(second_paragraph, 0);
  const auto& selected_end = Position(second_paragraph, 18);
  ASSERT_EQ("not unique snippet",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "not%20unique%20snippet-,not%20unique%20snippet,-of%20text");
}

// Exact selector with context test. Case when available prefix is the
// preceding block, which is a text node.
TEST_F(TextFragmentSelectorGeneratorTest,
       ExactTextSelector_PrevTextNodePrefix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph with not unique snippet</p>
    text
    <p id='second'>not unique snippet of text</p>
  )HTML");
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& selected_start = Position(second_paragraph, 0);
  const auto& selected_end = Position(second_paragraph, 18);
  ASSERT_EQ("not unique snippet",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "text-,not%20unique%20snippet,-of%20text");
}

// Exact selector with context test. Case when available suffix is the next
// block.
TEST_F(TextFragmentSelectorGeneratorTest, ExactTextSelector_NextNodeSuffix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph with not unique snippet</p>
    <p id='second'>not unique snippet of text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 21);
  const auto& selected_end = Position(first_paragraph, 39);
  ASSERT_EQ("not unique snippet",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "First%20paragraph%20with-,not%20unique%20snippet,-not%"
                 "20unique%20snippet");
}

// Exact selector with context test. Case when available suffix is the next
// block, which is a text node.
TEST_F(TextFragmentSelectorGeneratorTest,
       ExactTextSelector_NexttextNodeSuffix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph with not unique snippet</p>
    text
    <p id='second'>not unique snippet of text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 21);
  const auto& selected_end = Position(first_paragraph, 39);
  ASSERT_EQ("not unique snippet",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "First%20paragraph%20with-,not%20unique%20snippet,-text");
}

TEST_F(TextFragmentSelectorGeneratorTest, RangeSelector) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <p id='second'>Second paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(second_paragraph, 6);
  ASSERT_EQ("First paragraph text that is longer than 20 chars\n\nSecond",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "First%20paragraph%20text,Second");
}

// It should be more than 300 characters selected from the same node so that
// ranges are used.
TEST_F(TextFragmentSelectorGeneratorTest, RangeSelector_SameNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text text text text text text text
    text text text text text text text text text text text text text
    text text text text text text text text text text text text text
    text text text text text text text text text text text text text
    text text text text text text text text text and last text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 320);
  ASSERT_EQ(
      "First paragraph text text text text text text text \
text text text text text text text text text text text text text \
text text text text text text text text text text text text text \
text text text text text text text text text text text text text \
text text text text text text text text text and last text",
      PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "First%20paragraph%20text,and%20last%20text");
}

// It should be more than 300 characters selected from the same node so that
// ranges are used.
TEST_F(TextFragmentSelectorGeneratorTest,
       RangeSelector_SameNode_MultipleSelections) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text text text text text text text
    text text text text text text text text text text text text text
    text text text text text text text text text text text text text
    text text text text text text text text text text text text text
    text text text text text text text text text text and last text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 325);
  ASSERT_EQ(
      "First paragraph text text text text text text text \
text text text text text text text text text text text text text \
text text text text text text text text text text text text text \
text text text text text text text text text text text text text \
text text text text text text text text text text and last text",
      PlainText(EphemeralRange(selected_start, selected_end)));
  ASSERT_EQ(309u,
            PlainText(EphemeralRange(selected_start, selected_end)).length());

  VerifySelector(selected_start, selected_end,
                 "First%20paragraph%20text,and%20last%20text");

  scoped_ukm_recorder_.ResetRecorder();

  const auto& second_selected_start = Position(first_paragraph, 6);
  const auto& second_selected_end = Position(first_paragraph, 325);
  ASSERT_EQ(
      "paragraph text text text text text text text \
text text text text text text text text text text text text text \
text text text text text text text text text text text text text \
text text text text text text text text text text text text text \
text text text text text text text text text text and last text",
      PlainText(EphemeralRange(second_selected_start, second_selected_end)));
  ASSERT_EQ(303u, PlainText(EphemeralRange(second_selected_start,
                                           second_selected_end))
                      .length());

  VerifySelector(second_selected_start, second_selected_end,
                 "paragraph%20text%20text,and%20last%20text");
}

// When using all the selected text for the range is not enough for unique
// match, context should be added.
TEST_F(TextFragmentSelectorGeneratorTest, RangeSelector_RangeNotUnique) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph</p><p id='text1'>text</p>
    <p id='second'>Second paragraph</p><p id='text2'>text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  Node* first_text = GetDocument().getElementById("text1")->firstChild();
  const auto& selected_start = Position(first_paragraph, 6);
  const auto& selected_end = Position(first_text, 4);
  ASSERT_EQ("paragraph\n\ntext",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "First-,paragraph,text,-Second%20paragraph");
}

// When using all the selected text for the range is not enough for unique
// match, context should be added, but only prefxi and no suffix is available.
TEST_F(TextFragmentSelectorGeneratorTest,
       RangeSelector_RangeNotUnique_NoSuffix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph</p><p id='text1'>text</p>
    <p id='second'>Second paragraph</p><p id='text2'>text</p>
  )HTML");
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  Node* second_text = GetDocument().getElementById("text2")->firstChild();
  const auto& selected_start = Position(second_paragraph, 7);
  const auto& selected_end = Position(second_text, 4);
  ASSERT_EQ("paragraph\n\ntext",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end, "Second-,paragraph,text");
}

// When no range end is available it should return empty selector.
// There is no range end available because there is no word break in the second
// half of the selection.
TEST_F(TextFragmentSelectorGeneratorTest, RangeSelector_NoRangeEnd) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text text text text text text text
    text text text text text text text text text text text text text
    text text text text text text text text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_text_and_last_text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(first_paragraph, 312);
  ASSERT_EQ(
      "First paragraph text text text text text text text \
text text text text text text text text text text text text text \
text text text text text text text text_text_text_text_text_text_\
text_text_text_text_text_text_text_text_text_text_text_text_text_\
text_text_text_text_text_text_text_text_text_and_last_text",
      PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelectorFails(selected_start, selected_end,
                      LinkGenerationError::kNoRange);
}

// Selection should be autocompleted to contain full words.
TEST_F(TextFragmentSelectorGeneratorTest, WordLimit) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 7);
  const auto& selected_end = Position(first_paragraph, 33);
  ASSERT_EQ("aragraph text that is long",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "paragraph%20text%20that%20is%20longer");
}

// Selection should be autocompleted to contain full words. The autocompletion
// should work with extra spaces.
TEST_F(TextFragmentSelectorGeneratorTest, WordLimit_ExtraSpaces) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First
    paragraph text
    that is longer than 20 chars</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 11);
  const auto& selected_end = Position(first_paragraph, 41);
  ASSERT_EQ("aragraph text that is long",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "paragraph%20text%20that%20is%20longer");
}

// When selection starts at the end of a word, selection shouldn't be
// autocompleted to contain extra words.
TEST_F(TextFragmentSelectorGeneratorTest,
       WordLimit_SelectionStartsAndEndsAtWordLimit) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer  than 20 chars</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 5);
  const auto& selected_end = Position(first_paragraph, 37);
  ASSERT_EQ(" paragraph text that is longer ",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "paragraph%20text%20that%20is%20longer");
}

// Check the case when selections starts with an non text node.
TEST_F(TextFragmentSelectorGeneratorTest, StartsWithImage) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <img id="img">
    <p id='first'>First paragraph text that is longer  than 20 chars</p>
  )HTML");
  Node* img = GetDocument().getElementById("img");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(img, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("\nFirst", PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "Test%20page-,First,-paragraph%20text%20that");
}

// Check the case when selections starts with an non text node.
TEST_F(TextFragmentSelectorGeneratorTest, StartsWithBlockWithImage) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <div id="img_div">
      <img id="img">
    </div>
    <p id='first'>First paragraph text that is longer  than 20 chars</p>
  )HTML");
  Node* img = GetDocument().getElementById("img_div");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(img, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("\nFirst", PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "Test%20page-,First,-paragraph%20text%20that");
}

// Check the case when selections starts with a node nested in "inline-block"
// node. crbug.com/1151474
TEST_F(TextFragmentSelectorGeneratorTest, StartsWithInlineBlockChild) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      li {
        display: inline-block;
      }
    </style>
    <div>Test page</div>
    <ul>
      <li>
        <a id="link1"/>
      </li>
      <li>
        <a id="link2"/>
      </li>
      <li>
        <a id="link3"/>
      </li>
    </ul>
    <p id='first'>First paragraph text that is longer  than 20 chars</p>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  Node* img = GetDocument().getElementById("link1");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position::LastPositionInNode(*img);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("  \nFirst", PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "Test%20page-,First,-paragraph%20text%20that");
}

// Check the case when selections ends with an non text node.
TEST_F(TextFragmentSelectorGeneratorTest, EndswithImage) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <img id="img">
    </img>
  )HTML");
  Node* img = GetDocument().getElementById("img");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 44);
  const auto& end = Position(img, 0);
  ASSERT_EQ("chars\n\n", PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "longer%20than%2020-,chars");
}

// Check the case when selections starts at the end of the previous block.
TEST_F(TextFragmentSelectorGeneratorTest, StartIsEndofPrevBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph     </p>
    <p id='second'>Second paragraph</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& start = Position(first_paragraph, 18);
  const auto& end = Position(second_paragraph, 6);
  ASSERT_EQ("\nSecond", PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "First%20paragraph-,Second,-paragraph");
}

// Check the case when selections starts at the end of the previous block.
TEST_F(TextFragmentSelectorGeneratorTest, EndIsStartofNextBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph</p>
    <p id='second'>     Second paragraph</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(second_paragraph, 2);
  ASSERT_EQ("First paragraph\n\n", PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "First%20paragraph,-Second%20paragraph");
}

// Check the case when parent of selection start is a sibling of a node where
// selection ends.
//   :root
//  /      \
// div      p
//  |       |
//  p      "]Second"
//  |
// "[First..."
// Where [] indicate selection. In this case, when the selection is adjusted, we
// want to ensure it correctly traverses the tree back to the previous text node
// and not to the <div>(sibling of second <p>).
// See crbug.com/1154308 for more context.
TEST_F(TextFragmentSelectorGeneratorTest, PrevNodeIsSiblingsChild) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");

  // HTML is intentionally not formatted. Adding new lines and indentation
  // creates empty text nodes which changes the dom tree.
  request.Complete(R"HTML(
    <!DOCTYPE html>
  <div><p id='start'>First paragraph</p></div><p id='end'>Second paragraph</p>
  )HTML");
  GetDocument().UpdateStyleAndLayoutTree();
  Node* first_paragraph = GetDocument().getElementById("start")->firstChild();
  Node* second_paragraph = GetDocument().getElementById("end");
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(second_paragraph, 0);
  ASSERT_EQ("First paragraph\n\n", PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "First%20paragraph,-Second%20paragraph");
}

// Check the case when parent of selection start is a sibling of a node where
// selection ends.
//    :root
//  /    |     \
// div  div     p
//  |    |       \
//  p   "test"   "]Second"
//  |
//"[First..."
//
// Where [] indicate selection. In this case, when the selection is adjusted, we
// want to ensure it correctly traverses the tree back to the previous text by
// correctly skipping the invisible div but not skipping the second <p>.
// See crbug.com/1154308 for more context.
TEST_F(TextFragmentSelectorGeneratorTest, PrevPrevNodeIsSiblingsChild) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  // HTML is intentionally not formatted. Adding new lines and indentation
  // creates empty text nodes which changes the dom tree.
  request.Complete(R"HTML(
    <!DOCTYPE html>
  <div><p id='start'>First paragraph</p></div><div style='display:none'>test</div><p id='end'>Second paragraph</p>
  )HTML");
  GetDocument().UpdateStyleAndLayoutTree();
  Node* first_paragraph = GetDocument().getElementById("start")->firstChild();
  Node* second_paragraph = GetDocument().getElementById("end");
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(second_paragraph, 0);
  ASSERT_EQ("First paragraph\n\n", PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "First%20paragraph,-Second%20paragraph");
}

// Checks that for short selection that have nested block element range selector
// is used.
TEST_F(TextFragmentSelectorGeneratorTest, RangeSelector_SameNode_Interrupted) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <div>block text</div> paragraph text</div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph->nextSibling()->nextSibling(), 10);
  ASSERT_EQ("First\nblock text\nparagraph",
            PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "First,paragraph");
}

// Check min number of words is used for context if possible.
TEST_F(TextFragmentSelectorGeneratorTest, MultiwordContext) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <p id='second'>Second paragraph text that is short</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& selected_start = Position(first_paragraph, 6);
  const auto& selected_end = Position(first_paragraph, 28);
  ASSERT_EQ("paragraph text that is",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "First-,paragraph%20text%20that%20is,-longer%20than%2020");
}

// Check min number of words is used for range if possible.
TEST_F(TextFragmentSelectorGeneratorTest, MultiWordRangeSelector) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>Test page</div>
    <p id='first'>First paragraph text that is longer than 20 chars</p>
    <p id='second'>Second paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& selected_start = Position(first_paragraph, 0);
  const auto& selected_end = Position(second_paragraph, 6);
  ASSERT_EQ("First paragraph text that is longer than 20 chars\n\nSecond",
            PlainText(EphemeralRange(selected_start, selected_end)));

  VerifySelector(selected_start, selected_end,
                 "First%20paragraph%20text,Second");
}

// Checks the case when selection end position is a non text node.
TEST_F(TextFragmentSelectorGeneratorTest, SelectionEndsWithNonText) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
  <!DOCTYPE html>
  <div id='div'>
    <p id='start'>First paragraph</p>
    <p id='second'>Second paragraph</p>
  </div>
  )HTML");
  GetDocument().UpdateStyleAndLayoutTree();
  Node* first_paragraph = GetDocument().getElementById("start")->firstChild();
  Node* div = GetDocument().getElementById("div");
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(div, 2);
  ASSERT_EQ("First paragraph\n\n", PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "First%20paragraph,-Second%20paragraph");
}

// Checks the case when selection end position is a non text node which doesn't
// have text child node.
TEST_F(TextFragmentSelectorGeneratorTest,
       SelectionEndsWithNonTextWithNoTextChild) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
  <div id='div'><p id='start'>First paragraph</p><p id='second'>Second paragraph</p><img id="img">
  </div>
  )HTML");
  GetDocument().UpdateStyleAndLayoutTree();
  Node* first_paragraph = GetDocument().getElementById("start")->firstChild();
  Node* div = GetDocument().getElementById("div");
  const auto& start = Position(first_paragraph, 0);
  const auto& end =
      Position(div, 3);  // Points to the 3rd child of the div, which is <img>
  ASSERT_EQ("First paragraph\n\nSecond paragraph\n\n",
            PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "First%20paragraph,Second%20paragraph");
}

// Checks the case when selection end position is a non text node which doesn't
// have text child node.
TEST_F(TextFragmentSelectorGeneratorTest, SelectionEndsWithImageDiv) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
  <div id='div'><p id='start'>First paragraph</p><p id='second'>Second paragraph</p><div id='div_img'><img id="img"></div>
  </div>
  )HTML");
  GetDocument().UpdateStyleAndLayoutTree();
  Node* first_paragraph = GetDocument().getElementById("start")->firstChild();
  Node* div = GetDocument().getElementById("div");
  const auto& start = Position(first_paragraph, 0);
  const auto& end =
      Position(div, 3);  // Points to the 3rd child of the div, which is div_img
  ASSERT_EQ("First paragraph\n\nSecond paragraph\n\n",
            PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "First%20paragraph,Second%20paragraph");
}

// Checks the case when selected range contains a range with same start and end.
// The problematic case should have both range end and suffix.
TEST_F(TextFragmentSelectorGeneratorTest, OverlappingRange) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <div>block text</div>text suffix</div>
  )HTML");
  GetDocument().UpdateStyleAndLayoutTree();
  Node* start_node = GetDocument().getElementById("first")->firstChild();
  Node* end_node = GetDocument().getElementById("first")->lastChild();
  const auto& start = Position(start_node, 0);
  const auto& end = Position(end_node, 4);
  ASSERT_EQ("First\nblock text\ntext", PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "First,text,-suffix");
}

// Checks selection across table cells.
TEST_F(TextFragmentSelectorGeneratorTest, Table) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
<table id='table'>
  <tbody>
    <tr>
      <td id='row1-col1'>row1 col1</td>
      <td id='row1-col2'>row1 col2</td>
      <td id='row1-col3'>row1 col3</td>
    </tr>
  </tbody>
</table>
  )HTML");
  GetDocument().UpdateStyleAndLayoutTree();
  Node* first_paragraph =
      GetDocument().getElementById("row1-col1")->firstChild();
  Node* second_paragraph =
      GetDocument().getElementById("row1-col3")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(second_paragraph, 9);
  ASSERT_EQ("row1 col1\trow1 col2\trow1 col3",
            PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "row1%20col1,row1%20col3");
}

// Checks selection across an input element.
TEST_F(TextFragmentSelectorGeneratorTest, Input) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
  <div id='div'>
    First paragraph<input type='text'> Second paragraph
  </div>
  )HTML");
  GetDocument().UpdateStyleAndLayoutTree();
  Node* div = GetDocument().getElementById("div");
  const auto& start = Position(div->firstChild(), 0);
  const auto& end = Position(div->lastChild(), 7);
  ASSERT_EQ("First paragraph Second", PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "First%20paragraph,Second");
}

// Checks selection across a shadow tree. Input that has text value will create
// a shadow tree,
TEST_F(TextFragmentSelectorGeneratorTest, InputSubmit) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
  <div id='div'>
    First paragraph<input type='submit' value="button text"> Second paragraph
  </div>
  )HTML");
  GetDocument().UpdateStyleAndLayoutTree();
  Node* div = GetDocument().getElementById("div");
  const auto& start = Position(div->firstChild(), 0);
  const auto& end = Position(div->lastChild(), 7);
  ASSERT_EQ("First paragraph Second", PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "First%20paragraph,Second");
}

// Checks that haphen, ampersand and comma in selector are escaped.
// crbug.com/1245669
TEST_F(TextFragmentSelectorGeneratorTest, EscapeSelectorSpecialChars) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
  <div id='div'>First paragraph with hyphen- ampersand& and comma,</div>
  )HTML");
  GetDocument().UpdateStyleAndLayoutTree();
  Node* div = GetDocument().getElementById("div");
  const auto& start = Position(div->firstChild(), 0);
  const auto& end = Position(div->firstChild(), 50);
  ASSERT_EQ("First paragraph with hyphen- ampersand& and comma,",
            PlainText(EphemeralRange(start, end)));

  VerifySelector(
      start, end,
      "First%20paragraph%20with%20hyphen%2D%20ampersand%26%20and%20comma%2C");
}

// Checks selection right after a shadow tree will use the shadow tree for
// prefix. Input with text value will create a shadow tree.
TEST_F(TextFragmentSelectorGeneratorTest, InputSubmitPrefix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
  <div id='div'>
    <input type='submit' value="button text"> paragraph text
  </div>
  )HTML");
  GetDocument().UpdateStyleAndLayoutTree();
  Node* div = GetDocument().getElementById("div");
  const auto& start = Position(div->lastChild(), 0);
  const auto& end = Position(div->lastChild(), 10);
  ASSERT_EQ(" paragraph", PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "button%20text-,paragraph,-text");
}

// Checks selection right after a shadow tree will use the shadow tree for
// prefix. Input with text value will create a shadow tree.
TEST_F(TextFragmentSelectorGeneratorTest, InputSubmitOneWordPrefix) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
  <div id='div'>
    <input type='submit' value="button"> paragraph text
  </div>
  )HTML");
  GetDocument().UpdateStyleAndLayoutTree();
  Node* div = GetDocument().getElementById("div");
  const auto& start = Position(div->lastChild(), 0);
  const auto& end = Position(div->lastChild(), 10);
  ASSERT_EQ(" paragraph", PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "button-,paragraph,-text");
}

// Ensure generation works correctly when the range begins anchored to a shadow
// host. The shadow root has more children than the shadow host so this ensures
// we're using flat tree node traversals.
TEST_F(TextFragmentSelectorGeneratorTest, RangeBeginsOnShadowHost) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
  <div id='host'></div>
  the quick brown fox jumped over the lazy dog.
  )HTML");

  Element* host = GetDocument().getElementById("host");
  ShadowRoot& root = host->AttachShadowRootInternal(ShadowRootType::kOpen);
  root.appendChild(MakeGarbageCollected<HTMLDivElement>(root.GetDocument()));
  root.appendChild(MakeGarbageCollected<HTMLDivElement>(root.GetDocument()));

  Compositor().BeginFrame();

  const auto& start = Position(host, PositionAnchorType::kAfterChildren);
  const auto& end = Position(host->nextSibling(), 12);
  ASSERT_EQ("the quick", PlainText(EphemeralRange(start, end)));

  VerifySelector(start, end, "the%20quick,-brown%20fox%20jumped");
}

// Basic test case for |GetNextTextBlock|.
TEST_F(TextFragmentSelectorGeneratorTest, GetPreviousTextBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 16);
  const auto& end = Position(first_paragraph, 20);
  ASSERT_EQ("text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("First paragraph",
            GetTextFragmentSelectorGenerator()->GetPreviousTextBlockForTesting(
                start));
}

// Check the case when available prefix contains collapsible space.
TEST_F(TextFragmentSelectorGeneratorTest, GetPreviousTextBlock_ExtraSpace) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First

         paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 26);
  const auto& end = Position(first_paragraph, 30);
  ASSERT_EQ("text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("First paragraph",
            GetTextFragmentSelectorGenerator()->GetPreviousTextBlockForTesting(
                start));
}

// Check the case when available prefix complete text content of the previous
// block.
TEST_F(TextFragmentSelectorGeneratorTest, GetPreviousTextBlock_PrevNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
    <p id='second'>Second paragraph text</p>
  )HTML");
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& start = Position(second_paragraph, 0);
  const auto& end = Position(second_paragraph, 6);
  ASSERT_EQ("Second", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("First paragraph text",
            GetTextFragmentSelectorGenerator()->GetPreviousTextBlockForTesting(
                start));
}

// Check the case when there is a commented block between selection and the
// available prefix.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetPreviousTextBlock_PrevNode_WithComment) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
    <!--
      multiline comment that should be ignored.
    //-->
    <p id='second'>Second paragraph text</p>
  )HTML");
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& start = Position(second_paragraph, 0);
  const auto& end = Position(second_paragraph, 6);
  ASSERT_EQ("Second", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("First paragraph text",
            GetTextFragmentSelectorGenerator()->GetPreviousTextBlockForTesting(
                start));
}

// Check the case when available prefix is a text node outside of selection
// block.
TEST_F(TextFragmentSelectorGeneratorTest, GetPreviousTextBlock_PrevTextNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    text
    <p id='first'>First paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("text",
            GetTextFragmentSelectorGenerator()->GetPreviousTextBlockForTesting(
                start));
}

// Check the case when available prefix is a parent node text content outside of
// selection block.
TEST_F(TextFragmentSelectorGeneratorTest, GetPreviousTextBlock_ParentNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div>nested
    <p id='first'>First paragraph text</p></div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("nested",
            GetTextFragmentSelectorGenerator()->GetPreviousTextBlockForTesting(
                start));
}

// Check the case when available prefix contains non-block tag(e.g. <b>).
TEST_F(TextFragmentSelectorGeneratorTest, GetPreviousTextBlock_NestedTextNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First <b>bold text</b> paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->lastChild();
  const auto& start = Position(first_paragraph, 11);
  const auto& end = Position(first_paragraph, 15);
  ASSERT_EQ("text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("First bold text paragraph",
            GetTextFragmentSelectorGenerator()->GetPreviousTextBlockForTesting(
                start));
}

// Check the case when available prefix is collected until nested block.
TEST_F(TextFragmentSelectorGeneratorTest, GetPreviousTextBlock_NestedBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <div id='div'>div</div> paragraph text</div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("div")->nextSibling();
  const auto& start = Position(first_paragraph, 11);
  const auto& end = Position(first_paragraph, 15);
  ASSERT_EQ("text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("paragraph",
            GetTextFragmentSelectorGenerator()->GetPreviousTextBlockForTesting(
                start));
}

// Check the case when available prefix includes non-block element but stops at
// nested block.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetPreviousTextBlock_NestedBlockInNestedText) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <b><div id='div'>div</div>bold</b> paragraph text</div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->lastChild();
  const auto& start = Position(first_paragraph, 11);
  const auto& end = Position(first_paragraph, 15);
  ASSERT_EQ("text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("bold paragraph",
            GetTextFragmentSelectorGenerator()->GetPreviousTextBlockForTesting(
                start));
}

// Check the case when available prefix includes invisible block.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetPreviousTextBlock_NestedInvisibleBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <div id='div' style='display:none'>invisible</div> paragraph text</div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("div")->nextSibling();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 10);
  ASSERT_EQ("paragraph", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("First",
            GetTextFragmentSelectorGenerator()->GetPreviousTextBlockForTesting(
                start));
}

// Check the case when previous node is used for available prefix when selection
// is not at index=0 but there is only space before it.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetPreviousTextBlock_SpacesBeforeSelection) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
    <p id='second'>
      Second paragraph text
    </p>
  )HTML");
  Node* second_paragraph = GetDocument().getElementById("second")->firstChild();
  const auto& start = Position(second_paragraph, 6);
  const auto& end = Position(second_paragraph, 13);
  ASSERT_EQ("Second", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("First paragraph text",
            GetTextFragmentSelectorGenerator()->GetPreviousTextBlockForTesting(
                start));
}

// Check the case when previous node is used for available prefix when selection
// is not at index=0 but there is only invisible block.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetPreviousTextBlock_InvisibleBeforeSelection) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
    <div id='second'>
      <p id='invisible' style='display:none'>
        invisible text
      </p>
      Second paragraph text
    </div>
  )HTML");
  Node* second_paragraph =
      GetDocument().getElementById("invisible")->nextSibling();
  const auto& start = Position(second_paragraph, 6);
  const auto& end = Position(second_paragraph, 13);
  ASSERT_EQ("Second", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ("First paragraph text",
            GetTextFragmentSelectorGenerator()->GetPreviousTextBlockForTesting(
                start));
}

// Similar test for suffix.

// Basic test case for |GetNextTextBlock|.
TEST_F(TextFragmentSelectorGeneratorTest, GetNextTextBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ(
      "paragraph text",
      GetTextFragmentSelectorGenerator()->GetNextTextBlockForTesting(end));
}

// Check the case when available suffix contains collapsible space.
TEST_F(TextFragmentSelectorGeneratorTest, GetNextTextBlock_ExtraSpace) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph


     text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ(
      "paragraph text",
      GetTextFragmentSelectorGenerator()->GetNextTextBlockForTesting(end));
}

// Check the case when available suffix is complete text content of the next
// block.
TEST_F(TextFragmentSelectorGeneratorTest, GetNextTextBlock_NextNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
    <p id='second'>Second paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 20);
  ASSERT_EQ("First paragraph text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ(
      "Second paragraph text",
      GetTextFragmentSelectorGenerator()->GetNextTextBlockForTesting(end));
}

// Check the case when there is a commented block between selection and the
// available suffix.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetNextTextBlock_NextNode_WithComment) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
    <!--
      multiline comment that should be ignored.
    //-->
    <p id='second'>Second paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 20);
  ASSERT_EQ("First paragraph text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ(
      "Second paragraph text",
      GetTextFragmentSelectorGenerator()->GetNextTextBlockForTesting(end));
}

// Check the case when available suffix is a text node outside of selection
// block.
TEST_F(TextFragmentSelectorGeneratorTest, GetNextTextBlock_NextTextNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First paragraph text</p>
    text
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 20);
  ASSERT_EQ("First paragraph text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ(
      "text",
      GetTextFragmentSelectorGenerator()->GetNextTextBlockForTesting(end));
}

// Check the case when available suffix is a parent node text content outside of
// selection block.
TEST_F(TextFragmentSelectorGeneratorTest, GetNextTextBlock_ParentNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div><p id='first'>First paragraph text</p> nested</div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 20);
  ASSERT_EQ("First paragraph text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ(
      "nested",
      GetTextFragmentSelectorGenerator()->GetNextTextBlockForTesting(end));
}

// Check the case when available suffix contains non-block tag(e.g. <b>).
TEST_F(TextFragmentSelectorGeneratorTest, GetNextTextBlock_NestedTextNode) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>First <b>bold text</b> paragraph text</p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ(
      "bold text paragraph text",
      GetTextFragmentSelectorGenerator()->GetNextTextBlockForTesting(end));
}

// Check the case when available suffix is collected until nested block.
TEST_F(TextFragmentSelectorGeneratorTest, GetNextTextBlock_NestedBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First paragraph <div id='div'>div</div> text</div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ(
      "paragraph",
      GetTextFragmentSelectorGenerator()->GetNextTextBlockForTesting(end));
}

// Check the case when available suffix includes non-block element but stops at
// nested block.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetNextTextBlock_NestedBlockInNestedText) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <b>bold<div id='div'>div</div></b> paragraph text</div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ(
      "bold",
      GetTextFragmentSelectorGenerator()->GetNextTextBlockForTesting(end));
}

// Check the case when available suffix includes invisible block.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetNextTextBlock_NestedInvisibleBlock) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>First <div id='div' style='display:none'>invisible</div> paragraph text</div>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  const auto& end = Position(first_paragraph, 5);
  ASSERT_EQ("First", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ(
      "paragraph text",
      GetTextFragmentSelectorGenerator()->GetNextTextBlockForTesting(end));
}

// Check the case when next node is used for available suffix when selection is
// not at last index but there is only space after it.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetNextTextBlock_SpacesAfterSelection) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <p id='first'>
      First paragraph text
    </p>
    <p id='second'>
      Second paragraph text
    </p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 23);
  const auto& end = Position(first_paragraph, 27);
  ASSERT_EQ("text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ(
      "Second paragraph text",
      GetTextFragmentSelectorGenerator()->GetNextTextBlockForTesting(end));
}

// Check the case when next node is used for available suffix when selection is
// not at last index but there is only invisible block after it.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetNextTextBlock_InvisibleAfterSelection) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>
      First paragraph text
      <div id='invisible' style='display:none'>
        invisible text
      </div>
    </div>
    <p id='second'>
      Second paragraph text
    </p>
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 23);
  const auto& end = Position(first_paragraph, 27);
  ASSERT_EQ("text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ(
      "Second paragraph text",
      GetTextFragmentSelectorGenerator()->GetNextTextBlockForTesting(end));
}

// Check the case when previous node is used for available prefix when selection
// is not at last index but there is only invisible block. Invisible block
// contains another block which also should be invisible.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetNextTextBlock_InvisibleAfterSelection_WithNestedInvisible) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id='first'>
      First paragraph text
      <div id='invisible' style='display:none'>
        invisible text
        <div>
          nested invisible text
        </div
      </div>
    </div>
    <p id='second'>
      Second paragraph text
      <div id='invisible' style='display:none'>
        invisible text
        <div>
          nested invisible text
        </div
      </div>
    </p>
    test
  )HTML");
  Node* first_paragraph = GetDocument().getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 23);
  const auto& end = Position(first_paragraph, 27);
  ASSERT_EQ("text", PlainText(EphemeralRange(start, end)));

  EXPECT_EQ(
      "Second paragraph text",
      GetTextFragmentSelectorGenerator()->GetNextTextBlockForTesting(end));
}

TEST_F(TextFragmentSelectorGeneratorTest, BeforeAndAfterAnchor) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    Foo
    <div id="first">Hello World</div>
    Bar
  )HTML");

  Node* node = GetDocument().getElementById("first");
  const auto& start = Position(node, PositionAnchorType::kBeforeAnchor);
  const auto& end = Position(node, PositionAnchorType::kAfterAnchor);
  VerifySelectorFails(start, end, LinkGenerationError::kEmptySelection);
}

// Check the case when GetPreviousTextBlock is an EOL node from Shadow Root.
TEST_F(TextFragmentSelectorGeneratorTest,
       GetPreviousTextBlock_ShouldSkipNodesWithNoLayoutObject) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <div id="host1"></div>
  )HTML");
  ShadowRoot& shadow1 =
      GetDocument().getElementById("host1")->AttachShadowRootInternal(
          ShadowRootType::kOpen);
  shadow1.setInnerHTML(R"HTML(
    <style>
          :host {display: contents;}
    </style>
    <p>Right click the link below to experience a crash:</p>
    <a href="/foo" id='first'>I crash</a>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(GetDocument().View()->NeedsLayout());
  Node* first_paragraph = shadow1.getElementById("first")->firstChild();
  const auto& start = Position(first_paragraph, 0);
  EXPECT_EQ("Right click the link below to experience a crash:",
            GetTextFragmentSelectorGenerator()->GetPreviousTextBlockForTesting(
                start));
}

}  // namespace blink
