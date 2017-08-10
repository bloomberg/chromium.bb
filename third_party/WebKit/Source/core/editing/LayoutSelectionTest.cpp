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
  LayoutObject* Current() const { return current_; }
  void Next() {
    if (!current_) {
      current_ = GetDocument().body()->GetLayoutObject();
      return;
    }
    current_ = current_->NextInPreOrder();
  }

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

std::ostream& operator<<(std::ostream& ostream, LayoutObject* layout_object) {
  PrintLayoutObjectForSelection(ostream, layout_object);
  return ostream;
}

enum class InvalidateOption { ShouldInvalidate, NotInvalidate };

static bool TestLayoutObjectState(LayoutObject* object,
                                  SelectionState state,
                                  InvalidateOption invalidate) {
  if (!object)
    return false;
  if (object->GetSelectionState() != state)
    return false;
  if (object->ShouldInvalidateSelection() !=
      (invalidate == InvalidateOption::ShouldInvalidate))
    return false;
  return true;
}

using IsTypeOf = bool (LayoutObject::*)(void) const;
static IsTypeOf IsLayoutBlock = &LayoutObject::IsLayoutBlock;
static IsTypeOf IsLayoutBlockFlow = &LayoutObject::IsLayoutBlockFlow;
static IsTypeOf IsLayoutInline = &LayoutObject::IsLayoutInline;
static IsTypeOf IsBR = &LayoutObject::IsBR;
static IsTypeOf IsListItem = &LayoutObject::IsListItem;
static IsTypeOf IsListMarker = &LayoutObject::IsListMarker;

static bool TestLayoutObject(LayoutObject* object,
                             IsTypeOf predicate,
                             SelectionState state,
                             InvalidateOption invalidate) {
  if (!TestLayoutObjectState(object, state, invalidate))
    return false;

  if (!(object->*predicate)())
    return false;
  return true;
}
static bool TestLayoutObject(LayoutObject* object,
                             const String& text,
                             SelectionState state,
                             InvalidateOption invalidate) {
  if (!TestLayoutObjectState(object, state, invalidate))
    return false;

  if (!object->IsText())
    return false;
  if (text != ToLayoutText(object)->GetText())
    return false;
  return true;
}

#define TEST_NEXT(predicate, state, invalidate)                             \
  Next();                                                                   \
  EXPECT_TRUE(TestLayoutObject(Current(), predicate, SelectionState::state, \
                               InvalidateOption::invalidate))               \
      << Current();

#define TEST_NO_NEXT_LAYOUT_OBJECT() \
  Next();                            \
  EXPECT_EQ(Current(), nullptr)

TEST_F(LayoutSelectionTest, TraverseLayoutObject) {
  SetBodyContent("foo<br>bar");
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SelectAllChildren(*GetDocument().body())
                               .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_NEXT(IsLayoutBlock, kStartAndEnd, ShouldInvalidate);
  TEST_NEXT("foo", kStart, ShouldInvalidate);
  TEST_NEXT(IsBR, kInside, ShouldInvalidate);
  TEST_NEXT("bar", kEnd, ShouldInvalidate);
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
  TEST_NEXT(IsLayoutBlock, kStartAndEnd, ShouldInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("before", kNone, NotInvalidate);
  TEST_NEXT("foo", kStartAndEnd, ShouldInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("after", kNone, NotInvalidate);
  TEST_NO_NEXT_LAYOUT_OBJECT();
}

TEST_F(LayoutSelectionTest, TraverseLayoutObjectTruncateBR) {
  SetBodyContent("<br><br>foo<br><br>");
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SelectAllChildren(*GetDocument().body())
                               .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_NEXT(IsLayoutBlock, kStartAndEnd, ShouldInvalidate);
  TEST_NEXT(IsBR, kStart, ShouldInvalidate);
  TEST_NEXT(IsBR, kInside, ShouldInvalidate);
  TEST_NEXT("foo", kInside, ShouldInvalidate);
  TEST_NEXT(IsBR, kEnd, ShouldInvalidate);
  TEST_NEXT(IsBR, kNone, NotInvalidate);
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
  TEST_NEXT(IsLayoutBlock, kStartAndEnd, ShouldInvalidate);
  TEST_NEXT(IsLayoutBlockFlow, kStartAndEnd, ShouldInvalidate);
  TEST_NEXT(IsListItem, kStart, ShouldInvalidate);
  TEST_NEXT(IsListMarker, kNone, NotInvalidate);
  TEST_NEXT("foo", kStart, ShouldInvalidate);
  TEST_NEXT(IsListItem, kEnd, ShouldInvalidate);
  TEST_NEXT(IsListMarker, kNone, NotInvalidate);
  TEST_NEXT("bar", kEnd, ShouldInvalidate);
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
  TEST_NEXT(IsLayoutBlock, kStartAndEnd, ShouldInvalidate);
  TEST_NEXT(IsLayoutBlockFlow, kStart, ShouldInvalidate);
  TEST_NEXT("foo", kStart, ShouldInvalidate);
  TEST_NEXT(IsLayoutBlock, kEnd, ShouldInvalidate);
  TEST_NEXT("Foo", kInside, ShouldInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("bar2", kEnd, ShouldInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("bar1", kNone, NotInvalidate);
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
  TEST_NEXT(IsLayoutBlock, kStartAndEnd, ShouldInvalidate);
  TEST_NEXT(IsLayoutBlockFlow, kNone, NotInvalidate);
  TEST_NEXT("div1", kNone, NotInvalidate);
  TEST_NEXT(IsLayoutBlockFlow, kStartAndEnd, ShouldInvalidate);
  TEST_NEXT("foo", kNone, NotInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("bar", kStartAndEnd, ShouldInvalidate);
  TEST_NEXT("baz", kNone, NotInvalidate);
  TEST_NO_NEXT_LAYOUT_OBJECT();

  Node* d1 = GetDocument().QuerySelector("#d1");
  Node* d2 = GetDocument().QuerySelector("#d2");
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(d1, 0), Position(d2, 0))
          .Build());
  // This commit should not crash.
  Selection().CommitAppearanceIfNeeded();
  TEST_NEXT(IsLayoutBlock, kEnd, ShouldInvalidate);
  TEST_NEXT(IsLayoutBlockFlow, kStart, ShouldInvalidate);
  TEST_NEXT("div1", kStart, ShouldInvalidate);
  TEST_NEXT(IsLayoutBlockFlow, kEnd, ShouldInvalidate);
  TEST_NEXT("foo", kNone, NotInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("bar", kNone, ShouldInvalidate);
  TEST_NEXT("baz", kNone, NotInvalidate);
  TEST_NO_NEXT_LAYOUT_OBJECT();
}

}  // namespace blink
