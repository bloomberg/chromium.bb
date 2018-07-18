// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/layout_selection.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/shadow_root_init.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

static const LayoutTextFragment* ToFirstLetterOrNull(
    const LayoutObject& layout_object) {
  if (const LayoutTextFragment* fragment =
          ToLayoutTextFragmentOrNull(layout_object)) {
    if (fragment->IsRemainingTextLayoutObject())
      return nullptr;
    return fragment;
  }
  return nullptr;
}

static const Node* FindNextNodeOnDOMIncludingShadow(const Node& node) {
  if (const ShadowRoot* shadow_root = node.GetShadowRoot())
    return shadow_root;
  if (const Node* first_child = node.firstChild())
    return first_child;
  if (const Node* next_sibling = node.nextSibling())
    return next_sibling;
  for (const Node* parent = node.parentNode(); parent;) {
    if (parent->IsShadowRoot()) {
      Node* host = parent->ParentOrShadowHostNode();
      DCHECK_EQ(host->GetShadowRoot(), parent);
      if (const Node* first_child = host->firstChild())
        return first_child;
      parent = host;
      continue;
    }
    if (const Node* next_sibling = parent->nextSibling())
      return next_sibling;
    parent = parent->parentNode();
  }
  return nullptr;
}

static LayoutTextFragment* FirstLetterPartFor(
    const LayoutObject* layout_object) {
  if (!layout_object->IsText())
    return nullptr;
  if (!ToLayoutText(layout_object)->IsTextFragment())
    return nullptr;
  return ToLayoutTextFragment(const_cast<LayoutObject*>(
      AssociatedLayoutObjectOf(*layout_object->GetNode(), 0)));
}

static LayoutObject* NextLayoutObjectOnDOMTreeIncludeShadow(
    const LayoutObject& layout_object) {
  // If |layout_object| is a first letter part, return remaining part.
  if (const LayoutTextFragment* fragment = ToFirstLetterOrNull(layout_object)) {
    return fragment->AssociatedTextNode()->GetLayoutObject();
  }
  // Otherwise, find the first layouted object of following nodes.
  const Node* node = layout_object.GetNode();
  DCHECK(node);
  for (const Node* next = FindNextNodeOnDOMIncludingShadow(*node); next;
       next = FindNextNodeOnDOMIncludingShadow(*next)) {
    LayoutObject* layout_object = next->GetLayoutObject();
    if (!layout_object)
      continue;
    if (LayoutObject* first_letter = FirstLetterPartFor(layout_object))
      return first_letter;
    return layout_object;
  }
  return nullptr;
}

class LayoutSelectionTest : public EditingTestBase {
 protected:
  LayoutObject* Current() const { return current_; }
  void Reset() {
    results.clear();
    stream = std::stringstream();
    current_ = GetDocument().body()->GetLayoutObject();
  }

  void Next() {
    DCHECK(current_);
    current_ = NextLayoutObjectOnDOMTreeIncludeShadow(*current_);
  }
  void TestNext(bool result) {
    results.push_back(result);
    if (!result) {
      stream << "case " << results.size() - 1 << " failed.\n";
    }
    if (!current_) {
      stream << "cases are more than layout objects.\n";
      return;
    }
    Next();
  }

  bool CheckResult() {
    if (current_)
      stream << "cases are fewer than layout objects.";

    std::string errors = stream.str();
    if (errors.size()) {
      std::stringstream layout_tree;
      PrintLayoutTree(layout_tree);
      LOG(ERROR) << "\n" << errors << layout_tree.str();
    }
    return !errors.size();
  }

  WTF::Vector<bool> results;
  std::stringstream stream;

  void PrintLayoutTree(std::stringstream& stream) {
    for (LayoutObject* runner = GetDocument().body()->GetLayoutObject(); runner;
         runner = NextLayoutObjectOnDOMTreeIncludeShadow(*runner)) {
      stream << '\n';
      PrintLayoutObjectForSelection(stream, runner);
    }
  }

  void PrintText(std::ostream& ostream, const Text& text) {
    ostream << "'" << text.data().Utf8().data() << "'";
  }

  void PrintSelectionInfo(std::ostream& ostream, LayoutObject* layout_object) {
    if (!layout_object) {
      ostream << "<null>";
      return;
    }
    if (const LayoutTextFragment* fragment =
            ToLayoutTextFragmentOrNull(*layout_object)) {
      // TODO(yoichio): Treat LayoutQuote which may generate LayoutTextFragment
      // that's neither first-letter nor remaining text.
      if (fragment->IsRemainingTextLayoutObject())
        ostream << "remaining part of ";
      else
        ostream << "first-letter of ";
      PrintText(ostream, *fragment->AssociatedTextNode());
    } else {
      if (Text* text = ToTextOrNull(layout_object->GetNode()))
        PrintText(ostream, *text);
      else
        ostream << layout_object->GetNode();
    }
    ostream << ", " << layout_object->GetSelectionState()
            << (layout_object->ShouldInvalidateSelection()
                    ? ", ShouldInvalidate "
                    : ", NotInvalidate ");
  }

  void PrintDOMTreeInternal(std::ostream& ostream,
                            const Node& node,
                            size_t depth) {
    ostream << RepeatString("  ", depth).Utf8().data();
    PrintSelectionInfo(ostream, node.GetLayoutObject());
    if (ShadowRoot* shadow_root = node.GetShadowRoot()) {
      ostream << std::endl << RepeatString("  ", depth + 1).Utf8().data();
      ostream << "#shadow-root";
      for (Node* child = shadow_root->firstChild(); child;
           child = child->nextSibling()) {
        PrintDOMTreeInternal(ostream, *child, depth + 2);
      }
    }

    for (Node* child = node.firstChild(); child; child = child->nextSibling()) {
      ostream << std::endl;
      PrintDOMTreeInternal(ostream, *child, depth + 1);
    }
  }

#ifndef NDEBUG
  void PrintDOMTreeForDebug() {
    std::stringstream stream;
    stream << "\nPrintDOMTreeForDebug";
    PrintDOMTreeInternal(stream, *GetDocument().body(), 0u);
    LOG(INFO) << stream.str();
  }
#endif

  std::string DumpSelectionInfo() {
    std::stringstream stream;
    PrintDOMTreeInternal(stream, *GetDocument().body(), 0u);
    return stream.str();
  }

 private:
  LayoutObject* current_ = nullptr;
};

// You can test SeletionStatus of all LayoutObjects in the body with following
// macros. See TraverseLayoutObject example below.
#define TEST_RESET() Reset();
#define TEST_NEXT(predicate, state, invalidate)                          \
  TestNext(TestLayoutObject(Current(), predicate, SelectionState::state, \
                            InvalidateOption::invalidate));
#define TEST_CHECK() EXPECT_TRUE(CheckResult())

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

using IsTypeOf =
    base::RepeatingCallback<bool(const LayoutObject& layout_object)>;
using IsTypeOfSimple = bool(const LayoutObject& layout_object);
#define USING_LAYOUTOBJECT_FUNC(member_func)                   \
  static bool member_func(const LayoutObject& layout_object) { \
    return layout_object.member_func();                        \
  }

USING_LAYOUTOBJECT_FUNC(IsLayoutBlock);
USING_LAYOUTOBJECT_FUNC(IsLayoutBlockFlow);
USING_LAYOUTOBJECT_FUNC(IsLayoutNGBlockFlow);
USING_LAYOUTOBJECT_FUNC(IsLayoutInline);
USING_LAYOUTOBJECT_FUNC(IsBR);
USING_LAYOUTOBJECT_FUNC(IsListItem);
USING_LAYOUTOBJECT_FUNC(IsLayoutImage);
USING_LAYOUTOBJECT_FUNC(IsLayoutButton);
USING_LAYOUTOBJECT_FUNC(IsRuby);
USING_LAYOUTOBJECT_FUNC(IsRubyText);
USING_LAYOUTOBJECT_FUNC(IsSVGRoot);
USING_LAYOUTOBJECT_FUNC(IsSVGText);
USING_LAYOUTOBJECT_FUNC(IsLayoutEmbeddedContent);

static IsTypeOf IsLayoutTextFragmentOf(const String& text) {
  return WTF::BindRepeating(
      [](const String& text, const LayoutObject& object) {
        if (!object.IsText())
          return false;
        if (text != ToLayoutText(object).GetText())
          return false;
        return ToLayoutText(object).IsTextFragment();
      },
      text);
}

static IsTypeOf IsLayoutOf(const String& text) {
  return WTF::BindRepeating(
      [](const String& text, const LayoutObject& object) {
        return ToElement(object.GetNode())->tagName() == text;
      },
      text);
}

static bool IsSVGTSpan(const LayoutObject& layout_object) {
  return layout_object.GetName() == String("LayoutSVGTSpan");
}

static bool IsLegacyBlockFlow(const LayoutObject& layout_object) {
  return layout_object.IsLayoutBlockFlow() && !layout_object.IsLayoutNGMixin();
}

static bool TestLayoutObject(LayoutObject* object,
                             IsTypeOfSimple& predicate,
                             SelectionState state,
                             InvalidateOption invalidate) {
  if (!TestLayoutObjectState(object, state, invalidate))
    return false;

  if (!predicate(*object))
    return false;
  return true;
}
static bool TestLayoutObject(LayoutObject* object,
                             const IsTypeOf& predicate,
                             SelectionState state,
                             InvalidateOption invalidate) {
  if (!TestLayoutObjectState(object, state, invalidate))
    return false;

  if (!predicate.Run(*object))
    return false;
  return true;
}
static bool TestLayoutObject(LayoutObject* object,
                             const String& text,
                             SelectionState state,
                             InvalidateOption invalidate) {
  return TestLayoutObject(
      object,
      WTF::BindRepeating(
          [](const String& text, const LayoutObject& object) {
            if (!object.IsText())
              return false;
            if (text != ToLayoutText(object).GetText())
              return false;
            return true;
          },
          text),
      state, invalidate);
}

TEST_F(LayoutSelectionTest, TraverseLayoutObject) {
  SetBodyContent("foo<br>bar");
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SelectAllChildren(*GetDocument().body())
          .Build());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', Start, ShouldInvalidate \n"
      "  BR, Inside, ShouldInvalidate \n"
      "  'bar', End, ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, TraverseLayoutObjectTruncateVisibilityHidden) {
  SetBodyContent(
      "<span style='visibility:hidden;'>before</span>"
      "foo"
      "<span style='visibility:hidden;'>after</span>");
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SelectAllChildren(*GetDocument().body())
          .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("before", kNone, NotInvalidate);
  TEST_NEXT("foo", kStartAndEnd, ShouldInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("after", kNone, NotInvalidate);
  TEST_CHECK();
}

TEST_F(LayoutSelectionTest, TraverseLayoutObjectBRs) {
  SetBodyContent("<br><br>foo<br><br>");
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SelectAllChildren(*GetDocument().body())
          .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsBR, kStart, ShouldInvalidate);
  TEST_NEXT(IsBR, kInside, ShouldInvalidate);
  TEST_NEXT("foo", kInside, ShouldInvalidate);
  TEST_NEXT(IsBR, kInside, ShouldInvalidate);
  TEST_NEXT(IsBR, kEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_FALSE(Selection().LayoutSelectionStart().has_value());
  EXPECT_FALSE(Selection().LayoutSelectionEnd().has_value());
}

TEST_F(LayoutSelectionTest, TraverseLayoutObjectListStyleImage) {
  SetBodyContent(
      "<style>ul {list-style-image:url(data:"
      "image/gif;base64,R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs=)}"
      "</style>"
      "<ul><li>foo<li>bar</ul>");
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SelectAllChildren(*GetDocument().body())
          .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsLayoutBlockFlow, kContain, NotInvalidate);
  TEST_NEXT(IsListItem, kContain, NotInvalidate);
  TEST_NEXT("foo", kStart, ShouldInvalidate);
  TEST_NEXT(IsListItem, kContain, NotInvalidate);
  TEST_NEXT("bar", kEnd, ShouldInvalidate);
  TEST_CHECK();
}

TEST_F(LayoutSelectionTest, TraverseLayoutObjectCrossingShadowBoundary) {
  Selection().SetSelectionAndEndTyping(SetSelectionTextToBody(
      "^foo"
      "<div>"
      "<template data-mode=open>"
      "Foo<slot name=s2></slot><slot name=s1></slot>"
      "</template>"
      // Set selection at SPAN@0 instead of "bar1"@0
      "<span slot=s1><!--|-->bar1</span><span slot=s2>bar2</span>"
      "</div>"));
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutOf("BODY"), kContain, NotInvalidate);
  TEST_NEXT("foo", kStart, ShouldInvalidate);
  TEST_NEXT(IsLayoutOf("DIV"), kContain, NotInvalidate);
  TEST_NEXT("Foo", kInside, ShouldInvalidate);
  TEST_NEXT(IsLayoutOf("SPAN"), kNone, NotInvalidate);
  TEST_NEXT("bar1", kNone, NotInvalidate);
  TEST_NEXT(IsLayoutOf("SPAN"), kNone, NotInvalidate);
  TEST_NEXT("bar2", kEnd, ShouldInvalidate);
  TEST_CHECK();
}

// crbug.com/752715
TEST_F(LayoutSelectionTest,
       InvalidationShouldNotChangeRefferedLayoutObjectState) {
  SetBodyContent(
      "<div id='d1'>div1</div><div id='d2'>foo<span>bar</span>baz</div>");
  Node* span = GetDocument().QuerySelector("span");
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(span->firstChild(), 0),
                            Position(span->firstChild(), 3))
          .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsLayoutBlockFlow, kNone, NotInvalidate);
  TEST_NEXT("div1", kNone, NotInvalidate);
  TEST_NEXT(IsLayoutBlockFlow, kContain, NotInvalidate);
  TEST_NEXT("foo", kNone, NotInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("bar", kStartAndEnd, ShouldInvalidate);
  TEST_NEXT("baz", kNone, NotInvalidate);
  TEST_CHECK();

  Node* d1 = GetDocument().QuerySelector("#d1");
  Node* d2 = GetDocument().QuerySelector("#d2");
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(d1, 0), Position(d2, 0))
          .Build());
  // This commit should not crash.
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsLayoutBlockFlow, kContain, NotInvalidate);
  TEST_NEXT("div1", kStartAndEnd, ShouldInvalidate);
  TEST_NEXT(IsLayoutBlockFlow, kNone, NotInvalidate);
  TEST_NEXT("foo", kNone, NotInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("bar", kNone, ShouldInvalidate);
  TEST_NEXT("baz", kNone, NotInvalidate);
  TEST_CHECK();
}

TEST_F(LayoutSelectionTest, TraverseLayoutObjectLineWrap) {
  SetBodyContent("bar\n");
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SelectAllChildren(*GetDocument().body())
          .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("bar\n", kStartAndEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_EQ(0u, Selection().LayoutSelectionStart());
  EXPECT_EQ(4u, Selection().LayoutSelectionEnd());
}

TEST_F(LayoutSelectionTest, FirstLetter) {
  SetBodyContent(
      "<style>::first-letter { color: red; }</style>"
      "<span>foo</span>");
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SelectAllChildren(*GetDocument().body())
          .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT(IsLayoutTextFragmentOf("f"), kStart, ShouldInvalidate);
  TEST_NEXT(IsLayoutTextFragmentOf("oo"), kEnd, ShouldInvalidate);
  TEST_CHECK();
}

TEST_F(LayoutSelectionTest, FirstLetterClearSeletion) {
  InsertStyleElement("div::first-letter { color: red; }");
  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("fo^o<div>bar</div>b|az"));
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("foo", kStart, ShouldInvalidate);
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsLayoutTextFragmentOf("b"), kInside, ShouldInvalidate);
  TEST_NEXT(IsLayoutTextFragmentOf("ar"), kInside, ShouldInvalidate);
  TEST_NEXT("baz", kEnd, ShouldInvalidate);
  TEST_CHECK();

  Selection().ClearLayoutSelection();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kNone, NotInvalidate);
  TEST_NEXT("foo", kNone, ShouldInvalidate);
  TEST_NEXT(IsLayoutBlock, kNone, NotInvalidate);
  TEST_NEXT(IsLayoutTextFragmentOf("b"), kNone, ShouldInvalidate);
  TEST_NEXT(IsLayoutTextFragmentOf("ar"), kNone, ShouldInvalidate);
  TEST_NEXT("baz", kNone, ShouldInvalidate);
  TEST_CHECK();
}

TEST_F(LayoutSelectionTest, FirstLetterUpdateSeletion) {
  SetBodyContent(
      "<style>div::first-letter { color: red; }</style>"
      "foo<div>bar</div>baz");
  Node* const foo = GetDocument().body()->firstChild()->nextSibling();
  Node* const baz = GetDocument()
                        .body()
                        ->firstChild()
                        ->nextSibling()
                        ->nextSibling()
                        ->nextSibling();
  // <div>fo^o</div><div>bar</div>b|az
  Selection().SetSelectionAndEndTyping(SelectionInDOMTree::Builder()
                                           .SetBaseAndExtent({foo, 2}, {baz, 1})
                                           .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("foo", kStart, ShouldInvalidate);
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsLayoutTextFragmentOf("b"), kInside, ShouldInvalidate);
  TEST_NEXT(IsLayoutTextFragmentOf("ar"), kInside, ShouldInvalidate);
  TEST_NEXT("baz", kEnd, ShouldInvalidate);
  TEST_CHECK();
  // <div>foo</div><div>bar</div>ba^z|
  Selection().SetSelectionAndEndTyping(SelectionInDOMTree::Builder()
                                           .SetBaseAndExtent({baz, 2}, {baz, 3})
                                           .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("foo", kNone, ShouldInvalidate);
  // TODO(yoichio): Invalidating next LayoutBlock is flaky but it doesn't
  // matter in wild because we don't paint selection gap. I will update
  // invalidating propagation so this flakiness should be fixed as:
  // TEST_NEXT(IsLayoutBlock, kNone, NotInvalidate);
  Next();
  TEST_NEXT(IsLayoutTextFragmentOf("b"), kNone, ShouldInvalidate);
  TEST_NEXT(IsLayoutTextFragmentOf("ar"), kNone, ShouldInvalidate);
  TEST_NEXT("baz", kStartAndEnd, ShouldInvalidate);
  TEST_CHECK();
}

TEST_F(LayoutSelectionTest, CommitAppearanceIfNeededNotCrash) {
  Selection().SetSelectionAndEndTyping(SetSelectionTextToBody(
      "<div>"
      "<template data-mode=open>foo</template>"
      "<span>|bar<span>"  // <span> is not appeared in flat tree.
      "</div>"
      "<div>baz^</div>"));
  Selection().CommitAppearanceIfNeeded();
}

TEST_F(LayoutSelectionTest, SelectImage) {
  const SelectionInDOMTree& selection =
      SetSelectionTextToBody("^<img style=\"width:100px; height:100px\"/>|");
  Selection().SetSelectionAndEndTyping(selection);
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsLayoutImage, kStartAndEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_FALSE(Selection().LayoutSelectionStart().has_value());
  EXPECT_FALSE(Selection().LayoutSelectionEnd().has_value());
}

TEST_F(LayoutSelectionTest, MoveOnSameNode_Start) {
  const SelectionInDOMTree& selection =
      SetSelectionTextToBody("f^oo<span>b|ar</span>");
  Selection().SetSelectionAndEndTyping(selection);
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("foo", kStart, ShouldInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("bar", kEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_EQ(1u, Selection().LayoutSelectionStart());
  EXPECT_EQ(1u, Selection().LayoutSelectionEnd());

  // Paint virtually and clear ShouldInvalidate flag.
  UpdateAllLifecyclePhases();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("foo", kStart, NotInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("bar", kEnd, NotInvalidate);
  TEST_CHECK();

  // "fo^o<span>b|ar</span>"
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent({selection.Base().AnchorNode(), 2},
                            selection.Extent())
          .Build());
  Selection().CommitAppearanceIfNeeded();
  // Only "foo" should be invalidated.
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("foo", kStart, ShouldInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("bar", kEnd, NotInvalidate);
  TEST_CHECK();
  EXPECT_EQ(2u, Selection().LayoutSelectionStart());
  EXPECT_EQ(1u, Selection().LayoutSelectionEnd());
}

TEST_F(LayoutSelectionTest, MoveOnSameNode_End) {
  const SelectionInDOMTree& selection =
      SetSelectionTextToBody("f^oo<span>b|ar</span>");
  Selection().SetSelectionAndEndTyping(selection);
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("foo", kStart, ShouldInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("bar", kEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_EQ(1u, Selection().LayoutSelectionStart());
  EXPECT_EQ(1u, Selection().LayoutSelectionEnd());

  // Paint virtually and clear ShouldInvalidate flag.
  UpdateAllLifecyclePhases();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("foo", kStart, NotInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("bar", kEnd, NotInvalidate);
  TEST_CHECK();

  // "fo^o<span>ba|r</span>"
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(selection.Base(),
                            {selection.Extent().AnchorNode(), 2})
          .Build());
  Selection().CommitAppearanceIfNeeded();
  // Only "bar" should be invalidated.
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("foo", kStart, NotInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("bar", kEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_EQ(1u, Selection().LayoutSelectionStart());
  EXPECT_EQ(2u, Selection().LayoutSelectionEnd());
}

TEST_F(LayoutSelectionTest, MoveOnSameNode_StartAndEnd) {
  const SelectionInDOMTree& selection = SetSelectionTextToBody("f^oob|ar");
  Selection().SetSelectionAndEndTyping(selection);
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("foobar", kStartAndEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_EQ(1u, Selection().LayoutSelectionStart());
  EXPECT_EQ(4u, Selection().LayoutSelectionEnd());

  // Paint virtually and clear ShouldInvalidate flag.
  UpdateAllLifecyclePhases();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("foobar", kStartAndEnd, NotInvalidate);
  TEST_CHECK();

  // "f^ooba|r"
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(selection.Base(),
                            {selection.Extent().AnchorNode(), 5})
          .Build());
  Selection().CommitAppearanceIfNeeded();
  // "foobar" should be invalidated.
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("foobar", kStartAndEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_EQ(1u, Selection().LayoutSelectionStart());
  EXPECT_EQ(5u, Selection().LayoutSelectionEnd());
}

TEST_F(LayoutSelectionTest, MoveOnSameNode_StartAndEnd_Collapse) {
  const SelectionInDOMTree& selection = SetSelectionTextToBody("f^oob|ar");
  Selection().SetSelectionAndEndTyping(selection);
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("foobar", kStartAndEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_EQ(1u, Selection().LayoutSelectionStart());
  EXPECT_EQ(4u, Selection().LayoutSelectionEnd());

  // Paint virtually and clear ShouldInvalidate flag.
  UpdateAllLifecyclePhases();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("foobar", kStartAndEnd, NotInvalidate);
  TEST_CHECK();

  // "foo^|bar"
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse({selection.Base().AnchorNode(), 3})
          .Build());
  Selection().CommitAppearanceIfNeeded();
  // "foobar" should be invalidated.
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kNone, NotInvalidate);
  TEST_NEXT("foobar", kNone, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_FALSE(Selection().LayoutSelectionStart().has_value());
  EXPECT_FALSE(Selection().LayoutSelectionEnd().has_value());
}

TEST_F(LayoutSelectionTest, ContentEditableButton) {
  SetBodyContent("<input type=button value=foo contenteditable>");
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SelectAllChildren(*GetDocument().body())
          .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsLayoutButton, kContain, NotInvalidate);
  TEST_NEXT("foo", kStartAndEnd, ShouldInvalidate);
  TEST_CHECK();
}

TEST_F(LayoutSelectionTest, ClearSelection) {
  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<div>f^o|o</div>"));
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("foo", kStartAndEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_EQ(1u, Selection().LayoutSelectionStart());
  EXPECT_EQ(2u, Selection().LayoutSelectionEnd());

  UpdateAllLifecyclePhases();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT("foo", kStartAndEnd, NotInvalidate);
  TEST_CHECK();

  Selection().ClearLayoutSelection();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kNone, NotInvalidate);
  TEST_NEXT(IsLayoutBlock, kNone, NotInvalidate);
  TEST_NEXT("foo", kNone, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_FALSE(Selection().LayoutSelectionStart().has_value());
  EXPECT_FALSE(Selection().LayoutSelectionEnd().has_value());
}

TEST_F(LayoutSelectionTest, SVG) {
  const SelectionInDOMTree& selection =
      SetSelectionTextToBody("<svg><text x=10 y=10>fo^o|bar</text></svg>");
  Selection().SetSelectionAndEndTyping(selection);
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsSVGRoot, kNone, NotInvalidate);
  // LayoutSVGText should be invalidate though it is kContain.
  TEST_NEXT(IsSVGText, kContain, ShouldInvalidate);
  TEST_NEXT("foobar", kStartAndEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_EQ(2u, Selection().LayoutSelectionStart());
  EXPECT_EQ(3u, Selection().LayoutSelectionEnd());

  UpdateAllLifecyclePhases();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsSVGRoot, kNone, NotInvalidate);
  TEST_NEXT(IsSVGText, kContain, NotInvalidate);
  TEST_NEXT("foobar", kStartAndEnd, NotInvalidate);
  TEST_CHECK();

  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(selection.Base(),
                            {selection.Extent().AnchorNode(), 4})
          .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsSVGRoot, kNone, NotInvalidate);
  TEST_NEXT(IsSVGText, kContain, ShouldInvalidate);
  TEST_NEXT("foobar", kStartAndEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_EQ(2u, Selection().LayoutSelectionStart());
  EXPECT_EQ(4u, Selection().LayoutSelectionEnd());
}

// crbug.com/781705
TEST_F(LayoutSelectionTest, SVGAncestor) {
  const SelectionInDOMTree& selection = SetSelectionTextToBody(
      "<svg><text x=10 y=10><tspan>fo^o|bar</tspan></text></svg>");
  Selection().SetSelectionAndEndTyping(selection);
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsSVGRoot, kNone, NotInvalidate);
  // LayoutSVGText should be invalidated.
  TEST_NEXT(IsSVGText, kContain, ShouldInvalidate);
  TEST_NEXT(IsSVGTSpan, kNone, NotInvalidate);
  TEST_NEXT("foobar", kStartAndEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_EQ(2u, Selection().LayoutSelectionStart());
  EXPECT_EQ(3u, Selection().LayoutSelectionEnd());

  UpdateAllLifecyclePhases();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsSVGRoot, kNone, NotInvalidate);
  TEST_NEXT(IsSVGText, kContain, NotInvalidate);
  TEST_NEXT(IsSVGTSpan, kNone, NotInvalidate);
  TEST_NEXT("foobar", kStartAndEnd, NotInvalidate);
  TEST_CHECK();

  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(selection.Base(),
                            {selection.Extent().AnchorNode(), 4})
          .Build());
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsSVGRoot, kNone, NotInvalidate);
  TEST_NEXT(IsSVGText, kContain, ShouldInvalidate);
  TEST_NEXT(IsSVGTSpan, kNone, NotInvalidate);
  TEST_NEXT("foobar", kStartAndEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_EQ(2u, Selection().LayoutSelectionStart());
  EXPECT_EQ(4u, Selection().LayoutSelectionEnd());
}

TEST_F(LayoutSelectionTest, Embed) {
  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("^<embed type=foobar></embed>|"));
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutBlock, kContain, NotInvalidate);
  TEST_NEXT(IsLayoutEmbeddedContent, kStartAndEnd, ShouldInvalidate);
  TEST_CHECK();
}

// http:/crbug.com/843144
TEST_F(LayoutSelectionTest, Ruby) {
  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("^<ruby>foo<rt>bar</rt></ruby>|"));
  Selection().CommitAppearanceIfNeeded();
  TEST_RESET();
  TEST_NEXT(IsLayoutOf("BODY"), kContain, NotInvalidate);
  TEST_NEXT(IsRuby, kNone, NotInvalidate);
  TEST_NEXT("foo", kStart, ShouldInvalidate);
  TEST_NEXT(IsRubyText, kContain, NotInvalidate);
  TEST_NEXT("bar", kEnd, ShouldInvalidate);
  TEST_CHECK();

  UpdateAllLifecyclePhases();
  TEST_RESET();
  TEST_NEXT(IsLayoutOf("BODY"), kContain, NotInvalidate);
  TEST_NEXT(IsRuby, kNone, NotInvalidate);
  TEST_NEXT("foo", kStart, NotInvalidate);
  TEST_NEXT(IsRubyText, kContain, NotInvalidate);
  TEST_NEXT("bar", kEnd, NotInvalidate);
  TEST_CHECK();

  Selection().ClearLayoutSelection();
  TEST_RESET();
  TEST_NEXT(IsLayoutOf("BODY"), kNone, NotInvalidate);
  TEST_NEXT(IsRuby, kNone, NotInvalidate);
  TEST_NEXT("foo", kNone, ShouldInvalidate);
  // TODO(yoichio): These 2 should be kNone.
  // TEST_NEXT(IsRubyText, kNone, NotInvalidate);
  // TEST_NEXT("bar", kNone, ShouldInvalidate);
  TEST_NEXT(IsRubyText, kContain, NotInvalidate);
  TEST_NEXT("bar", kEnd, NotInvalidate);
  TEST_CHECK();
}

static const NGPaintFragment* FindNGPaintFragmentInternal(
    const NGPaintFragment* paint,
    const LayoutObject* layout_object) {
  if (paint->GetLayoutObject() == layout_object)
    return paint;
  for (const auto& child : paint->Children()) {
    if (const NGPaintFragment* child_fragment =
            FindNGPaintFragmentInternal(child.get(), layout_object))
      return child_fragment;
  }
  return nullptr;
}

static const NGPaintFragment& GetNGPaintFragment(
    const LayoutObject* layout_object) {
  DCHECK(layout_object->IsText());
  LayoutBlockFlow* block_flow = layout_object->EnclosingNGBlockFlow();
  DCHECK(block_flow);
  DCHECK(block_flow->IsLayoutNGMixin());
  LayoutNGBlockFlow* layout_ng = ToLayoutNGBlockFlow(block_flow);
  const NGPaintFragment* paint_fragment =
      FindNGPaintFragmentInternal(layout_ng->PaintFragment(), layout_object);
  DCHECK(paint_fragment);
  return *paint_fragment;
}

class NGLayoutSelectionTest
    : public LayoutSelectionTest,
      private ScopedLayoutNGForTest,
      private ScopedPaintUnderInvalidationCheckingForTest {
 public:
  NGLayoutSelectionTest()
      : ScopedLayoutNGForTest(true),
        ScopedPaintUnderInvalidationCheckingForTest(true) {}

  const Text* GetFirstTextNode() {
    for (const Node& runner : NodeTraversal::StartsAt(*GetDocument().body())) {
      if (runner.IsTextNode())
        return &ToText(runner);
    }
    NOTREACHED();
    return nullptr;
  }

  bool IsFirstTextLineBreak(const std::string& selection_text) {
    SetSelectionAndUpdateLayoutSelection(selection_text);
    const Text* const first_text = GetFirstTextNode();
    const NGPaintFragment& fragment =
        GetNGPaintFragment(first_text->GetLayoutObject());
    const LayoutSelectionStatus& status =
        Selection().ComputeLayoutSelectionStatus(fragment);
    return status.line_break == SelectSoftLineBreak::kSelected;
  }

  LayoutSelectionStatus ComputeLayoutSelectionStatus(const Node& node) {
    return Selection().ComputeLayoutSelectionStatus(
        GetNGPaintFragment(node.GetLayoutObject()));
  }

  void SetSelectionAndUpdateLayoutSelection(const std::string& selection_text) {
    const SelectionInDOMTree& selection =
        SetSelectionTextToBody(selection_text);
    Selection().SetSelectionAndEndTyping(selection);
    Selection().CommitAppearanceIfNeeded();
  }
};

std::ostream& operator<<(std::ostream& ostream,
                         const LayoutSelectionStatus& status) {
  const String line_break =
      (status.line_break == SelectSoftLineBreak::kSelected) ? "kSelected"
                                                            : "kNotSelected";
  return ostream << status.start << ", " << status.end << ", " << std::boolalpha
                 << line_break;
}

TEST_F(NGLayoutSelectionTest, SelectOnOneText) {
  SetSelectionAndUpdateLayoutSelection("foo<span>b^a|r</span>");
  TEST_RESET();
  TEST_NEXT(IsLayoutNGBlockFlow, kContain, NotInvalidate);
  TEST_NEXT("foo", kNone, NotInvalidate);
  TEST_NEXT(IsLayoutInline, kNone, NotInvalidate);
  TEST_NEXT("bar", kStartAndEnd, ShouldInvalidate);
  TEST_CHECK();

  LayoutObject* const foo =
      GetDocument().body()->firstChild()->GetLayoutObject();
  EXPECT_EQ(LayoutSelectionStatus(0u, 0u, SelectSoftLineBreak::kNotSelected),
            Selection().ComputeLayoutSelectionStatus(GetNGPaintFragment(foo)));
  LayoutObject* const bar = GetDocument()
                                .body()
                                ->firstChild()
                                ->nextSibling()
                                ->firstChild()
                                ->GetLayoutObject();
  EXPECT_EQ(LayoutSelectionStatus(4u, 5u, SelectSoftLineBreak::kNotSelected),
            Selection().ComputeLayoutSelectionStatus(GetNGPaintFragment(bar)));
}

TEST_F(NGLayoutSelectionTest, FirstLetterInAnotherBlockFlow) {
  SetSelectionAndUpdateLayoutSelection(
      "<style>:first-letter { float: right}</style>^fo|o");
  TEST_RESET();
  TEST_NEXT(IsLayoutNGBlockFlow, kContain, NotInvalidate);
  TEST_NEXT(IsLayoutTextFragmentOf("f"), kStart, ShouldInvalidate);
  TEST_NEXT(IsLayoutTextFragmentOf("oo"), kEnd, ShouldInvalidate);
  TEST_CHECK();
  Node* const foo = GetDocument().body()->firstChild()->nextSibling();
  const LayoutTextFragment* const foo_f =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*foo, 0));
  EXPECT_EQ(
      LayoutSelectionStatus(0u, 1u, SelectSoftLineBreak::kSelected),
      Selection().ComputeLayoutSelectionStatus(GetNGPaintFragment(foo_f)));
  const LayoutTextFragment* const foo_oo =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*foo, 1));
  EXPECT_EQ(
      LayoutSelectionStatus(1u, 2u, SelectSoftLineBreak::kNotSelected),
      Selection().ComputeLayoutSelectionStatus(GetNGPaintFragment(foo_oo)));
}

TEST_F(NGLayoutSelectionTest, TwoNGBlockFlows) {
  SetSelectionAndUpdateLayoutSelection("<div>f^oo</div><div>ba|r</div>");
  TEST_RESET();
  TEST_NEXT(IsLayoutNGBlockFlow, kContain, NotInvalidate);
  TEST_NEXT(IsLayoutNGBlockFlow, kContain, NotInvalidate);
  TEST_NEXT("foo", kStart, ShouldInvalidate);
  TEST_NEXT(IsLayoutNGBlockFlow, kContain, NotInvalidate);
  TEST_NEXT("bar", kEnd, ShouldInvalidate);
  TEST_CHECK();
  LayoutObject* const foo =
      GetDocument().body()->firstChild()->firstChild()->GetLayoutObject();
  EXPECT_EQ(LayoutSelectionStatus(1u, 3u, SelectSoftLineBreak::kSelected),
            Selection().ComputeLayoutSelectionStatus(GetNGPaintFragment(foo)));
  LayoutObject* const bar = GetDocument()
                                .body()
                                ->firstChild()
                                ->nextSibling()
                                ->firstChild()
                                ->GetLayoutObject();
  EXPECT_EQ(LayoutSelectionStatus(0u, 2u, SelectSoftLineBreak::kNotSelected),
            Selection().ComputeLayoutSelectionStatus(GetNGPaintFragment(bar)));
}

// TODO(editing-dev): Once LayoutNG supports editing, we should change this
// test to use LayoutNG tree.
TEST_F(NGLayoutSelectionTest, MixedBlockFlowsAsSibling) {
  SetSelectionAndUpdateLayoutSelection(
      "<div>f^oo</div>"
      "<div contenteditable>ba|r</div>");
  TEST_RESET();
  TEST_NEXT(IsLayoutBlockFlow, kContain, NotInvalidate);
  TEST_NEXT(IsLayoutBlockFlow, kContain, NotInvalidate);
  TEST_NEXT("foo", kStart, ShouldInvalidate);
  TEST_NEXT(IsLayoutBlockFlow, kContain, NotInvalidate);
  TEST_NEXT("bar", kEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_EQ(1u, Selection().LayoutSelectionStart().value());
  EXPECT_EQ(2u, Selection().LayoutSelectionEnd().value());
}

// TODO(editing-dev): Once LayoutNG supports editing, we should change this
// test to use LayoutNG tree.
TEST_F(NGLayoutSelectionTest, MixedBlockFlowsAnscestor) {
  // Both "foo" and "bar" for DIV elements should be legacy LayoutBlock.
  SetSelectionAndUpdateLayoutSelection(
      "<div contenteditable>f^oo"
      "<div contenteditable=false>ba|r</div></div>");
  TEST_RESET();
  TEST_NEXT(IsLayoutBlockFlow, kContain, NotInvalidate);
  TEST_NEXT(IsLegacyBlockFlow, kContain, NotInvalidate);
  TEST_NEXT("foo", kStart, ShouldInvalidate);
  TEST_NEXT(IsLegacyBlockFlow, kContain, NotInvalidate);
  TEST_NEXT("bar", kEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_EQ(1u, Selection().LayoutSelectionStart().value());
}

// TODO(editing-dev): Once LayoutNG supports editing, we should change this
// test to use LayoutNG tree.
TEST_F(NGLayoutSelectionTest, MixedBlockFlowsDecendant) {
  SetSelectionAndUpdateLayoutSelection(
      "<div contenteditable=false>f^oo"
      "<div contenteditable>ba|r</div></div>");
  TEST_RESET();
  TEST_NEXT(IsLayoutBlockFlow, kContain, NotInvalidate);
  TEST_NEXT(IsLayoutBlockFlow, kContain, NotInvalidate);
  TEST_NEXT("foo", kStart, ShouldInvalidate);
  TEST_NEXT(IsLegacyBlockFlow, kContain, NotInvalidate);
  TEST_NEXT("bar", kEnd, ShouldInvalidate);
  TEST_CHECK();
  EXPECT_EQ(1u, Selection().LayoutSelectionStart().value());
  EXPECT_EQ(2u, Selection().LayoutSelectionEnd().value());
}

TEST_F(NGLayoutSelectionTest, LineBreakBasic) {
  LoadAhem();
  EXPECT_FALSE(IsFirstTextLineBreak("<div>f^oo<br>ba|r</div>"));
  EXPECT_FALSE(IsFirstTextLineBreak("<div>^foo<br><br>|</div>"));
  EXPECT_TRUE(IsFirstTextLineBreak(
      "<div style='font: Ahem; width: 2em'>f^oo ba|r</div>"));
  EXPECT_TRUE(IsFirstTextLineBreak("<div>f^oo</div><div>b|ar</div>"));
  EXPECT_FALSE(IsFirstTextLineBreak("<div>f^oo |</div>"));
  EXPECT_FALSE(IsFirstTextLineBreak("<div>f^oo <!--|--></div>"));
  EXPECT_FALSE(IsFirstTextLineBreak("<div>f^oo </div>|"));
  EXPECT_FALSE(IsFirstTextLineBreak("<div>f^oo|</div>"));
  EXPECT_FALSE(IsFirstTextLineBreak("<div>f^oo<!--|--></div>"));
  EXPECT_FALSE(IsFirstTextLineBreak("<div>f^oo</div>|"));
  // TODO(yoichio): Fix the test. See LayoutSelection::IsLineBreak.
  // EXPECT_FALSE(IsFirstTextLineBreak(
  //    "<div style='display:inline-block'>f^oo</div>bar|"));
}

TEST_F(NGLayoutSelectionTest, LineBreakInlineBlock) {
  LoadAhem();
  EXPECT_FALSE(
      IsFirstTextLineBreak("<div style='display:inline-block'>^x</div>y|"));
}

TEST_F(NGLayoutSelectionTest, LineBreakImage) {
  SetSelectionAndUpdateLayoutSelection(
      "<div>^<img id=img1 width=10px height=10px>foo<br>"
      "bar<img id=img2 width=10px height=10px>|</div>");
  Node* const foo =
      GetDocument().body()->firstChild()->firstChild()->nextSibling();
  EXPECT_EQ(SelectSoftLineBreak::kNotSelected,
            ComputeLayoutSelectionStatus(*foo).line_break);
  Node* const bar = foo->nextSibling()->nextSibling();
  EXPECT_EQ(SelectSoftLineBreak::kNotSelected,
            ComputeLayoutSelectionStatus(*bar).line_break);
}

TEST_F(NGLayoutSelectionTest, BRStatus) {
  const SelectionInDOMTree& selection =
      SetSelectionTextToBody("<div>foo<!--^--><br><!--|-->bar</div>");
  Selection().SetSelectionAndEndTyping(selection);
  Selection().CommitAppearanceIfNeeded();
  LayoutObject* const layout_br =
      GetDocument().QuerySelector("br")->GetLayoutObject();
  CHECK(layout_br->IsBR());
  EXPECT_EQ(
      LayoutSelectionStatus(3u, 4u, SelectSoftLineBreak::kNotSelected),
      Selection().ComputeLayoutSelectionStatus(GetNGPaintFragment(layout_br)));
}

}  // namespace blink
