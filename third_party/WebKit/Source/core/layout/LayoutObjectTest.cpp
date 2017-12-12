// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutObject.h"

#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/frame/LocalFrameView.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/LayoutView.h"
#include "platform/json/JSONValues.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using ::testing::Return;

class LayoutObjectTest : public RenderingTest {
 public:
  LayoutObjectTest() : RenderingTest(EmptyLocalFrameClient::Create()) {}

 protected:
  template <bool should_have_wrapper>
  void ExpectAnonymousInlineWrapperFor(Node*);
};

template <bool should_have_wrapper>
void LayoutObjectTest::ExpectAnonymousInlineWrapperFor(Node* node) {
  ASSERT_TRUE(node);
  EXPECT_TRUE(node->IsTextNode());
  LayoutObject* text_layout = node->GetLayoutObject();
  ASSERT_TRUE(text_layout);
  LayoutObject* text_parent = text_layout->Parent();
  ASSERT_TRUE(text_parent);
  if (should_have_wrapper) {
    EXPECT_TRUE(text_parent->IsAnonymous());
    EXPECT_TRUE(text_parent->IsInline());
  } else {
    EXPECT_FALSE(text_parent->IsAnonymous());
  }
}

TEST_F(LayoutObjectTest, LayoutDecoratedNameCalledWithPositionedObject) {
  SetBodyInnerHTML("<div id='div' style='position: fixed'>test</div>");
  Element* div = GetDocument().getElementById(AtomicString("div"));
  DCHECK(div);
  LayoutObject* obj = div->GetLayoutObject();
  DCHECK(obj);
  EXPECT_STREQ("LayoutBlockFlow (positioned)",
               obj->DecoratedName().Ascii().data());
}

// Some display checks.
TEST_F(LayoutObjectTest, DisplayNoneCreateObject) {
  SetBodyInnerHTML("<div style='display:none'></div>");
  EXPECT_EQ(nullptr, GetDocument().body()->firstChild()->GetLayoutObject());
}

TEST_F(LayoutObjectTest, DisplayBlockCreateObject) {
  SetBodyInnerHTML("<foo style='display:block'></foo>");
  LayoutObject* layout_object =
      GetDocument().body()->firstChild()->GetLayoutObject();
  EXPECT_NE(nullptr, layout_object);
  EXPECT_TRUE(layout_object->IsLayoutBlockFlow());
  EXPECT_FALSE(layout_object->IsInline());
}

TEST_F(LayoutObjectTest, DisplayInlineBlockCreateObject) {
  SetBodyInnerHTML("<foo style='display:inline-block'></foo>");
  LayoutObject* layout_object =
      GetDocument().body()->firstChild()->GetLayoutObject();
  EXPECT_NE(nullptr, layout_object);
  EXPECT_TRUE(layout_object->IsLayoutBlockFlow());
  EXPECT_TRUE(layout_object->IsInline());
}

// Containing block test.
TEST_F(LayoutObjectTest, ContainingBlockLayoutViewShouldBeNull) {
  EXPECT_EQ(nullptr, GetLayoutView().ContainingBlock());
}

TEST_F(LayoutObjectTest, ContainingBlockBodyShouldBeDocumentElement) {
  EXPECT_EQ(GetDocument().body()->GetLayoutObject()->ContainingBlock(),
            GetDocument().documentElement()->GetLayoutObject());
}

TEST_F(LayoutObjectTest, ContainingBlockDocumentElementShouldBeLayoutView) {
  EXPECT_EQ(
      GetDocument().documentElement()->GetLayoutObject()->ContainingBlock(),
      GetLayoutView());
}

TEST_F(LayoutObjectTest, ContainingBlockStaticLayoutObjectShouldBeParent) {
  SetBodyInnerHTML("<foo style='position:static'></foo>");
  LayoutObject* body_layout_object = GetDocument().body()->GetLayoutObject();
  LayoutObject* layout_object = body_layout_object->SlowFirstChild();
  EXPECT_EQ(layout_object->ContainingBlock(), body_layout_object);
}

TEST_F(LayoutObjectTest,
       ContainingBlockAbsoluteLayoutObjectShouldBeLayoutView) {
  SetBodyInnerHTML("<foo style='position:absolute'></foo>");
  LayoutObject* layout_object =
      GetDocument().body()->GetLayoutObject()->SlowFirstChild();
  EXPECT_EQ(layout_object->ContainingBlock(), GetLayoutView());
}

TEST_F(
    LayoutObjectTest,
    ContainingBlockAbsoluteLayoutObjectShouldBeNonStaticallyPositionedBlockAncestor) {
  SetBodyInnerHTML(
      "<div style='position:relative'><bar "
      "style='position:absolute'></bar></div>");
  LayoutObject* containing_blocklayout_object =
      GetDocument().body()->GetLayoutObject()->SlowFirstChild();
  LayoutObject* layout_object = containing_blocklayout_object->SlowFirstChild();
  EXPECT_EQ(layout_object->ContainingBlock(), containing_blocklayout_object);
}

TEST_F(
    LayoutObjectTest,
    ContainingBlockAbsoluteLayoutObjectShouldNotBeNonStaticallyPositionedInlineAncestor) {
  SetBodyInnerHTML(
      "<span style='position:relative'><bar "
      "style='position:absolute'></bar></span>");
  LayoutObject* body_layout_object = GetDocument().body()->GetLayoutObject();
  LayoutObject* layout_object =
      body_layout_object->SlowFirstChild()->SlowFirstChild();

  // Sanity check: Make sure we don't generate anonymous objects.
  EXPECT_EQ(nullptr, body_layout_object->SlowFirstChild()->NextSibling());
  EXPECT_EQ(nullptr, layout_object->SlowFirstChild());
  EXPECT_EQ(nullptr, layout_object->NextSibling());

  EXPECT_EQ(layout_object->ContainingBlock(), body_layout_object);
}

TEST_F(LayoutObjectTest, PaintingLayerOfOverflowClipLayerUnderColumnSpanAll) {
  SetBodyInnerHTML(R"HTML(
    <div id='columns' style='columns: 3'>
      <div style='column-span: all'>
        <div id='overflow-clip-layer' style='height: 100px; overflow:
    hidden'></div>
      </div>
    </div>
  )HTML");

  LayoutObject* overflow_clip_object =
      GetLayoutObjectByElementId("overflow-clip-layer");
  LayoutBlock* columns = ToLayoutBlock(GetLayoutObjectByElementId("columns"));
  EXPECT_EQ(columns->Layer(), overflow_clip_object->PaintingLayer());
}

TEST_F(LayoutObjectTest, FloatUnderBlock) {
  SetBodyInnerHTML(R"HTML(
    <div id='layered-div' style='position: absolute'>
      <div id='container'>
        <div id='floating' style='float: left'>FLOAT</div>
      </div>
    </div>
  )HTML");

  LayoutBoxModelObject* layered_div =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("layered-div"));
  LayoutBoxModelObject* container =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("container"));
  LayoutObject* floating = GetLayoutObjectByElementId("floating");

  EXPECT_EQ(layered_div->Layer(), layered_div->PaintingLayer());
  EXPECT_EQ(layered_div->Layer(), floating->PaintingLayer());
  EXPECT_EQ(container, floating->Container());
  EXPECT_EQ(container, floating->ContainingBlock());
}

TEST_F(LayoutObjectTest, FloatUnderInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='layered-div' style='position: absolute'>
      <div id='container'>
        <span id='layered-span' style='position: relative'>
          <div id='floating' style='float: left'>FLOAT</div>
        </span>
      </div>
    </div>
  )HTML");

  LayoutBoxModelObject* layered_div =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("layered-div"));
  LayoutBoxModelObject* container =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("container"));
  LayoutBoxModelObject* layered_span =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("layered-span"));
  LayoutObject* floating = GetLayoutObjectByElementId("floating");

  EXPECT_EQ(layered_div->Layer(), layered_div->PaintingLayer());
  EXPECT_EQ(layered_span->Layer(), layered_span->PaintingLayer());
  EXPECT_EQ(layered_div->Layer(), floating->PaintingLayer());
  EXPECT_EQ(container, floating->Container());
  EXPECT_EQ(container, floating->ContainingBlock());

  LayoutObject::AncestorSkipInfo skip_info(layered_span);
  EXPECT_EQ(container, floating->Container(&skip_info));
  EXPECT_TRUE(skip_info.AncestorSkipped());

  skip_info = LayoutObject::AncestorSkipInfo(container);
  EXPECT_EQ(container, floating->Container(&skip_info));
  EXPECT_FALSE(skip_info.AncestorSkipped());
}

TEST_F(LayoutObjectTest, MutableForPaintingClearPaintFlags) {
  LayoutObject* object = GetDocument().body()->GetLayoutObject();
  object->SetShouldDoFullPaintInvalidation();
  EXPECT_TRUE(object->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(object->NeedsPaintOffsetAndVisualRectUpdate());
  object->SetMayNeedPaintInvalidation();
  EXPECT_TRUE(object->MayNeedPaintInvalidation());
  object->SetMayNeedPaintInvalidationSubtree();
  EXPECT_TRUE(object->MayNeedPaintInvalidationSubtree());
  object->SetMayNeedPaintInvalidationAnimatedBackgroundImage();
  EXPECT_TRUE(object->MayNeedPaintInvalidationAnimatedBackgroundImage());
  object->SetShouldInvalidateSelection();
  EXPECT_TRUE(object->ShouldInvalidateSelection());
  object->SetBackgroundChangedSinceLastPaintInvalidation();
  EXPECT_TRUE(object->BackgroundChangedSinceLastPaintInvalidation());
  object->SetNeedsPaintPropertyUpdate();
  EXPECT_TRUE(object->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(object->Parent()->DescendantNeedsPaintPropertyUpdate());
  object->bitfields_.SetDescendantNeedsPaintPropertyUpdate(true);
  EXPECT_TRUE(object->DescendantNeedsPaintPropertyUpdate());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInPrePaint);
  object->GetMutableForPainting().ClearPaintFlags();

  EXPECT_FALSE(object->ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(object->MayNeedPaintInvalidation());
  EXPECT_FALSE(object->MayNeedPaintInvalidationSubtree());
  EXPECT_FALSE(object->MayNeedPaintInvalidationAnimatedBackgroundImage());
  EXPECT_FALSE(object->ShouldInvalidateSelection());
  EXPECT_FALSE(object->BackgroundChangedSinceLastPaintInvalidation());
  EXPECT_FALSE(object->NeedsPaintPropertyUpdate());
  EXPECT_FALSE(object->DescendantNeedsPaintPropertyUpdate());
}

TEST_F(LayoutObjectTest, SubtreeNeedsPaintPropertyUpdate) {
  LayoutObject* object = GetDocument().body()->GetLayoutObject();
  object->SetSubtreeNeedsPaintPropertyUpdate();
  EXPECT_TRUE(object->SubtreeNeedsPaintPropertyUpdate());
  EXPECT_TRUE(object->NeedsPaintPropertyUpdate());
  EXPECT_TRUE(object->Parent()->DescendantNeedsPaintPropertyUpdate());

  GetDocument().Lifecycle().AdvanceTo(DocumentLifecycle::kInPrePaint);
  object->GetMutableForPainting().ClearPaintFlags();

  EXPECT_FALSE(object->SubtreeNeedsPaintPropertyUpdate());
  EXPECT_FALSE(object->NeedsPaintPropertyUpdate());
}

TEST_F(LayoutObjectTest, NeedsPaintOffsetAndVisualRectUpdate) {
  LayoutObject* object = GetDocument().body()->GetLayoutObject();
  LayoutObject* parent = object->Parent();

  object->SetShouldDoFullPaintInvalidation();
  EXPECT_TRUE(object->ShouldDoFullPaintInvalidation());
  EXPECT_TRUE(object->NeedsPaintOffsetAndVisualRectUpdate());
  EXPECT_TRUE(parent->MayNeedPaintInvalidation());
  EXPECT_TRUE(parent->NeedsPaintOffsetAndVisualRectUpdate());
  object->ClearPaintInvalidationFlags();
  EXPECT_FALSE(object->ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(object->NeedsPaintOffsetAndVisualRectUpdate());
  parent->ClearPaintInvalidationFlags();
  EXPECT_FALSE(parent->MayNeedPaintInvalidation());
  EXPECT_FALSE(parent->NeedsPaintOffsetAndVisualRectUpdate());

  object->SetMayNeedPaintInvalidation();
  EXPECT_TRUE(object->MayNeedPaintInvalidation());
  EXPECT_TRUE(object->NeedsPaintOffsetAndVisualRectUpdate());
  EXPECT_TRUE(parent->MayNeedPaintInvalidation());
  EXPECT_TRUE(parent->NeedsPaintOffsetAndVisualRectUpdate());
  object->ClearPaintInvalidationFlags();
  EXPECT_FALSE(object->MayNeedPaintInvalidation());
  EXPECT_FALSE(object->NeedsPaintOffsetAndVisualRectUpdate());
  parent->ClearPaintInvalidationFlags();
  EXPECT_FALSE(parent->MayNeedPaintInvalidation());
  EXPECT_FALSE(parent->NeedsPaintOffsetAndVisualRectUpdate());

  object->SetShouldDoFullPaintInvalidationWithoutGeometryChange();
  EXPECT_TRUE(object->ShouldDoFullPaintInvalidation());
  EXPECT_FALSE(object->NeedsPaintOffsetAndVisualRectUpdate());
  EXPECT_TRUE(parent->MayNeedPaintInvalidation());
  EXPECT_FALSE(parent->NeedsPaintOffsetAndVisualRectUpdate());
  object->SetMayNeedPaintInvalidation();
  EXPECT_TRUE(object->NeedsPaintOffsetAndVisualRectUpdate());
  EXPECT_TRUE(parent->NeedsPaintOffsetAndVisualRectUpdate());
  object->ClearPaintInvalidationFlags();
  EXPECT_FALSE(object->MayNeedPaintInvalidation());
  EXPECT_FALSE(object->NeedsPaintOffsetAndVisualRectUpdate());
  parent->ClearPaintInvalidationFlags();
  EXPECT_FALSE(parent->MayNeedPaintInvalidation());
  EXPECT_FALSE(parent->NeedsPaintOffsetAndVisualRectUpdate());

  object->SetMayNeedPaintInvalidationWithoutGeometryChange();
  EXPECT_TRUE(object->MayNeedPaintInvalidation());
  EXPECT_FALSE(object->NeedsPaintOffsetAndVisualRectUpdate());
  EXPECT_TRUE(parent->MayNeedPaintInvalidation());
  EXPECT_FALSE(parent->NeedsPaintOffsetAndVisualRectUpdate());
  object->SetMayNeedPaintInvalidation();
  EXPECT_TRUE(object->NeedsPaintOffsetAndVisualRectUpdate());
  EXPECT_TRUE(parent->NeedsPaintOffsetAndVisualRectUpdate());
  object->ClearPaintInvalidationFlags();
  EXPECT_FALSE(object->MayNeedPaintInvalidation());
  EXPECT_FALSE(object->NeedsPaintOffsetAndVisualRectUpdate());
  parent->ClearPaintInvalidationFlags();
  EXPECT_FALSE(parent->MayNeedPaintInvalidation());
  EXPECT_FALSE(parent->NeedsPaintOffsetAndVisualRectUpdate());
}

TEST_F(LayoutObjectTest, AssociatedLayoutObjectOfFirstLetterPunctuations) {
  const char* body_content =
      "<style>p:first-letter {color:red;}</style><p id=sample>(a)bc</p>";
  SetBodyInnerHTML(body_content);

  Node* sample = GetDocument().getElementById("sample");
  Node* text = sample->firstChild();

  const LayoutTextFragment* layout_object0 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 0));
  EXPECT_FALSE(layout_object0->IsRemainingTextLayoutObject());

  const LayoutTextFragment* layout_object1 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 1));
  EXPECT_EQ(layout_object0, layout_object1)
      << "A character 'a' should be part of first letter.";

  const LayoutTextFragment* layout_object2 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 2));
  EXPECT_EQ(layout_object0, layout_object2)
      << "close parenthesis should be part of first letter.";

  const LayoutTextFragment* layout_object3 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 3));
  EXPECT_TRUE(layout_object3->IsRemainingTextLayoutObject());
}

TEST_F(LayoutObjectTest, AssociatedLayoutObjectOfFirstLetterSplit) {
  V8TestingScope scope;

  const char* body_content =
      "<style>p:first-letter {color:red;}</style><p id=sample>abc</p>";
  SetBodyInnerHTML(body_content);

  Node* sample = GetDocument().getElementById("sample");
  Node* first_letter = sample->firstChild();
  // Split "abc" into "a" "bc"
  ToText(first_letter)->splitText(1, ASSERT_NO_EXCEPTION);
  GetDocument().View()->UpdateAllLifecyclePhases();

  const LayoutTextFragment* layout_object0 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*first_letter, 0));
  EXPECT_FALSE(layout_object0->IsRemainingTextLayoutObject());

  const LayoutTextFragment* layout_object1 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*first_letter, 1));
  EXPECT_EQ(layout_object0, layout_object1);
}

TEST_F(LayoutObjectTest,
       AssociatedLayoutObjectOfFirstLetterWithTrailingWhitespace) {
  const char* body_content =
      "<style>div:first-letter {color:red;}</style><div id=sample>a\n "
      "<div></div></div>";
  SetBodyInnerHTML(body_content);

  Node* sample = GetDocument().getElementById("sample");
  Node* text = sample->firstChild();

  const LayoutTextFragment* layout_object0 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 0));
  EXPECT_FALSE(layout_object0->IsRemainingTextLayoutObject());

  const LayoutTextFragment* layout_object1 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 1));
  EXPECT_TRUE(layout_object1->IsRemainingTextLayoutObject());

  const LayoutTextFragment* layout_object2 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 2));
  EXPECT_EQ(layout_object1, layout_object2);
}

TEST_F(LayoutObjectTest, VisualRect) {
  class MockLayoutObject : public LayoutObject {
   public:
    MockLayoutObject() : LayoutObject(nullptr) {}
    MOCK_CONST_METHOD0(VisualRectRespectsVisibility, bool());

   private:
    LayoutRect LocalVisualRectIgnoringVisibility() const {
      return LayoutRect(10, 10, 20, 20);
    }
    const char* GetName() const final { return "MockLayoutObject"; }
    void UpdateLayout() final {}
    FloatRect LocalBoundingBoxRectForAccessibility() const final {
      return FloatRect();
    }
  };

  MockLayoutObject mock_object;
  auto style = ComputedStyle::Create();
  mock_object.SetStyle(style.get());
  EXPECT_EQ(LayoutRect(10, 10, 20, 20), mock_object.LocalVisualRect());
  EXPECT_EQ(LayoutRect(10, 10, 20, 20), mock_object.LocalVisualRect());

  style->SetVisibility(EVisibility::kHidden);
  EXPECT_CALL(mock_object, VisualRectRespectsVisibility())
      .WillOnce(Return(true));
  EXPECT_EQ(LayoutRect(), mock_object.LocalVisualRect());
  EXPECT_CALL(mock_object, VisualRectRespectsVisibility())
      .WillOnce(Return(false));
  EXPECT_EQ(LayoutRect(10, 10, 20, 20), mock_object.LocalVisualRect());
}

TEST_F(LayoutObjectTest, DisplayContentsInlineWrapper) {
  SetBodyInnerHTML("<div id='div' style='display:contents;color:pink'>A</div>");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  Node* text = div->firstChild();
  ASSERT_TRUE(text);
  ExpectAnonymousInlineWrapperFor<true>(text);
}

TEST_F(LayoutObjectTest, DisplayContentsNoInlineWrapper) {
  SetBodyInnerHTML("<div id='div' style='display:contents'>A</div>");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  Node* text = div->firstChild();
  ASSERT_TRUE(text);
  ExpectAnonymousInlineWrapperFor<false>(text);
}

TEST_F(LayoutObjectTest, DisplayContentsAddInlineWrapper) {
  SetBodyInnerHTML("<div id='div' style='display:contents'>A</div>");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  Node* text = div->firstChild();
  ASSERT_TRUE(text);
  ExpectAnonymousInlineWrapperFor<false>(text);

  div->SetInlineStyleProperty(CSSPropertyColor, "pink");
  GetDocument().View()->UpdateAllLifecyclePhases();
  ExpectAnonymousInlineWrapperFor<true>(text);
}

TEST_F(LayoutObjectTest, DisplayContentsRemoveInlineWrapper) {
  SetBodyInnerHTML("<div id='div' style='display:contents;color:pink'>A</div>");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  Node* text = div->firstChild();
  ASSERT_TRUE(text);
  ExpectAnonymousInlineWrapperFor<true>(text);

  div->RemoveInlineStyleProperty(CSSPropertyColor);
  GetDocument().View()->UpdateAllLifecyclePhases();
  ExpectAnonymousInlineWrapperFor<false>(text);
}

TEST_F(LayoutObjectTest, DisplayContentsWrapperPerTextNode) {
  // This test checks the current implementation; that text node siblings do not
  // share inline wrappers. Doing so requires code to handle all situations
  // where text nodes are no longer layout tree siblings by splitting wrappers,
  // and merge wrappers when text nodes become layout tree siblings.
  SetBodyInnerHTML(
      "<div id='div' style='display:contents;color:pink'>A<!-- -->B</div>");
  Element* div = GetDocument().getElementById("div");
  ASSERT_TRUE(div);
  Node* text1 = div->firstChild();
  ASSERT_TRUE(text1);
  Node* text2 = div->lastChild();
  ASSERT_TRUE(text2);
  EXPECT_NE(text1, text2);

  ExpectAnonymousInlineWrapperFor<true>(text1);
  ExpectAnonymousInlineWrapperFor<true>(text2);

  EXPECT_NE(text1->GetLayoutObject()->Parent(),
            text2->GetLayoutObject()->Parent());
}

TEST_F(LayoutObjectTest, DisplayContentsWrapperInTable) {
  SetBodyInnerHTML(R"HTML(
    <div id='table' style='display:table'>
      <div id='none' style='display:none'></div>
      <div id='contents' style='display:contents;color:green'>Green</div>
    </div>
  )HTML");

  Element* none = GetDocument().getElementById("none");
  Element* contents = GetDocument().getElementById("contents");

  ExpectAnonymousInlineWrapperFor<true>(contents->firstChild());

  none->SetInlineStyleProperty(CSSPropertyDisplay, "inline");
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(none->GetLayoutObject());
  LayoutObject* inline_parent = none->GetLayoutObject()->Parent();
  ASSERT_TRUE(inline_parent);
  LayoutObject* wrapper_parent =
      contents->firstChild()->GetLayoutObject()->Parent()->Parent();
  ASSERT_TRUE(wrapper_parent);
  EXPECT_EQ(wrapper_parent, inline_parent);
  EXPECT_TRUE(inline_parent->IsTableCell());
  EXPECT_TRUE(inline_parent->IsAnonymous());
}

TEST_F(LayoutObjectTest, DisplayContentsWrapperInTableSection) {
  SetBodyInnerHTML(R"HTML(
    <div id='section' style='display:table-row-group'>
      <div id='none' style='display:none'></div>
      <div id='contents' style='display:contents;color:green'>Green</div>
    </div>
  )HTML");

  Element* none = GetDocument().getElementById("none");
  Element* contents = GetDocument().getElementById("contents");

  ExpectAnonymousInlineWrapperFor<true>(contents->firstChild());

  none->SetInlineStyleProperty(CSSPropertyDisplay, "inline");
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(none->GetLayoutObject());
  LayoutObject* inline_parent = none->GetLayoutObject()->Parent();
  ASSERT_TRUE(inline_parent);
  LayoutObject* wrapper_parent =
      contents->firstChild()->GetLayoutObject()->Parent()->Parent();
  ASSERT_TRUE(wrapper_parent);
  EXPECT_EQ(wrapper_parent, inline_parent);
  EXPECT_TRUE(inline_parent->IsTableCell());
  EXPECT_TRUE(inline_parent->IsAnonymous());
}

TEST_F(LayoutObjectTest, DisplayContentsWrapperInTableRow) {
  SetBodyInnerHTML(R"HTML(
    <div id='row' style='display:table-row'>
      <div id='none' style='display:none'></div>
      <div id='contents' style='display:contents;color:green'>Green</div>
    </div>
  )HTML");

  Element* none = GetDocument().getElementById("none");
  Element* contents = GetDocument().getElementById("contents");

  ExpectAnonymousInlineWrapperFor<true>(contents->firstChild());

  none->SetInlineStyleProperty(CSSPropertyDisplay, "inline");
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(none->GetLayoutObject());
  LayoutObject* inline_parent = none->GetLayoutObject()->Parent();
  ASSERT_TRUE(inline_parent);
  LayoutObject* wrapper_parent =
      contents->firstChild()->GetLayoutObject()->Parent()->Parent();
  ASSERT_TRUE(wrapper_parent);
  EXPECT_EQ(wrapper_parent, inline_parent);
  EXPECT_TRUE(inline_parent->IsTableCell());
  EXPECT_TRUE(inline_parent->IsAnonymous());
}

TEST_F(LayoutObjectTest, DisplayContentsWrapperInTableCell) {
  SetBodyInnerHTML(R"HTML(
    <div id='cell' style='display:table-cell'>
      <div id='none' style='display:none'></div>
      <div id='contents' style='display:contents;color:green'>Green</div>
    </div>
  )HTML");

  Element* cell = GetDocument().getElementById("cell");
  Element* none = GetDocument().getElementById("none");
  Element* contents = GetDocument().getElementById("contents");

  ExpectAnonymousInlineWrapperFor<true>(contents->firstChild());

  none->SetInlineStyleProperty(CSSPropertyDisplay, "inline");
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(none->GetLayoutObject());
  EXPECT_EQ(cell->GetLayoutObject(), none->GetLayoutObject()->Parent());
}

}  // namespace blink
