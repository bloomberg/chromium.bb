// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/frame/find_in_page.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {
class InvisibleFindInPageClient : public mojom::blink::FindInPageClient {
 public:
  InvisibleFindInPageClient()
      : binding_(this),
        active_index_(-1),
        count_(-1),
        find_results_are_ready_(false) {}

  ~InvisibleFindInPageClient() override = default;

  void SetFrame(WebLocalFrameImpl* frame) {
    mojom::blink::FindInPageClientPtr client;
    binding_.Bind(MakeRequest(&client));
    frame->GetFindInPage()->SetClient(std::move(client));
  }

  void SetNumberOfMatches(
      int request_id,
      unsigned int current_number_of_matches,
      mojom::blink::FindMatchUpdateType final_update) final {
    count_ = current_number_of_matches;
    find_results_are_ready_ =
        (final_update == mojom::blink::FindMatchUpdateType::kFinalUpdate);
  }

  void SetActiveMatch(int request_id,
                      const WebRect& active_match_rect,
                      int active_match_ordinal,
                      mojom::blink::FindMatchUpdateType final_update) final {
    active_index_ = active_match_ordinal;
    find_results_are_ready_ =
        (final_update == mojom::blink::FindMatchUpdateType::kFinalUpdate);
  }

  bool FindResultsAreReady() const { return find_results_are_ready_; }
  int Count() const { return count_; }
  int ActiveIndex() const { return active_index_; }

  void Reset() {
    find_results_are_ready_ = false;
    count_ = -1;
    active_index_ = -1;
  }

 private:
  mojo::Binding<mojom::blink::FindInPageClient> binding_;
  int active_index_;
  int count_;
  bool find_results_are_ready_;
};

class FindInvisibleTest : public ::testing::WithParamInterface<bool>,
                          public testing::Test,
                          private ScopedLayoutNGForTest {
 protected:
  FindInvisibleTest() : ScopedLayoutNGForTest(GetParam()) {}

  bool LayoutNGEnabled() const { return GetParam(); }

  void SetUp() override { web_view_helper_.Initialize(); }

  void TearDown() override { web_view_helper_.Reset(); }

  Document& GetDocument() {
    return *static_cast<Document*>(
        web_view_helper_.LocalMainFrame()->GetDocument());
  }

  FindInPage* GetFindInPage() {
    return web_view_helper_.LocalMainFrame()->GetFindInPage();
  }

  WebLocalFrameImpl* LocalMainFrame() {
    return web_view_helper_.LocalMainFrame();
  }

  void UpdateAllLifecyclePhasesForTest() {
    GetDocument().View()->UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kTest);
  }

  void SetInnerHTML(const char* content) {
    GetDocument().documentElement()->SetInnerHTMLFromString(
        String::FromUTF8(content));
    UpdateAllLifecyclePhasesForTest();
  }

  void ResizeAndFocus() {
    web_view_helper_.Resize(WebSize(640, 480));
    web_view_helper_.GetWebView()->MainFrameWidget()->SetFocus(true);
    test::RunPendingTasks();
  }

 private:
  frame_test_helpers::WebViewHelper web_view_helper_;
};

INSTANTIATE_TEST_CASE_P(All, FindInvisibleTest, ::testing::Bool());

TEST_P(FindInvisibleTest, FindingInvisibleTextHasCorrectCount) {
  ResizeAndFocus();
  SetInnerHTML(
      "<div invisible>"
      " foo1"
      " <div>foo2</div>"
      " foo3"
      "</div>"
      "<div>"
      " bar"
      " <span invisible>foo4</span>"
      "</div>");
  auto* find_in_page = GetFindInPage();

  InvisibleFindInPageClient client;
  client.SetFrame(LocalMainFrame());
  auto find_options = mojom::blink::FindOptions::New();
  find_options->run_synchronously_for_testing = true;
  find_options->find_next = false;
  find_options->forward = true;

  int current_id = 0;
  find_in_page->Find(current_id++, "foo", find_options->Clone());
  EXPECT_FALSE(client.FindResultsAreReady());
  test::RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());
  EXPECT_EQ(4, client.Count());
  // Active match is not supported yet for invisible elements.
  EXPECT_EQ(-1, client.ActiveIndex());
}

TEST_P(FindInvisibleTest, FindingInvisibleTextHasIncorrectActiveMatch) {
  ResizeAndFocus();
  SetInnerHTML(
      "<div id='div'>1foo<div invisible>22foo</div>333foo</div>"
      "<div invisible><input id='input' value='4444foo'></div>55555foo");
  auto* find_in_page = GetFindInPage();

  InvisibleFindInPageClient client;
  client.SetFrame(LocalMainFrame());
  auto find_options = mojom::blink::FindOptions::New();
  find_options->run_synchronously_for_testing = true;
  find_options->find_next = false;
  find_options->forward = true;

  int current_id = 0;
  find_in_page->Find(current_id++, "foo", find_options->Clone());
  test::RunPendingTasks();
  find_in_page->StopFinding(
      mojom::blink::StopFindAction::kStopFindActionKeepSelection);
  EXPECT_TRUE(client.FindResultsAreReady());
  EXPECT_EQ(5, client.Count());

  // Finding next focuses on "1foo".
  WebRange range = LocalMainFrame()->SelectionRange();
  ASSERT_FALSE(range.IsNull());
  EXPECT_EQ(1, range.StartOffset());
  EXPECT_EQ(4, range.EndOffset());

  find_options->find_next = true;
  find_in_page->Find(current_id++, "foo", find_options->Clone());
  test::RunPendingTasks();
  find_in_page->StopFinding(
      mojom::blink::StopFindAction::kStopFindActionKeepSelection);
  // Finding next focuses on "333foo", skipping "22foo".
  range = LocalMainFrame()->SelectionRange();
  ASSERT_FALSE(range.IsNull());
  EXPECT_EQ(3, range.StartOffset());
  EXPECT_EQ(6, range.EndOffset());

  find_options->find_next = true;
  find_in_page->Find(current_id++, "foo", find_options->Clone());
  test::RunPendingTasks();
  find_in_page->StopFinding(
      mojom::blink::StopFindAction::kStopFindActionKeepSelection);
  // Finding next focuses on "55555foo", skipping "4444foo".
  range = LocalMainFrame()->SelectionRange();
  ASSERT_FALSE(range.IsNull());
  EXPECT_EQ(5, range.StartOffset());
  EXPECT_EQ(8, range.EndOffset());
}

}  // namespace blink
