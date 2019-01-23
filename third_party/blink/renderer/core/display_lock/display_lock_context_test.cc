// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/frame/find_in_page.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {
namespace {
class DisplayLockTestFindInPageClient : public mojom::blink::FindInPageClient {
 public:
  DisplayLockTestFindInPageClient()
      : find_results_are_ready_(false), count_(-1), binding_(this) {}

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
    find_results_are_ready_ =
        (final_update == mojom::blink::FindMatchUpdateType::kFinalUpdate);
  }

  bool FindResultsAreReady() const { return find_results_are_ready_; }
  int Count() const { return count_; }
  void Reset() {
    find_results_are_ready_ = false;
    count_ = -1;
  }

 private:
  bool find_results_are_ready_;
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

  auto* element = GetDocument().getElementById("container");
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  // We should be in pending acquire state, which means we would allow things
  // like style and layout but disallow paint.
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint());

  UpdateAllLifecyclePhasesForTest();

  // Sanity checks to ensure the element is locked.
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint());

  EXPECT_FALSE(element->GetDisplayLockContext()->IsSearchable());

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

  auto* element = GetDocument().getElementById("container");
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  // We should be in pending acquire state, which means we would allow things
  // like style and layout but disallow paint.
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint());

  UpdateAllLifecyclePhasesForTest();

  // Sanity checks to ensure the element is locked.
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint());

  EXPECT_FALSE(element->GetDisplayLockContext()->IsSearchable());

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

  find_in_page->Find(current_id++, "testing", find_options->Clone());
  EXPECT_FALSE(client.FindResultsAreReady());
  test::RunPendingTasks();
  EXPECT_TRUE(client.FindResultsAreReady());
  EXPECT_EQ(1, client.Count());
  client.Reset();
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

  auto* element = GetDocument().getElementById("container");
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  // We should be in pending acquire state, which means we would allow things
  // like style and layout but disallow paint.
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_TRUE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint());

  UpdateAllLifecyclePhasesForTest();

  // Sanity checks to ensure the element is locked.
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldStyle());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldLayout());
  EXPECT_FALSE(element->GetDisplayLockContext()->ShouldPaint());

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

  EXPECT_TRUE(GetDocument().getElementById("textfield")->IsKeyboardFocusable());
  EXPECT_TRUE(GetDocument().getElementById("textfield")->IsMouseFocusable());
  EXPECT_TRUE(GetDocument().getElementById("textfield")->IsFocusable());

  // Calling explicit focus() should focus the element
  GetDocument().getElementById("textfield")->focus();
  EXPECT_EQ(GetDocument().FocusedElement(),
            GetDocument().getElementById("textfield"));
}
}  // namespace blink
