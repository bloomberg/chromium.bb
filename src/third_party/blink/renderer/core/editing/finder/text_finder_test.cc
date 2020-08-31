// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/finder/text_finder.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_list.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_in_page_coordinates.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using blink::test::RunPendingTasks;

namespace blink {

class TextFinderTest : public testing::Test {
 protected:
  TextFinderTest() {
    web_view_helper_.Initialize();
    WebLocalFrameImpl& frame_impl = *web_view_helper_.LocalMainFrame();
    frame_impl.ViewImpl()->MainFrameWidget()->Resize(WebSize(640, 480));
    frame_impl.ViewImpl()->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
    document_ = static_cast<Document*>(frame_impl.GetDocument());
    text_finder_ = &frame_impl.EnsureTextFinder();
  }

  Document& GetDocument() const;
  TextFinder& GetTextFinder() const;

  v8::Local<v8::Value> EvalJs(const std::string& script);

  static gfx::RectF FindInPageRect(Node* start_container,
                                   int start_offset,
                                   Node* end_container,
                                   int end_offset);

 private:
  frame_test_helpers::WebViewHelper web_view_helper_;
  Persistent<Document> document_;
  Persistent<TextFinder> text_finder_;
};

v8::Local<v8::Value> TextFinderTest::EvalJs(const std::string& script) {
  return GetDocument()
      .GetFrame()
      ->GetScriptController()
      .ExecuteScriptInMainWorldAndReturnValue(ScriptSourceCode(script.c_str()),
                                              KURL(),
                                              SanitizeScriptErrors::kSanitize);
}

Document& TextFinderTest::GetDocument() const {
  return *document_;
}

TextFinder& TextFinderTest::GetTextFinder() const {
  return *text_finder_;
}

gfx::RectF TextFinderTest::FindInPageRect(Node* start_container,
                                          int start_offset,
                                          Node* end_container,
                                          int end_offset) {
  const Position start_position(start_container, start_offset);
  const Position end_position(end_container, end_offset);
  const EphemeralRange range(start_position, end_position);
  return gfx::RectF(FindInPageRectFromRange(range));
}

TEST_F(TextFinderTest, FindTextSimple) {
  GetDocument().body()->setInnerHTML("XXXXFindMeYYYYfindmeZZZZ");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Node* text_node = GetDocument().body()->firstChild();

  int identifier = 0;
  WebString search_text(String("FindMe"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  bool wrap_within_frame = true;

  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  Range* active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_node, active_match->startContainer());
  EXPECT_EQ(4u, active_match->startOffset());
  EXPECT_EQ(text_node, active_match->endContainer());
  EXPECT_EQ(10u, active_match->endOffset());

  find_options->find_next = true;
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_node, active_match->startContainer());
  EXPECT_EQ(14u, active_match->startOffset());
  EXPECT_EQ(text_node, active_match->endContainer());
  EXPECT_EQ(20u, active_match->endOffset());

  // Should wrap to the first match.
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_node, active_match->startContainer());
  EXPECT_EQ(4u, active_match->startOffset());
  EXPECT_EQ(text_node, active_match->endContainer());
  EXPECT_EQ(10u, active_match->endOffset());

  // Search in the reverse order.
  identifier = 1;
  find_options = mojom::blink::FindOptions::New();
  find_options->forward = false;

  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_node, active_match->startContainer());
  EXPECT_EQ(14u, active_match->startOffset());
  EXPECT_EQ(text_node, active_match->endContainer());
  EXPECT_EQ(20u, active_match->endOffset());

  find_options->find_next = true;
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_node, active_match->startContainer());
  EXPECT_EQ(4u, active_match->startOffset());
  EXPECT_EQ(text_node, active_match->endContainer());
  EXPECT_EQ(10u, active_match->endOffset());

  // Wrap to the first match (last occurence in the document).
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_node, active_match->startContainer());
  EXPECT_EQ(14u, active_match->startOffset());
  EXPECT_EQ(text_node, active_match->endContainer());
  EXPECT_EQ(20u, active_match->endOffset());
}

TEST_F(TextFinderTest, FindTextAutosizing) {
  GetDocument().body()->setInnerHTML("XXXXFindMeYYYYfindmeZZZZ");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  WebString search_text(String("FindMe"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  bool wrap_within_frame = true;

  // Set viewport scale to 20 in order to simulate zoom-in
  GetDocument().GetPage()->SetDefaultPageScaleLimits(1, 20);
  GetDocument().GetPage()->SetPageScaleFactor(20);
  VisualViewport& visual_viewport =
      GetDocument().GetPage()->GetVisualViewport();

  // Enforce autosizing
  GetDocument().GetSettings()->SetTextAutosizingEnabled(true);
  GetDocument().GetSettings()->SetTextAutosizingWindowSizeOverride(
      IntSize(20, 20));
  GetDocument().GetTextAutosizer()->UpdatePageInfo();
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // In case of autosizing, scale _should_ change
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  ASSERT_TRUE(GetTextFinder().ActiveMatch());
  ASSERT_EQ(1, visual_viewport.Scale());  // in this case to 1

  // Disable autosizing and reset scale to 20
  visual_viewport.SetScale(20);
  GetDocument().GetSettings()->SetTextAutosizingEnabled(false);
  GetDocument().GetTextAutosizer()->UpdatePageInfo();
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  ASSERT_TRUE(GetTextFinder().ActiveMatch());
  ASSERT_EQ(20, visual_viewport.Scale());
}

TEST_F(TextFinderTest, FindTextNotFound) {
  GetDocument().body()->setInnerHTML("XXXXFindMeYYYYfindmeZZZZ");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  WebString search_text(String("Boo"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  bool wrap_within_frame = true;

  EXPECT_FALSE(GetTextFinder().Find(identifier, search_text, *find_options,
                                    wrap_within_frame));
  EXPECT_FALSE(GetTextFinder().ActiveMatch());
}

TEST_F(TextFinderTest, FindTextInShadowDOM) {
  GetDocument().body()->setInnerHTML("<b>FOO</b><i>foo</i>");
  ShadowRoot& shadow_root =
      GetDocument().body()->CreateV0ShadowRootForTesting();
  shadow_root.setInnerHTML(
      "<content select=\"i\"></content><u>Foo</u><content></content>");
  Node* text_in_b_element = GetDocument().body()->firstChild()->firstChild();
  Node* text_in_i_element = GetDocument().body()->lastChild()->firstChild();
  Node* text_in_u_element = shadow_root.childNodes()->item(1)->firstChild();
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  WebString search_text(String("foo"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  bool wrap_within_frame = true;

  // TextIterator currently returns the matches in the flat treeorder, so
  // in this case the matches will be returned in the order of
  // <i> -> <u> -> <b>.
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  Range* active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_in_i_element, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(text_in_i_element, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  find_options->find_next = true;
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_in_u_element, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(text_in_u_element, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_in_b_element, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(text_in_b_element, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  // Should wrap to the first match.
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_in_i_element, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(text_in_i_element, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  // Fresh search in the reverse order.
  identifier = 1;
  find_options = mojom::blink::FindOptions::New();
  find_options->forward = false;

  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_in_b_element, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(text_in_b_element, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  find_options->find_next = true;
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_in_u_element, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(text_in_u_element, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_in_i_element, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(text_in_i_element, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());

  // And wrap.
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame));
  active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_EQ(text_in_b_element, active_match->startContainer());
  EXPECT_EQ(0u, active_match->startOffset());
  EXPECT_EQ(text_in_b_element, active_match->endContainer());
  EXPECT_EQ(3u, active_match->endOffset());
}

TEST_F(TextFinderTest, ScopeTextMatchesSimple) {
  GetDocument().body()->setInnerHTML("XXXXFindMeYYYYfindmeZZZZ");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* text_node = GetDocument().body()->firstChild();

  int identifier = 0;
  WebString search_text(String("FindMe"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  EXPECT_EQ(2, GetTextFinder().TotalMatchCount());
  WebVector<gfx::RectF> match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(2u, match_rects.size());
  EXPECT_EQ(FindInPageRect(text_node, 4, text_node, 10), match_rects[0]);
  EXPECT_EQ(FindInPageRect(text_node, 14, text_node, 20), match_rects[1]);

  // Modify the document size and ensure the cached match rects are recomputed
  // to reflect the updated layout.
  GetDocument().body()->setAttribute(html_names::kStyleAttr, "margin: 2000px");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(2u, match_rects.size());
  EXPECT_EQ(FindInPageRect(text_node, 4, text_node, 10), match_rects[0]);
  EXPECT_EQ(FindInPageRect(text_node, 14, text_node, 20), match_rects[1]);
}

TEST_F(TextFinderTest, ScopeTextMatchesRepeated) {
  GetDocument().body()->setInnerHTML("XXXXFindMeYYYYfindmeZZZZ");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* text_node = GetDocument().body()->firstChild();

  int identifier = 0;
  WebString search_text1(String("XFindMe"));
  WebString search_text2(String("FindMe"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text1,
                                            *find_options);
  GetTextFinder().StartScopingStringMatches(identifier, search_text2,
                                            *find_options);

  // Only searchText2 should be highlighted.
  EXPECT_EQ(2, GetTextFinder().TotalMatchCount());
  WebVector<gfx::RectF> match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(2u, match_rects.size());
  EXPECT_EQ(FindInPageRect(text_node, 4, text_node, 10), match_rects[0]);
  EXPECT_EQ(FindInPageRect(text_node, 14, text_node, 20), match_rects[1]);
}

TEST_F(TextFinderTest, ScopeTextMatchesWithShadowDOM) {
  GetDocument().body()->setInnerHTML("<b>FOO</b><i>foo</i>");
  ShadowRoot& shadow_root =
      GetDocument().body()->CreateV0ShadowRootForTesting();
  shadow_root.setInnerHTML(
      "<content select=\"i\"></content><u>Foo</u><content></content>");
  Node* text_in_b_element = GetDocument().body()->firstChild()->firstChild();
  Node* text_in_i_element = GetDocument().body()->lastChild()->firstChild();
  Node* text_in_u_element = shadow_root.childNodes()->item(1)->firstChild();
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  WebString search_text(String("fOO"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  // TextIterator currently returns the matches in the flat tree order,
  // so in this case the matches will be returned in the order of
  // <i> -> <u> -> <b>.
  EXPECT_EQ(3, GetTextFinder().TotalMatchCount());
  WebVector<gfx::RectF> match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(3u, match_rects.size());
  EXPECT_EQ(FindInPageRect(text_in_i_element, 0, text_in_i_element, 3),
            match_rects[0]);
  EXPECT_EQ(FindInPageRect(text_in_u_element, 0, text_in_u_element, 3),
            match_rects[1]);
  EXPECT_EQ(FindInPageRect(text_in_b_element, 0, text_in_b_element, 3),
            match_rects[2]);
}

TEST_F(TextFinderTest, ScopeRepeatPatternTextMatches) {
  GetDocument().body()->setInnerHTML("ab ab ab ab ab");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* text_node = GetDocument().body()->firstChild();

  int identifier = 0;
  WebString search_text(String("ab ab"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  EXPECT_EQ(2, GetTextFinder().TotalMatchCount());
  WebVector<gfx::RectF> match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(2u, match_rects.size());
  EXPECT_EQ(FindInPageRect(text_node, 0, text_node, 5), match_rects[0]);
  EXPECT_EQ(FindInPageRect(text_node, 6, text_node, 11), match_rects[1]);
}

TEST_F(TextFinderTest, OverlappingMatches) {
  GetDocument().body()->setInnerHTML("aababaa");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* text_node = GetDocument().body()->firstChild();

  int identifier = 0;
  WebString search_text(String("aba"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  // We shouldn't find overlapped matches.
  EXPECT_EQ(1, GetTextFinder().TotalMatchCount());
  WebVector<gfx::RectF> match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(1u, match_rects.size());
  EXPECT_EQ(FindInPageRect(text_node, 1, text_node, 4), match_rects[0]);
}

TEST_F(TextFinderTest, SequentialMatches) {
  GetDocument().body()->setInnerHTML("ababab");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Node* text_node = GetDocument().body()->firstChild();

  int identifier = 0;
  WebString search_text(String("ab"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  EXPECT_EQ(3, GetTextFinder().TotalMatchCount());
  WebVector<gfx::RectF> match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(3u, match_rects.size());
  EXPECT_EQ(FindInPageRect(text_node, 0, text_node, 2), match_rects[0]);
  EXPECT_EQ(FindInPageRect(text_node, 2, text_node, 4), match_rects[1]);
  EXPECT_EQ(FindInPageRect(text_node, 4, text_node, 6), match_rects[2]);
}

TEST_F(TextFinderTest, FindTextJavaScriptUpdatesDOM) {
  GetDocument().body()->setInnerHTML("<b>XXXXFindMeYYYY</b><i></i>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  WebString search_text(String("FindMe"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;
  bool wrap_within_frame = true;
  bool active_now;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  find_options->find_next = true;
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame, &active_now));
  EXPECT_TRUE(active_now);
  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame, &active_now));
  EXPECT_TRUE(active_now);

  // Add new text to DOM and try FindNext.
  auto* i_element = To<Element>(GetDocument().body()->lastChild());
  ASSERT_TRUE(i_element);
  i_element->setInnerHTML("ZZFindMe");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame, &active_now));
  Range* active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_FALSE(active_now);
  EXPECT_EQ(2u, active_match->startOffset());
  EXPECT_EQ(8u, active_match->endOffset());

  // Restart full search and check that added text is found.
  find_options->find_next = false;
  GetTextFinder().ResetMatchCount();
  GetTextFinder().CancelPendingScopingEffort();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  EXPECT_EQ(2, GetTextFinder().TotalMatchCount());

  WebVector<gfx::RectF> match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(2u, match_rects.size());
  Node* text_in_b_element = GetDocument().body()->firstChild()->firstChild();
  Node* text_in_i_element = GetDocument().body()->lastChild()->firstChild();
  EXPECT_EQ(FindInPageRect(text_in_b_element, 4, text_in_b_element, 10),
            match_rects[0]);
  EXPECT_EQ(FindInPageRect(text_in_i_element, 2, text_in_i_element, 8),
            match_rects[1]);
}

TEST_F(TextFinderTest, FindTextJavaScriptUpdatesDOMAfterNoMatches) {
  GetDocument().body()->setInnerHTML("<b>XXXXYYYY</b><i></i>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  WebString search_text(String("FindMe"));
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;
  bool wrap_within_frame = true;
  bool active_now = false;

  GetTextFinder().ResetMatchCount();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  find_options->find_next = true;
  ASSERT_FALSE(GetTextFinder().Find(identifier, search_text, *find_options,
                                    wrap_within_frame, &active_now));
  EXPECT_FALSE(active_now);

  // Add new text to DOM and try FindNext.
  auto* i_element = To<Element>(GetDocument().body()->lastChild());
  ASSERT_TRUE(i_element);
  i_element->setInnerHTML("ZZFindMe");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  ASSERT_TRUE(GetTextFinder().Find(identifier, search_text, *find_options,
                                   wrap_within_frame, &active_now));
  Range* active_match = GetTextFinder().ActiveMatch();
  ASSERT_TRUE(active_match);
  EXPECT_FALSE(active_now);
  EXPECT_EQ(2u, active_match->startOffset());
  EXPECT_EQ(8u, active_match->endOffset());

  // Restart full search and check that added text is found.
  find_options->find_next = false;
  GetTextFinder().ResetMatchCount();
  GetTextFinder().CancelPendingScopingEffort();
  GetTextFinder().StartScopingStringMatches(identifier, search_text,
                                            *find_options);

  EXPECT_EQ(1, GetTextFinder().TotalMatchCount());

  WebVector<gfx::RectF> match_rects = GetTextFinder().FindMatchRects();
  ASSERT_EQ(1u, match_rects.size());
  Node* text_in_i_element = GetDocument().body()->lastChild()->firstChild();
  EXPECT_EQ(FindInPageRect(text_in_i_element, 2, text_in_i_element, 8),
            match_rects[0]);
}

TEST_F(TextFinderTest, ScopeWithTimeouts) {
  // Make a long string.
  String search_pattern("abc");
  StringBuilder text;
  // Make 4 substrings "abc" in text.
  for (int i = 0; i < 100; ++i) {
    if (i == 1 || i == 10 || i == 50 || i == 90) {
      text.Append(search_pattern);
    } else {
      text.Append('a');
    }
  }

  GetDocument().body()->setInnerHTML(text.ToString());
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  int identifier = 0;
  auto find_options =
      mojom::blink::FindOptions::New();  // Default + add testing flag.
  find_options->run_synchronously_for_testing = true;

  GetTextFinder().ResetMatchCount();

  // There will be only one iteration before timeout, because increment
  // of the TimeProxyPlatform timer is greater than timeout threshold.
  GetTextFinder().StartScopingStringMatches(identifier, search_pattern,
                                            *find_options);

  EXPECT_EQ(4, GetTextFinder().TotalMatchCount());
}

TEST_F(TextFinderTest, BeforeMatchEvent) {
  V8TestingScope v8_testing_scope;

  EvalJs(R"(
      const spacer = document.createElement('div');
      spacer.style.height = '2000px';
      document.body.appendChild(spacer);

      const foo = document.createElement('div');
      foo.textContent = 'foo';
      document.body.appendChild(foo);
      window.beforematchFiredOnFoo = false;
      foo.addEventListener('beforematch', () => {
        window.beforematchFiredOnFoo = true;
      });

      const bar = document.createElement('div');
      bar.textContent = 'bar';
      document.body.appendChild(bar);
      window.beforematchFiredOnBar = false;
      bar.addEventListener('beforematch', () => {
        window.YOffsetOnBeforematch = window.pageYOffset;
        window.beforematchFiredOnBar = true;
      });
      )");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  auto find_options = mojom::blink::FindOptions::New();
  find_options->run_synchronously_for_testing = true;
  GetTextFinder().Find(/*identifier=*/0, WebString(String("bar")),
                       *find_options, /*wrap_within_frame=*/false);

  v8::Local<v8::Value> beforematch_fired_on_foo =
      EvalJs("window.beforematchFiredOnFoo");
  ASSERT_TRUE(beforematch_fired_on_foo->IsBoolean());
  EXPECT_FALSE(
      beforematch_fired_on_foo->ToBoolean(v8::Isolate::GetCurrent())->Value());

  v8::Local<v8::Value> beforematch_fired_on_bar =
      EvalJs("window.beforematchFiredOnBar");
  ASSERT_TRUE(beforematch_fired_on_bar->IsBoolean());
  EXPECT_TRUE(
      beforematch_fired_on_bar->ToBoolean(v8::Isolate::GetCurrent())->Value());

  // Scrolling should occur after the beforematch event.
  v8::Local<v8::Context> context =
      v8_testing_scope.GetScriptState()->GetContext();
  v8::Local<v8::Value> beforematch_y_offset =
      EvalJs("window.YOffsetOnBeforematch");
  ASSERT_TRUE(beforematch_y_offset->IsNumber());
  EXPECT_TRUE(
      beforematch_y_offset->ToNumber(context).ToLocalChecked()->Value() == 0);
}

TEST_F(TextFinderTest, BeforeMatchEventRemoveElement) {
  V8TestingScope v8_testing_scope;

  EvalJs(R"(
      const spacer = document.createElement('div');
      spacer.style.height = '2000px';
      document.body.appendChild(spacer);

      const foo = document.createElement('div');
      foo.textContent = 'foo';
      document.body.appendChild(foo);
      window.beforematchFiredOnFoo = false;
      foo.addEventListener('beforematch', () => {
        foo.remove();
        window.beforematchFiredOnFoo = true;
      });
      )");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  auto find_options = mojom::blink::FindOptions::New();
  find_options->run_synchronously_for_testing = true;
  GetTextFinder().Find(/*identifier=*/0, WebString(String("foo")),
                       *find_options, /*wrap_within_frame=*/false);

  v8::Local<v8::Value> beforematch_fired_on_foo =
      EvalJs("window.beforematchFiredOnFoo");
  ASSERT_TRUE(beforematch_fired_on_foo->IsBoolean());
  EXPECT_TRUE(
      beforematch_fired_on_foo->ToBoolean(v8::Isolate::GetCurrent())->Value());

  // TODO(jarhar): Update this test to include checks for scrolling behavior
  // once we decide what the behavior should be. Right now it is just here to
  // make sure we avoid a renderer crash due to the detached element.
}

// TODO(jarhar): Write more tests here once we decide on a behavior here:
// https://github.com/WICG/display-locking/issues/150

}  // namespace blink
