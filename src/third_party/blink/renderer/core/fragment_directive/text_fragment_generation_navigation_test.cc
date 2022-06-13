// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "components/shared_highlighting/core/common/shared_highlighting_data_driven_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/fragment_directive/text_directive.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_anchor.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_handler.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_selector_generator.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using test::RunPendingTasks;

class TextFragmentGenerationNavigationTest
    : public shared_highlighting::SharedHighlightingDataDrivenTest,
      public testing::WithParamInterface<base::FilePath>,
      public SimTest {
 public:
  TextFragmentGenerationNavigationTest() = default;
  ~TextFragmentGenerationNavigationTest() override = default;

  void SetUp() override;

  void RunAsyncMatchingTasks();
  TextFragmentAnchor* GetTextFragmentAnchor();

  // SharedHighlightingDataDrivenTest:
  void GenerateAndNavigate(std::string html_content,
                           std::string start_node_name,
                           int start_offset,
                           std::string end_node_name,
                           int end_offset,
                           std::string selected_text,
                           std::string highlight_text) override;

  void LoadHTML(String url, String html_content);

  RangeInFlatTree* GetSelectionRange(AtomicString start_node_name,
                                     int start_offset,
                                     AtomicString end_node_name,
                                     int end_offset);

  String GenerateSelector(const RangeInFlatTree& selection_range);

  String GetHighlightedText();
};

void TextFragmentGenerationNavigationTest::SetUp() {
  SimTest::SetUp();
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
}

void TextFragmentGenerationNavigationTest::RunAsyncMatchingTasks() {
  auto* scheduler = blink::scheduler::WebThreadScheduler::MainThreadScheduler();
  blink::scheduler::RunIdleTasksForTesting(scheduler, base::BindOnce([]() {}));
  RunPendingTasks();
}

TextFragmentAnchor*
TextFragmentGenerationNavigationTest::GetTextFragmentAnchor() {
  FragmentAnchor* fragmentAnchor =
      GetDocument().GetFrame()->View()->GetFragmentAnchor();
  if (!fragmentAnchor || !fragmentAnchor->IsTextFragmentAnchor()) {
    return nullptr;
  }
  return static_cast<TextFragmentAnchor*>(fragmentAnchor);
}

void TextFragmentGenerationNavigationTest::LoadHTML(String url,
                                                    String html_content) {
  SimRequest request(url, "text/html");
  LoadURL(url);
  request.Complete(html_content);
}

RangeInFlatTree* TextFragmentGenerationNavigationTest::GetSelectionRange(
    AtomicString start_node_name,
    int start_offset,
    AtomicString end_node_name,
    int end_offset) {
  Node* start_node =
      GetDocument().getElementById(start_node_name)->firstChild();
  Node* end_node = GetDocument().getElementById(end_node_name)->firstChild();
  const auto& selected_start = Position(start_node, start_offset);
  const auto& selected_end = Position(end_node, end_offset);

  return MakeGarbageCollected<RangeInFlatTree>(
      ToPositionInFlatTree(selected_start), ToPositionInFlatTree(selected_end));
}

String TextFragmentGenerationNavigationTest::GenerateSelector(
    const RangeInFlatTree& selection_range) {
  String selector;
  auto lambda =
      [](String& selector, const TextFragmentSelector& generated_selector,
         absl::optional<shared_highlighting::LinkGenerationError> error) {
        selector = generated_selector.ToString();
      };
  auto callback = WTF::Bind(lambda, std::ref(selector));
  GetDocument()
      .GetFrame()
      ->GetTextFragmentHandler()
      ->GetTextFragmentSelectorGenerator()
      ->Generate(selection_range, std::move(callback));
  base::RunLoop().RunUntilIdle();
  return selector;
}

String TextFragmentGenerationNavigationTest::GetHighlightedText() {
  TextFragmentAnchor* anchor = GetTextFragmentAnchor();
  if (!anchor || anchor->DirectiveFinderPairs().size() <= 0) {
    // Returns a null string, distinguishable from an empty string.
    return String();
  }

  auto directive_finder_pairs = anchor->DirectiveFinderPairs();
  EXPECT_EQ(1u, directive_finder_pairs.size());
  return PlainText(
      directive_finder_pairs[0].second.Get()->FirstMatch()->ToEphemeralRange());
}

void TextFragmentGenerationNavigationTest::GenerateAndNavigate(
    std::string html_content,
    std::string start_node_name,
    int start_offset,
    std::string end_node_name,
    int end_offset,
    std::string selected_text,
    std::string highlight_text) {
  String base_url = "https://example.com/test.html";
  String html_content_wtf = String::FromUTF8(html_content.c_str());
  LoadHTML(base_url, html_content_wtf);

  RangeInFlatTree* selection_range = GetSelectionRange(
      AtomicString::FromUTF8(start_node_name.c_str()), start_offset,
      AtomicString::FromUTF8(end_node_name.c_str()), end_offset);
  ASSERT_EQ(String::FromUTF8(selected_text.c_str()),
            PlainText(selection_range->ToEphemeralRange()));

  // Generate text fragment selector.
  String selector = GenerateSelector(*selection_range);

  // Navigate to generated link to text.
  String link_to_text_url = base_url + "#:~:text=" + selector;
  LoadHTML(link_to_text_url, html_content_wtf);

  RunAsyncMatchingTasks();
  Compositor().BeginFrame();

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  String actual_highlighted_text = GetHighlightedText();
  ASSERT_TRUE(actual_highlighted_text);

  String expected_highlighted_text = String::FromUTF8(highlight_text.c_str());
  EXPECT_EQ(expected_highlighted_text, actual_highlighted_text);
}

TEST_P(TextFragmentGenerationNavigationTest,
       DataDrivenGenerationAndNavigation) {
  RunOneDataDrivenTest(GetParam(), GetOutputDirectory(),
                       /* kIsExpectedToPass */ true);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TextFragmentGenerationNavigationTest,
    testing::ValuesIn(
        shared_highlighting::SharedHighlightingDataDrivenTest::GetTestFiles()));

}  // namespace blink
