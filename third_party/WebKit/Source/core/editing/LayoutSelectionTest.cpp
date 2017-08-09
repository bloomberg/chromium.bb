// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/LayoutSelection.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/ShadowRootInit.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutText.h"
#include "platform/wtf/Assertions.h"

namespace blink {

class LayoutSelectionTest : public EditingTestBase {
 public:
  // Test LayoutObject's type, SelectionState and ShouldInvalidateSelection
  // flag.
  bool TestNextLayoutObject(bool (LayoutObject::*predicate)(void) const,
                            SelectionState state) {
    if (!current_)
      current_ = GetDocument().body()->GetLayoutObject();
    else
      current_ = current_->NextInPreOrder();
    if (!current_)
      return false;

    if (current_->GetSelectionState() != state)
      return false;
    if (current_->ShouldInvalidateSelection() !=
        (state != SelectionState::kNone))
      return false;
    if (!(current_->*predicate)())
      return false;
    return true;
  }
  bool TestNextLayoutObject(const String& text, SelectionState state) {
    // TODO(yoichio): Share code using Function(see FunctionalTest.cpp).
    if (!current_)
      current_ = GetDocument().body()->GetLayoutObject();
    else
      current_ = current_->NextInPreOrder();
    if (!current_)
      return false;

    if (current_->GetSelectionState() != state)
      return false;
    if (current_->ShouldInvalidateSelection() !=
        (state != SelectionState::kNone))
      return false;

    if (!current_->IsText())
      return false;
    if (text != ToLayoutText(current_)->GetText())
      return false;
    return true;
  }
  LayoutObject* Current() { return current_; }
  void Reset() { current_ = nullptr; }
#ifndef NDEBUG
  void PrintLayoutTreeForDebug() {
    std::stringstream stream;
    for (LayoutObject* runner = GetDocument().body()->GetLayoutObject(); runner;
         runner = runner->NextInPreOrder()) {
      PrintLayoutObjectForSelection(stream, runner);
      stream << '\n';
    }
    LOG(INFO) << '\n' << stream.str();
  }
#endif

 private:
  LayoutObject* current_ = nullptr;
};

#define TEST_NEXT(predicate, state) \
  EXPECT_TRUE(TestNextLayoutObject(predicate, state)) << Current()

#define TEST_NO_NEXT_LAYOUT_OBJECT() \
  EXPECT_EQ(Current()->NextInPreOrder(), nullptr)

TEST_F(LayoutSelectionTest, TraverseLayoutObject) {
  SetBodyContent("foo<br>bar");
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SelectAllChildren(*GetDocument().body())
                               .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_NEXT(&LayoutObject::IsLayoutBlock, SelectionState::kStartAndEnd);
  TEST_NEXT("foo", SelectionState::kStart);
  TEST_NEXT(&LayoutObject::IsBR, SelectionState::kInside);
  TEST_NEXT("bar", SelectionState::kEnd);
  TEST_NO_NEXT_LAYOUT_OBJECT();
}

TEST_F(LayoutSelectionTest, TraverseLayoutObjectTruncateVisibilityHidden) {
  SetBodyContent(
      "<span style='visibility:hidden;'>before</span>"
      "foo"
      "<span style='visibility:hidden;'>after</span>");
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SelectAllChildren(*GetDocument().body())
                               .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_NEXT(&LayoutObject::IsLayoutBlock, SelectionState::kStartAndEnd);
  TEST_NEXT(&LayoutObject::IsLayoutInline, SelectionState::kNone);
  TEST_NEXT("before", SelectionState::kNone);
  TEST_NEXT("foo", SelectionState::kStartAndEnd);
  TEST_NEXT(&LayoutObject::IsLayoutInline, SelectionState::kNone);
  TEST_NEXT("after", SelectionState::kNone);
  TEST_NO_NEXT_LAYOUT_OBJECT();
}

TEST_F(LayoutSelectionTest, TraverseLayoutObjectTruncateBR) {
  SetBodyContent("<br><br>foo<br><br>");
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SelectAllChildren(*GetDocument().body())
                               .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_NEXT(&LayoutObject::IsLayoutBlock, SelectionState::kStartAndEnd);
  TEST_NEXT(&LayoutObject::IsBR, SelectionState::kStart);
  TEST_NEXT(&LayoutObject::IsBR, SelectionState::kInside);
  TEST_NEXT("foo", SelectionState::kInside);
  TEST_NEXT(&LayoutObject::IsBR, SelectionState::kEnd);
  TEST_NEXT(&LayoutObject::IsBR, SelectionState::kNone);
  TEST_NO_NEXT_LAYOUT_OBJECT();
}

TEST_F(LayoutSelectionTest, TraverseLayoutObjectListStyleImage) {
  SetBodyContent(
      "<style>ul {list-style-image:url(data:"
      "image/gif;base64,R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs=)}"
      "</style>"
      "<ul><li>foo<li>bar</ul>");
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SelectAllChildren(*GetDocument().body())
                               .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_NEXT(&LayoutObject::IsLayoutBlock, SelectionState::kStartAndEnd);
  TEST_NEXT(&LayoutObject::IsLayoutBlockFlow, SelectionState::kStartAndEnd);
  TEST_NEXT(&LayoutObject::IsListItem, SelectionState::kStart);
  TEST_NEXT(&LayoutObject::IsListMarker, SelectionState::kNone);
  TEST_NEXT("foo", SelectionState::kStart);
  TEST_NEXT(&LayoutObject::IsListItem, SelectionState::kEnd);
  TEST_NEXT(&LayoutObject::IsListMarker, SelectionState::kNone);
  TEST_NEXT("bar", SelectionState::kEnd);
  TEST_NO_NEXT_LAYOUT_OBJECT();
}

TEST_F(LayoutSelectionTest, TraverseLayoutObjectCrossingShadowBoundary) {
#ifndef NDEBUG
  PrintLayoutTreeForDebug();
#endif
  SetBodyContent(
      "foo<div id='host'>"
      "<span slot='s1'>bar1</span><span slot='s2'>bar2</span>"
      "</div>");
  // TODO(yoichio): Move NodeTest::AttachShadowTo to EditingTestBase and use
  // it.
  ShadowRootInit shadow_root_init;
  shadow_root_init.setMode("open");
  ShadowRoot* const shadow_root =
      GetDocument().QuerySelector("div")->attachShadow(
          ToScriptStateForMainWorld(GetDocument().GetFrame()), shadow_root_init,
          ASSERT_NO_EXCEPTION);
  shadow_root->setInnerHTML(
      "Foo<slot name='s2'></slot><slot name='s1'></slot>");

  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(GetDocument().body(), 0),
                            Position(GetDocument().QuerySelector("span"), 0))
          .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_NEXT(&LayoutObject::IsLayoutBlock, SelectionState::kStartAndEnd);
  TEST_NEXT(&LayoutObject::IsLayoutBlockFlow, SelectionState::kStart);
  TEST_NEXT("foo", SelectionState::kStart);
  TEST_NEXT(&LayoutObject::IsLayoutBlock, SelectionState::kEnd);
  TEST_NEXT("Foo", SelectionState::kInside);
  TEST_NEXT(&LayoutObject::IsLayoutInline, SelectionState::kNone);
  TEST_NEXT("bar2", SelectionState::kEnd);
  TEST_NEXT(&LayoutObject::IsLayoutInline, SelectionState::kNone);
  TEST_NEXT("bar1", SelectionState::kNone);
  TEST_NO_NEXT_LAYOUT_OBJECT();
}

// crbug.com/752715
TEST_F(LayoutSelectionTest,
       InvalidationShouldNotChangeRefferedLayoutObjectState) {
  SetBodyContent(
      "<div id='d1'>div1</div><div id='d2'>foo<span>bar</span>baz</div>");
  Node* span = GetDocument().QuerySelector("span");
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(span->firstChild(), 0),
                            Position(span->firstChild(), 3))
          .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_NEXT(&LayoutObject::IsLayoutBlock, SelectionState::kStartAndEnd);
  TEST_NEXT(&LayoutObject::IsLayoutBlockFlow, SelectionState::kNone);
  TEST_NEXT("div1", SelectionState::kNone);
  TEST_NEXT(&LayoutObject::IsLayoutBlockFlow, SelectionState::kStartAndEnd);
  TEST_NEXT("foo", SelectionState::kNone);
  TEST_NEXT(&LayoutObject::IsLayoutInline, SelectionState::kNone);
  TEST_NEXT("bar", SelectionState::kStartAndEnd);
  TEST_NEXT("baz", SelectionState::kNone);
  TEST_NO_NEXT_LAYOUT_OBJECT();

  Node* d1 = GetDocument().QuerySelector("#d1");
  Node* d2 = GetDocument().QuerySelector("#d2");
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(d1, 0), Position(d2, 0))
          .Build());
  // This commit should not crash.
  Selection().CommitAppearanceIfNeeded();
  Reset();
  TEST_NEXT(&LayoutObject::IsLayoutBlock, SelectionState::kEnd);
  TEST_NEXT(&LayoutObject::IsLayoutBlockFlow, SelectionState::kStart);
  TEST_NEXT("div1", SelectionState::kStart);
  TEST_NEXT(&LayoutObject::IsLayoutBlockFlow, SelectionState::kEnd);
  TEST_NEXT("foo", SelectionState::kNone);
  TEST_NEXT(&LayoutObject::IsLayoutInline, SelectionState::kNone);
  // TODO(yoichio) : Introduce enum class InvalidateOption and confirm
  // "bar" is SelectionState::kNone and ShouldInvalidation.
  // TEST_NEXT("bar", SelectionState::kNone);
  // TEST_NEXT("baz", SelectionState::kNone);
  // TEST_NO_NEXT_LAYOUT_OBJECT();
}

}  // namespace blink
