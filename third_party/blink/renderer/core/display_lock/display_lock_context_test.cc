// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_options.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/find_in_page.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {
namespace {
class DisplayLockTestFindInPageClient : public mojom::blink::FindInPageClient {
 public:
  DisplayLockTestFindInPageClient()
      : find_results_are_ready_(false),
        active_index_(-1),
        count_(-1),
        binding_(this) {}

  ~DisplayLockTestFindInPageClient() override = default;

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
    active_match_rect_ = active_match_rect;
    active_index_ = active_match_ordinal;
    find_results_are_ready_ =
        (final_update == mojom::blink::FindMatchUpdateType::kFinalUpdate);
  }

  bool FindResultsAreReady() const { return find_results_are_ready_; }
  int Count() const { return count_; }
  int ActiveIndex() const { return active_index_; }
  IntRect ActiveMatchRect() const { return active_match_rect_; }

  void Reset() {
    find_results_are_ready_ = false;
    count_ = -1;
    active_index_ = -1;
    active_match_rect_ = IntRect();
  }

 private:
  IntRect active_match_rect_;
  bool find_results_are_ready_;
  int active_index_;

  int count_;
  mojo::Binding<mojom::blink::FindInPageClient> binding_;
};

}  // namespace

class DisplayLockContextTest : public testing::Test {
 public:
  void SetUp() override {
    features_backup_.emplace();
    RuntimeEnabledFeatures::SetDisplayLockingEnabled(true);
    web_view_helper_.Initialize();
  }

  void TearDown() override {
    if (features_backup_) {
      features_backup_->Restore();
      features_backup_.reset();
    }
    web_view_helper_.Reset();
  }

  Document& GetDocument() {
    return *static_cast<Document*>(
        web_view_helper_.LocalMainFrame()->GetDocument());
  }
  TextFinder& GetTextFinder() {
    return web_view_helper_.LocalMainFrame()->EnsureTextFinder();
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

  void SetHtmlInnerHTML(const char* content) {
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
  base::Optional<RuntimeEnabledFeatures::Backup> features_backup_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(DisplayLockContextTest, LockedElementIsNotSearchableViaTextFinder) {
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: content;
    }
    </style>
    <body><div id="container">testing</div></body>
  )HTML");

  int identifier = 0;
  WebString search_text(String("testing"));
  auto& text_finder = GetTextFinder();
  auto find_options = mojom::blink::FindOptions::New();
  bool wrap_within_frame = true;

  ASSERT_TRUE(text_finder.Find(identifier, search_text, *find_options,
                               wrap_within_frame));
  text_finder.ClearActiveFindMatch();
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  auto* element = GetDocument().getElementById("container");
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  // We should be in pending acquire state. In this mode, we're still
  // technically not locked.
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint());
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  UpdateAllLifecyclePhasesForTest();

  // Sanity checks to ensure the element is locked.
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint());
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 1);

  EXPECT_FALSE(element->GetDisplayLockContext()->IsActivatable());

  EXPECT_FALSE(text_finder.Find(identifier, search_text, *find_options,
                                wrap_within_frame));

  // Now commit the lock and ensure we can find the text.
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->commit(script_state);
  }

  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldPaint());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  EXPECT_TRUE(text_finder.Find(identifier, search_text, *find_options,
                               wrap_within_frame));
}

TEST_F(DisplayLockContextTest, LockedElementIsNotSearchableViaFindInPage) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: content;
    }
    </style>
    <body><div id="container">testing</div></body>
  )HTML");

  WebString search_text(String("testing"));
  auto* find_in_page = GetFindInPage();
  ASSERT_TRUE(find_in_page);

  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());

  auto find_options = mojom::blink::FindOptions::New();
  find_options->run_synchronously_for_testing = true;
  find_options->find_next = false;
  find_options->forward = true;

  int current_id = 123;
  find_in_page->Find(current_id++, "testing", find_options->Clone());
  EXPECT_FALSE(client.FindResultsAreReady());
  test::RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());
  EXPECT_EQ(1, client.Count());
  client.Reset();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  auto* element = GetDocument().getElementById("container");
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  // We should be in pending acquire state, which means we would allow things
  // like style and layout but disallow paint. This is still considered an
  // unlocked state.
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint());
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  UpdateAllLifecyclePhasesForTest();

  // Sanity checks to ensure the element is locked.
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint());
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 1);

  EXPECT_FALSE(element->GetDisplayLockContext()->IsActivatable());

  find_in_page->Find(current_id++, "testing", find_options->Clone());
  EXPECT_FALSE(client.FindResultsAreReady());
  test::RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());
  EXPECT_EQ(0, client.Count());
  client.Reset();

  // Now commit the lock and ensure we can find the text.
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->commit(script_state);
  }

  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldPaint());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  find_in_page->Find(current_id++, "testing", find_options->Clone());
  EXPECT_FALSE(client.FindResultsAreReady());
  test::RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());
  EXPECT_EQ(1, client.Count());
  client.Reset();
}

TEST_F(DisplayLockContextTest,
       ActivatableLockedElementIsSearchableViaFindInPage) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: content;
    }
    </style>
    <body><div id="container">testing</div></body>
  )HTML");

  WebString search_text(String("testing"));
  auto* find_in_page = GetFindInPage();
  ASSERT_TRUE(find_in_page);

  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());

  auto find_options = mojom::blink::FindOptions::New();
  find_options->run_synchronously_for_testing = true;
  find_options->find_next = false;
  find_options->forward = true;

  int current_id = 123;
  find_in_page->Find(current_id++, "testing", find_options->Clone());
  EXPECT_FALSE(client.FindResultsAreReady());
  test::RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());
  EXPECT_EQ(1, client.Count());
  client.Reset();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  DisplayLockOptions options;
  options.setActivatable(true);
  auto* element = GetDocument().getElementById("container");
  auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
  {
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->acquire(script_state, &options);
  }

  UpdateAllLifecyclePhasesForTest();

  // Sanity checks to ensure the element is locked.
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint());
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  EXPECT_TRUE(element->GetDisplayLockContext()->IsActivatable());

  // Check if we can still get the same result with the same query.
  find_in_page->Find(current_id++, "testing", find_options->Clone());
  EXPECT_FALSE(client.FindResultsAreReady());
  test::RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());
  EXPECT_EQ(1, client.Count());
  EXPECT_EQ(1, client.ActiveIndex());
  client.Reset();

  // Check if the result is correct if we update the contents.
  element->SetInnerHTMLFromString(
      "<div>tes</div>ting"
      "<div style='display:none;'>testing</div>");
  find_in_page->Find(current_id++, "testing", find_options->Clone());
  EXPECT_FALSE(client.FindResultsAreReady());
  test::RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());
  EXPECT_EQ(0, client.Count());
  EXPECT_EQ(-1, client.ActiveIndex());
  client.Reset();
  // Assert the container is still locked.
  EXPECT_TRUE(element->GetDisplayLockContext()->IsLocked());

  // Check if the result is correct if we have non-activatable lock.
  element->SetInnerHTMLFromString(
      "<div>testing1</div>"
      "<div id='activatable' style='contain: style layout;'>"
      " testing2"
      " <div id='nestedNonActivatable' style='contain: style layout;'>"
      "   testing3"
      " </div>"
      "</div>"
      "<div id='nonActivatable' style='contain: style layout;'>testing4</div>");
  auto* activatable = GetDocument().getElementById("activatable");
  auto* non_activatable = GetDocument().getElementById("nonActivatable");
  auto* nested_non_activatable =
      GetDocument().getElementById("nestedNonActivatable");

  {
    ScriptState::Scope scope(script_state);
    activatable->getDisplayLockForBindings()->acquire(script_state, &options);
    nested_non_activatable->getDisplayLockForBindings()->acquire(script_state,
                                                                 nullptr);
    non_activatable->getDisplayLockForBindings()->acquire(script_state,
                                                          nullptr);
  }
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 4);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 2);

  EXPECT_TRUE(element->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(activatable->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(non_activatable->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(nested_non_activatable->GetDisplayLockContext()->IsLocked());

  find_in_page->Find(current_id++, "testing", find_options->Clone());
  EXPECT_FALSE(client.FindResultsAreReady());
  test::RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());
  EXPECT_EQ(2, client.Count());
  EXPECT_EQ(1, client.ActiveIndex());
  client.Reset();

  UpdateAllLifecyclePhasesForTest();
  // The locked container should be unlocked, since the match is inside that
  // container ("testing1" inside the div).
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 3);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 2);
  EXPECT_FALSE(element->GetDisplayLockContext()->IsLocked());
  // Since the active match isn't in any locked container, they need to be
  // locked.
  EXPECT_TRUE(activatable->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(non_activatable->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(nested_non_activatable->GetDisplayLockContext()->IsLocked());

  // Check if the result is correct if we update style.
  activatable->setAttribute("style", "contain: style layout; display: none;");
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 3);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 2);

  find_in_page->Find(current_id++, "testing", find_options->Clone());
  EXPECT_FALSE(client.FindResultsAreReady());
  test::RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());
  EXPECT_EQ(1, client.Count());
  EXPECT_EQ(1, client.ActiveIndex());
  client.Reset();

  // Now commit all the locks and ensure we can find.
  {
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->commit(script_state);
    activatable->getDisplayLockForBindings()->commit(script_state);
    nested_non_activatable->getDisplayLockForBindings()->commit(script_state);
    non_activatable->getDisplayLockForBindings()->commit(script_state);
  }

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  find_in_page->Find(current_id++, "testing", find_options->Clone());
  EXPECT_FALSE(client.FindResultsAreReady());
  test::RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());
  EXPECT_EQ(2, client.Count());
}

// Tests find-in-page active match navigation (find next/previous).
TEST_F(DisplayLockContextTest, FindInPageNavigateLockedMatches) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
      contain: content;
    }
    </style>
    <body>
      <div id="container">
        <div id="one">result</div>
        <div id="two"><b>r</b>esult</div>
        <div id="three">r<i>esul</i>t</div>
      </div>
    </body>
  )HTML");

  WebString search_text(String("result"));
  auto* find_in_page = GetFindInPage();
  ASSERT_TRUE(find_in_page);

  DisplayLockTestFindInPageClient client;
  client.SetFrame(LocalMainFrame());

  DisplayLockOptions options;
  options.setActivatable(true);

  // Lock the children and container.
  auto* container = GetDocument().getElementById("container");
  auto* div_one = GetDocument().getElementById("one");
  auto* div_two = GetDocument().getElementById("two");
  auto* div_three = GetDocument().getElementById("three");
  auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
  {
    ScriptState::Scope scope(script_state);
    container->getDisplayLockForBindings()->acquire(script_state, &options);
    div_one->getDisplayLockForBindings()->acquire(script_state, &options);
    div_two->getDisplayLockForBindings()->acquire(script_state, &options);
    div_three->getDisplayLockForBindings()->acquire(script_state, &options);
  }
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 4);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  auto find_options = mojom::blink::FindOptions::New();
  find_options->run_synchronously_for_testing = true;
  find_options->find_next = false;
  find_options->forward = true;

  int current_id = 123;

  // Find should activate "result" number 1 in "#one".
  find_in_page->Find(current_id++, search_text, find_options->Clone());
  test::RunPendingTasks();
  EXPECT_EQ(3, client.Count());
  EXPECT_EQ(1, client.ActiveIndex());

  EphemeralRange range_one = EphemeralRange::RangeOfContents(*div_one);
  ASSERT_FALSE(range_one.IsNull());
  EXPECT_EQ(ComputeTextRect(range_one), client.ActiveMatchRect());

  UpdateAllLifecyclePhasesForTest();
  // |div_one| and the container should be unlocked.
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 2);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);
  EXPECT_FALSE(container->GetDisplayLockContext()->IsLocked());
  EXPECT_FALSE(div_one->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(div_two->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(div_three->GetDisplayLockContext()->IsLocked());

  // Find next should activate "result" number 2 in "#two".
  client.Reset();
  find_options->find_next = true;
  find_in_page->Find(current_id++, search_text, find_options->Clone());
  test::RunPendingTasks();
  EXPECT_EQ(3, client.Count());
  EXPECT_EQ(2, client.ActiveIndex());

  EphemeralRange range_two = EphemeralRange::RangeOfContents(*div_two);
  ASSERT_FALSE(range_one.IsNull());
  EXPECT_EQ(ComputeTextRect(range_two), client.ActiveMatchRect());

  UpdateAllLifecyclePhasesForTest();
  // |div_two| should be unlocked.
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);
  EXPECT_FALSE(container->GetDisplayLockContext()->IsLocked());
  EXPECT_FALSE(div_one->GetDisplayLockContext()->IsLocked());
  EXPECT_FALSE(div_two->GetDisplayLockContext()->IsLocked());
  EXPECT_TRUE(div_three->GetDisplayLockContext()->IsLocked());

  // Find next should activate "result" number 3 in "#three".
  client.Reset();
  find_options->find_next = true;
  find_in_page->Find(current_id++, search_text, find_options->Clone());
  test::RunPendingTasks();
  EXPECT_EQ(3, client.Count());
  EXPECT_EQ(3, client.ActiveIndex());

  EphemeralRange range_three = EphemeralRange::RangeOfContents(*div_three);
  ASSERT_FALSE(range_three.IsNull());
  EXPECT_EQ(ComputeTextRect(range_three), client.ActiveMatchRect());

  UpdateAllLifecyclePhasesForTest();
  // |div_three| should be unlocked.
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);
  EXPECT_FALSE(container->GetDisplayLockContext()->IsLocked());
  EXPECT_FALSE(div_one->GetDisplayLockContext()->IsLocked());
  EXPECT_FALSE(div_two->GetDisplayLockContext()->IsLocked());
  EXPECT_FALSE(div_three->GetDisplayLockContext()->IsLocked());

  // Lock them again, now making |div_two| non-activatable.
  {
    ScriptState::Scope scope(script_state);
    div_one->getDisplayLockForBindings()->acquire(script_state, &options);
    div_two->getDisplayLockForBindings()->acquire(script_state, nullptr);
    div_three->getDisplayLockForBindings()->acquire(script_state, &options);
  }
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 3);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 1);

  // Find result in #one.
  find_in_page->ClearActiveFindMatch();
  client.Reset();
  find_options->find_next = false;
  find_in_page->Find(current_id++, search_text, find_options->Clone());
  test::RunPendingTasks();
  EXPECT_EQ(2, client.Count());
  EXPECT_EQ(1, client.ActiveIndex());
  EXPECT_EQ(ComputeTextRect(range_one), client.ActiveMatchRect());

  // Going forward from #one would go to #three.
  client.Reset();
  find_options->find_next = true;
  find_in_page->Find(current_id++, search_text, find_options->Clone());
  test::RunPendingTasks();
  EXPECT_EQ(2, client.Count());
  EXPECT_EQ(2, client.ActiveIndex());
  EXPECT_EQ(ComputeTextRect(range_three), client.ActiveMatchRect());

  // Going backwards from #three would go to #one.
  client.Reset();
  find_options->forward = false;
  find_in_page->Find(current_id++, search_text, find_options->Clone());
  test::RunPendingTasks();
  EXPECT_EQ(2, client.Count());
  EXPECT_EQ(1, client.ActiveIndex());
  EXPECT_EQ(ComputeTextRect(range_one), client.ActiveMatchRect());
}

TEST_F(DisplayLockContextTest, CallUpdateStyleAndLayoutAfterChange) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: content;
    }
    </style>
    <body><div id="container"><b>t</b>esting</div></body>
  )HTML");
  auto* element = GetDocument().getElementById("container");
  auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
  {
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }
  UpdateAllLifecyclePhasesForTest();

  // Sanity checks to ensure the element is locked.
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint());
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 1);

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  // Testing whitespace reattachment, shouldn't mark for reattachment.
  element->firstChild()->remove();

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  GetDocument().UpdateStyleAndLayout();

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  // Testing whitespace reattachment + dirty style.
  element->SetInnerHTMLFromString("<div>something</div>");

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_TRUE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  GetDocument().UpdateStyleAndLayout();

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_TRUE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  {
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->commit(script_state);
  }

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_TRUE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_FALSE(element->ChildNeedsReattachLayoutTree());

  // Simulating style recalc happening, will mark for reattachment.
  element->ClearChildNeedsStyleRecalc();
  element->firstChild()->ClearNeedsStyleRecalc();
  element->GetDisplayLockContext()->DidStyle();

  EXPECT_FALSE(element->NeedsStyleRecalc());
  EXPECT_FALSE(element->ChildNeedsStyleRecalc());
  EXPECT_FALSE(element->NeedsReattachLayoutTree());
  EXPECT_TRUE(element->ChildNeedsReattachLayoutTree());
}

TEST_F(DisplayLockContextTest, LockedElementAndDescendantsAreNotFocusable) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      height: 100px;
      contain: content;
    }
    </style>
    <body>
    <div id="container">
      <input id="textfield", type="text">
    </div>
    </body>
  )HTML");

  // We start off as being focusable.
  ASSERT_TRUE(GetDocument().getElementById("textfield")->IsKeyboardFocusable());
  ASSERT_TRUE(GetDocument().getElementById("textfield")->IsMouseFocusable());
  ASSERT_TRUE(GetDocument().getElementById("textfield")->IsFocusable());
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  auto* element = GetDocument().getElementById("container");
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  // We should be in pending acquire state, which means we would allow things
  // like style and layout but disallow paint. This is sitll considered an
  // unlocked state.
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint());
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  UpdateAllLifecyclePhasesForTest();

  // Sanity checks to ensure the element is locked.
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint());
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 1);

  // The input should not be focusable now.
  EXPECT_FALSE(
      GetDocument().getElementById("textfield")->IsKeyboardFocusable());
  EXPECT_FALSE(GetDocument().getElementById("textfield")->IsMouseFocusable());
  EXPECT_FALSE(GetDocument().getElementById("textfield")->IsFocusable());

  // Calling explicit focus() should also not focus the element.
  GetDocument().getElementById("textfield")->focus();
  EXPECT_FALSE(GetDocument().FocusedElement());

  // Now commit the lock and ensure we can focus the input
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->commit(script_state);
  }

  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldPaint());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);
  EXPECT_TRUE(GetDocument().getElementById("textfield")->IsKeyboardFocusable());
  EXPECT_TRUE(GetDocument().getElementById("textfield")->IsMouseFocusable());
  EXPECT_TRUE(GetDocument().getElementById("textfield")->IsFocusable());

  // Calling explicit focus() should focus the element
  GetDocument().getElementById("textfield")->focus();
  EXPECT_EQ(GetDocument().FocusedElement(),
            GetDocument().getElementById("textfield"));
}

TEST_F(DisplayLockContextTest, DisplayLockPreventsActivation) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <body>
    <div id="shadowHost">
      <div id="slotted"></div>
    </div>
    </body>
  )HTML");

  auto* host = GetDocument().getElementById("shadowHost");
  auto* slotted = GetDocument().getElementById("slotted");

  ASSERT_FALSE(host->DisplayLockPreventsActivation());
  ASSERT_FALSE(slotted->DisplayLockPreventsActivation());

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.SetInnerHTMLFromString(
      "<div id='container' style='contain:content;'><slot></slot></div>");
  UpdateAllLifecyclePhasesForTest();

  auto* container = shadow_root.getElementById("container");
  EXPECT_FALSE(host->DisplayLockPreventsActivation());
  EXPECT_FALSE(container->DisplayLockPreventsActivation());
  EXPECT_FALSE(slotted->DisplayLockPreventsActivation());

  auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
  {
    ScriptState::Scope scope(script_state);
    container->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);
  EXPECT_FALSE(host->DisplayLockPreventsActivation());
  EXPECT_FALSE(container->DisplayLockPreventsActivation());
  EXPECT_FALSE(slotted->DisplayLockPreventsActivation());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 1);
  EXPECT_FALSE(host->DisplayLockPreventsActivation());
  EXPECT_TRUE(container->DisplayLockPreventsActivation());
  EXPECT_TRUE(slotted->DisplayLockPreventsActivation());

  {
    ScriptState::Scope scope(script_state);
    container->getDisplayLockForBindings()->commit(script_state);
  }

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 1);
  EXPECT_FALSE(host->DisplayLockPreventsActivation());
  EXPECT_TRUE(container->DisplayLockPreventsActivation());
  EXPECT_TRUE(slotted->DisplayLockPreventsActivation());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);
  EXPECT_FALSE(host->DisplayLockPreventsActivation());
  EXPECT_FALSE(container->DisplayLockPreventsActivation());
  EXPECT_FALSE(slotted->DisplayLockPreventsActivation());
}

TEST_F(DisplayLockContextTest,
       LockedElementAndFlatTreeDescendantsAreNotFocusable) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <body>
    <div id="shadowHost">
      <input id="textfield" type="text">
    </div>
    </body>
  )HTML");

  auto* host = GetDocument().getElementById("shadowHost");
  auto* text_field = GetDocument().getElementById("textfield");
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.SetInnerHTMLFromString(
      "<div id='container' style='contain:content;'><slot></slot></div>");

  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(text_field->IsKeyboardFocusable());
  ASSERT_TRUE(text_field->IsMouseFocusable());
  ASSERT_TRUE(text_field->IsFocusable());

  auto* element = shadow_root.getElementById("container");
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }
  // We should be in pending acquire state, which means we would allow things
  // like style and layout but disallow paint. This is still considered an
  // unlocked state.
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint());
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  UpdateAllLifecyclePhasesForTest();

  // Sanity checks to ensure the element is locked.
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint());
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 1);

  // The input should not be focusable now.
  EXPECT_FALSE(text_field->IsKeyboardFocusable());
  EXPECT_FALSE(text_field->IsMouseFocusable());
  EXPECT_FALSE(text_field->IsFocusable());

  // Calling explicit focus() should also not focus the element.
  text_field->focus();
  EXPECT_FALSE(GetDocument().FocusedElement());
}

TEST_F(DisplayLockContextTest, LockedCountsWithMultipleLocks) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    .container {
      width: 100px;
      height: 100px;
      contain: content;
    }
    </style>
    <body>
    <div id="one" class="container">
      <div id="two" class="container"></div>
    </div>
    <div id="three" class="container"></div>
    </body>
  )HTML");

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  auto* one = GetDocument().getElementById("one");
  auto* two = GetDocument().getElementById("two");
  auto* three = GetDocument().getElementById("three");

  auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
  {
    ScriptState::Scope scope(script_state);
    one->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 1);

  {
    ScriptState::Scope scope(script_state);
    two->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 2);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 2);

  {
    ScriptState::Scope scope(script_state);
    three->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 3);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 3);

  // Now commit the inner lock.
  {
    ScriptState::Scope scope(script_state);
    two->getDisplayLockForBindings()->commit(script_state);
  }

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 2);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 2);

  // Commit the outer lock.
  {
    ScriptState::Scope scope(script_state);
    one->getDisplayLockForBindings()->commit(script_state);
  }

  UpdateAllLifecyclePhasesForTest();

  // Both inner and outer locks should have committed.
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 1);

  // Commit the sibling lock.
  {
    ScriptState::Scope scope(script_state);
    three->getDisplayLockForBindings()->commit(script_state);
  }

  UpdateAllLifecyclePhasesForTest();

  // Both inner and outer locks should have committed.
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);
}

TEST_F(DisplayLockContextTest, ActivatableNotCountedAsBlocking) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    .container {
      width: 100px;
      height: 100px;
      contain: content;
    }
    </style>
    <body>
    <div id="activatable" class="container"></div>
    <div id="nonActivatable" class="container"></div>
    </body>
  )HTML");

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  auto* activatable = GetDocument().getElementById("activatable");
  auto* non_activatable = GetDocument().getElementById("nonActivatable");

  DisplayLockOptions activatable_options;
  activatable_options.setActivatable(true);

  auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
  {
    ScriptState::Scope scope(script_state);
    activatable->getDisplayLockForBindings()->acquire(script_state,
                                                      &activatable_options);
  }

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);
  EXPECT_TRUE(activatable->GetDisplayLockContext()->IsActivatable());

  {
    ScriptState::Scope scope(script_state);
    non_activatable->getDisplayLockForBindings()->acquire(script_state,
                                                          nullptr);
  }

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 2);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 1);
  EXPECT_FALSE(non_activatable->GetDisplayLockContext()->IsActivatable());

  // Now commit the lock for |non_ctivatable|.
  {
    ScriptState::Scope scope(script_state);
    non_activatable->getDisplayLockForBindings()->commit(script_state);
  }

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);
  EXPECT_TRUE(activatable->GetDisplayLockContext()->IsActivatable());
  EXPECT_TRUE(activatable->GetDisplayLockContext()->IsActivatable());

  // Re-acquire the lock for |activatable|, but without the activatable flag.
  {
    ScriptState::Scope scope(script_state);
    activatable->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 1);
  EXPECT_FALSE(activatable->GetDisplayLockContext()->IsActivatable());

  // Re-acquire the lock for |activatable| again with the activatable flag.
  {
    ScriptState::Scope scope(script_state);
    activatable->getDisplayLockForBindings()->acquire(script_state,
                                                      &activatable_options);
  }

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);
  EXPECT_TRUE(activatable->GetDisplayLockContext()->IsActivatable());
}

TEST_F(DisplayLockContextTest, ElementInTemplate) {
  ResizeAndFocus();
  SetHtmlInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
      contain: content;
    }
    </style>
    <body>
      <template id="template"><div id="child">foo</div></template>
    </body>
  )HTML");

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  auto* template_el =
      ToHTMLTemplateElement(GetDocument().getElementById("template"));
  auto* child = ToElement(template_el->content()->firstChild());
  EXPECT_FALSE(child->isConnected());
  ASSERT_TRUE(child->getDisplayLockForBindings());

  // Try to lock an element in a template.
  auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
  {
    ScriptState::Scope scope(script_state);
    child->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);
  EXPECT_TRUE(child->getDisplayLockForBindings()->IsLocked());

  // commit() will unlock the element.
  {
    ScriptState::Scope scope(script_state);
    child->getDisplayLockForBindings()->commit(script_state);
  }
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(child->getDisplayLockForBindings()->IsLocked());

  // Try to lock an element that was moved from a template to a document.
  auto* document_child =
      ToElement(GetDocument().adoptNode(child, ASSERT_NO_EXCEPTION));
  GetDocument().body()->appendChild(document_child);

  {
    ScriptState::Scope scope(script_state);
    document_child->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 1);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 1);
  EXPECT_TRUE(document_child->getDisplayLockForBindings()->IsLocked());

  document_child->setAttribute("style", "color: red;");

  EXPECT_TRUE(document_child->NeedsStyleRecalc());

  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(document_child->NeedsStyleRecalc());

  // commit() will unlock the element and update the style.
  {
    ScriptState::Scope scope(script_state);
    document_child->getDisplayLockForBindings()->commit(script_state);
  }
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(document_child->getDisplayLockForBindings()->IsLocked());
  EXPECT_EQ(GetDocument().LockedDisplayLockCount(), 0);
  EXPECT_EQ(GetDocument().ActivationBlockingDisplayLockCount(), 0);

  EXPECT_FALSE(document_child->NeedsStyleRecalc());
  ASSERT_TRUE(document_child->GetComputedStyle());
  EXPECT_EQ(document_child->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()),
            MakeRGB(255, 0, 0));
}

}  // namespace blink
